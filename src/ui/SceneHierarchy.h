#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @file SceneHierarchy.h
 * @brief Scene hierarchy model: named entities and parent/child links (issue #59).
 *
 * Milestone 9 (In-Engine Editor & Tooling). The headless core behind the ImGui
 * scene hierarchy panel: track entities by id with a display name and parent/child
 * relationships, and support the panel's edits - rename, reparent (with cycle
 * prevention), and delete (recursive, or promoting children to the parent). The
 * engine keeps this in sync with the live scene (add on create, remove on destroy)
 * and uses the node ids - the same packed entity ids the inspector/picking use - to
 * drive selection into the entity inspector (#56).
 *
 * Ids are opaque; the hierarchy stores only names and links, not component data, so
 * it stays decoupled from the ECS and is unit-testable on its own. Header-only,
 * std only.
 */
namespace IKore {

class SceneHierarchy {
public:
    using NodeId = std::uint64_t;
    /// Sentinel meaning "no node" / the virtual root that top-level entities hang off.
    static constexpr NodeId kNoNode = ~NodeId{0};

    /**
     * @brief Add an entity with a display name under @p parent (kNoNode = top level).
     * @return false if the id is the sentinel, already exists, or @p parent is unknown.
     */
    bool add(NodeId id, std::string name, NodeId parent = kNoNode) {
        if (id == kNoNode || m_nodes.count(id) != 0) return false;
        if (parent != kNoNode && !exists(parent)) return false;
        Node node;
        node.name = std::move(name);
        node.parent = parent;
        m_nodes.emplace(id, std::move(node));
        childListOf(parent).push_back(id);
        return true;
    }

    bool exists(NodeId id) const { return m_nodes.count(id) != 0; }
    std::size_t size() const { return m_nodes.size(); }
    void clear() {
        m_nodes.clear();
        m_roots.clear();
    }

    /**
     * @brief Remove an entity.
     * @param recursive true: delete the whole subtree; false: promote children to
     *                  this node's parent before removing it.
     * @return false if the id is unknown.
     */
    bool remove(NodeId id, bool recursive = true) {
        auto it = m_nodes.find(id);
        if (it == m_nodes.end()) return false;
        const NodeId parent = it->second.parent;

        if (recursive) {
            for (const NodeId victim : subtree(id)) m_nodes.erase(victim);
        } else {
            const std::vector<NodeId> kids = it->second.children; // copy before edits
            for (const NodeId child : kids) {
                m_nodes[child].parent = parent;
                childListOf(parent).push_back(child);
            }
            m_nodes.erase(id);
        }
        removeChildLink(parent, id);
        return true;
    }

    bool rename(NodeId id, std::string name) {
        auto it = m_nodes.find(id);
        if (it == m_nodes.end()) return false;
        it->second.name = std::move(name);
        return true;
    }

    const std::string& name(NodeId id) const {
        static const std::string empty;
        auto it = m_nodes.find(id);
        return it == m_nodes.end() ? empty : it->second.name;
    }

    /**
     * @brief Move @p id under @p newParent (kNoNode = top level).
     * @return false if either id is unknown, they are the same, or the move would
     *         create a cycle (newParent is a descendant of id). A no-op move to the
     *         current parent succeeds.
     */
    bool reparent(NodeId id, NodeId newParent) {
        auto it = m_nodes.find(id);
        if (it == m_nodes.end()) return false;
        if (newParent != kNoNode && !exists(newParent)) return false;
        if (newParent == id || isAncestor(id, newParent)) return false;

        const NodeId old = it->second.parent;
        if (old == newParent) return true;
        removeChildLink(old, id);
        it->second.parent = newParent;
        childListOf(newParent).push_back(id);
        return true;
    }

    NodeId parent(NodeId id) const {
        auto it = m_nodes.find(id);
        return it == m_nodes.end() ? kNoNode : it->second.parent;
    }

    /// Children of @p id (top-level entities for kNoNode); empty if @p id is unknown.
    const std::vector<NodeId>& children(NodeId id) const {
        if (id == kNoNode) return m_roots;
        auto it = m_nodes.find(id);
        return it == m_nodes.end() ? emptyList() : it->second.children;
    }

    const std::vector<NodeId>& roots() const { return m_roots; }

    /// True if @p ancestor is a (strict) ancestor of @p of.
    bool isAncestor(NodeId ancestor, NodeId of) const {
        NodeId cur = parent(of);
        while (cur != kNoNode) {
            if (cur == ancestor) return true;
            cur = parent(cur);
        }
        return false;
    }

    /// All ids in the subtree rooted at @p id (including @p id).
    std::vector<NodeId> subtree(NodeId id) const {
        std::vector<NodeId> out;
        if (!exists(id)) return out;
        std::vector<NodeId> stack{id};
        while (!stack.empty()) {
            const NodeId cur = stack.back();
            stack.pop_back();
            out.push_back(cur);
            auto it = m_nodes.find(cur);
            if (it != m_nodes.end()) {
                for (const NodeId child : it->second.children) stack.push_back(child);
            }
        }
        return out;
    }

    /// Depth-first preorder listing of (id, depth), for flat rendering.
    std::vector<std::pair<NodeId, int>> flatten() const {
        std::vector<std::pair<NodeId, int>> out;
        for (const NodeId root : m_roots) flattenInto(root, 0, out);
        return out;
    }

private:
    struct Node {
        std::string name;
        NodeId parent{kNoNode};
        std::vector<NodeId> children;
    };

    static const std::vector<NodeId>& emptyList() {
        static const std::vector<NodeId> empty;
        return empty;
    }

    std::vector<NodeId>& childListOf(NodeId parent) {
        if (parent == kNoNode) return m_roots;
        return m_nodes[parent].children; // callers guarantee parent exists
    }

    void removeChildLink(NodeId parent, NodeId child) {
        std::vector<NodeId>& list = childListOf(parent);
        list.erase(std::remove(list.begin(), list.end(), child), list.end());
    }

    void flattenInto(NodeId id, int depth, std::vector<std::pair<NodeId, int>>& out) const {
        out.emplace_back(id, depth);
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            for (const NodeId child : it->second.children) flattenInto(child, depth + 1, out);
        }
    }

    std::unordered_map<NodeId, Node> m_nodes;
    std::vector<NodeId> m_roots;
};

} // namespace IKore
