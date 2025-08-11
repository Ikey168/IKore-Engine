#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <vector>
#include <limits>

namespace IKore {

    /**
     * @brief Represents a 3D axis-aligned bounding box
     * Used for frustum culling calculations
     */
    struct BoundingBox {
        glm::vec3 min;
        glm::vec3 max;
        
        BoundingBox() : min(0.0f), max(0.0f) {}
        
        BoundingBox(const glm::vec3& minPoint, const glm::vec3& maxPoint)
            : min(minPoint), max(maxPoint) {}
        
        /**
         * @brief Create bounding box from center point and extents
         * @param center Center point of the box
         * @param extents Half-size extents from center to edges
         */
        static BoundingBox fromCenterExtents(const glm::vec3& center, const glm::vec3& extents) {
            return BoundingBox(center - extents, center + extents);
        }
        
        /**
         * @brief Get center point of the bounding box
         */
        glm::vec3 getCenter() const {
            return (min + max) * 0.5f;
        }
        
        /**
         * @brief Get extents (half-size) of the bounding box
         */
        glm::vec3 getExtents() const {
            return (max - min) * 0.5f;
        }
        
        /**
         * @brief Get size (full dimensions) of the bounding box
         */
        glm::vec3 getSize() const {
            return max - min;
        }
        
        /**
         * @brief Transform bounding box by a matrix
         * @param transform Transformation matrix to apply
         * @return New transformed bounding box
         */
        BoundingBox transform(const glm::mat4& transform) const;
        
        /**
         * @brief Expand bounding box to include another point
         * @param point Point to include in the bounds
         */
        void expand(const glm::vec3& point);
        
        /**
         * @brief Expand bounding box to include another bounding box
         * @param other Bounding box to include
         */
        void expand(const BoundingBox& other);
        
        /**
         * @brief Check if point is inside the bounding box
         * @param point Point to test
         * @return True if point is inside
         */
        bool contains(const glm::vec3& point) const;
        
        /**
         * @brief Check if this bounding box intersects with another
         * @param other Other bounding box to test against
         * @return True if boxes intersect
         */
        bool intersects(const BoundingBox& other) const;
    };

    /**
     * @brief Represents a plane in 3D space using the equation ax + by + cz + d = 0
     */
    struct Plane {
        glm::vec3 normal;   // Normal vector (a, b, c)
        float distance;     // Distance from origin (d)
        
        Plane() : normal(0.0f, 1.0f, 0.0f), distance(0.0f) {}
        
        Plane(const glm::vec3& n, float d) : normal(glm::normalize(n)), distance(d) {}
        
        Plane(const glm::vec3& point, const glm::vec3& n) 
            : normal(glm::normalize(n)), distance(-glm::dot(point, normal)) {}
        
        /**
         * @brief Create plane from three points
         * @param p1, p2, p3 Three points on the plane
         */
        static Plane fromPoints(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);
        
        /**
         * @brief Calculate signed distance from point to plane
         * @param point Point to test
         * @return Signed distance (positive = in front, negative = behind)
         */
        float distanceToPoint(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
        
        /**
         * @brief Normalize the plane equation
         */
        void normalize();
    };

    /**
     * @brief Camera frustum for culling objects outside view
     * Contains 6 planes: left, right, top, bottom, near, far
     */
    class Frustum {
    public:
        enum PlaneIndex {
            LEFT = 0,
            RIGHT = 1,
            TOP = 2,
            BOTTOM = 3,
            NEAR = 4,
            FAR = 5
        };
        
    private:
        std::array<Plane, 6> planes;
        
    public:
        Frustum() = default;
        
        /**
         * @brief Extract frustum planes from view-projection matrix
         * @param viewProjectionMatrix Combined view and projection matrix
         */
        void extractFromMatrix(const glm::mat4& viewProjectionMatrix);
        
        /**
         * @brief Extract frustum planes from separate view and projection matrices
         * @param viewMatrix Camera view matrix
         * @param projectionMatrix Camera projection matrix
         */
        void extractFromMatrices(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
        
        /**
         * @brief Test if a point is inside the frustum
         * @param point Point to test
         * @return True if point is inside frustum
         */
        bool containsPoint(const glm::vec3& point) const;
        
        /**
         * @brief Test if a sphere is inside or intersecting the frustum
         * @param center Sphere center
         * @param radius Sphere radius
         * @return True if sphere is visible (inside or intersecting)
         */
        bool containsSphere(const glm::vec3& center, float radius) const;
        
        /**
         * @brief Test if an axis-aligned bounding box is inside or intersecting the frustum
         * @param boundingBox Bounding box to test
         * @return True if bounding box is visible (inside or intersecting)
         */
        bool containsAABB(const BoundingBox& boundingBox) const;
        
        /**
         * @brief Test if a transformed bounding box is inside or intersecting the frustum
         * @param boundingBox Local space bounding box
         * @param modelMatrix Transformation matrix to world space
         * @return True if transformed bounding box is visible
         */
        bool containsTransformedAABB(const BoundingBox& boundingBox, const glm::mat4& modelMatrix) const;
        
        /**
         * @brief Get a specific frustum plane
         * @param index Plane index (LEFT, RIGHT, TOP, BOTTOM, NEAR, FAR)
         * @return Reference to the plane
         */
        const Plane& getPlane(PlaneIndex index) const {
            return planes[index];
        }
        
        /**
         * @brief Get all frustum planes
         * @return Array of all 6 planes
         */
        const std::array<Plane, 6>& getPlanes() const {
            return planes;
        }
        
        /**
         * @brief Calculate the distance from a point to the frustum
         * Returns the smallest distance to any plane (negative if inside)
         * @param point Point to test
         * @return Distance to frustum
         */
        float distanceToPoint(const glm::vec3& point) const;
        
        /**
         * @brief Check if the frustum needs updating
         * This can be optimized by checking if view/projection matrices changed
         * @param lastViewMatrix Last frame's view matrix
         * @param lastProjectionMatrix Last frame's projection matrix
         * @param currentViewMatrix Current frame's view matrix
         * @param currentProjectionMatrix Current frame's projection matrix
         * @return True if frustum should be updated
         */
        static bool needsUpdate(const glm::mat4& lastViewMatrix, const glm::mat4& lastProjectionMatrix,
                               const glm::mat4& currentViewMatrix, const glm::mat4& currentProjectionMatrix);
    };

    /**
     * @brief Frustum culling manager for efficient object culling
     * Handles multiple objects and provides statistics
     */
    class FrustumCuller {
    public:
        struct CullingStats {
            size_t totalObjects = 0;
            size_t culledObjects = 0;
            size_t visibleObjects = 0;
            float cullingRatio = 0.0f;
            
            void reset() {
                totalObjects = culledObjects = visibleObjects = 0;
                cullingRatio = 0.0f;
            }
            
            void update() {
                cullingRatio = totalObjects > 0 ? (float)culledObjects / (float)totalObjects : 0.0f;
            }
        };
        
        /**
         * @brief Renderable object interface for frustum culling
         */
        struct RenderableObject {
            BoundingBox boundingBox;
            glm::mat4 modelMatrix;
            bool isVisible = true;
            
            RenderableObject(const BoundingBox& bounds, const glm::mat4& transform)
                : boundingBox(bounds), modelMatrix(transform) {}
        };
        
    private:
        Frustum frustum;
        CullingStats stats;
        
        // Optimization: cache last frame matrices to avoid unnecessary updates
        glm::mat4 lastViewMatrix;
        glm::mat4 lastProjectionMatrix;
        bool frustumNeedsUpdate = true;
        
    public:
        FrustumCuller() = default;
        
        /**
         * @brief Update frustum with new camera matrices
         * @param viewMatrix Current camera view matrix
         * @param projectionMatrix Current camera projection matrix
         */
        void updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
        
        /**
         * @brief Perform frustum culling on a list of objects
         * @param objects List of renderable objects to cull
         */
        void cullObjects(std::vector<RenderableObject>& objects);
        
        /**
         * @brief Test single object against frustum
         * @param object Object to test
         * @return True if object is visible
         */
        bool isVisible(const RenderableObject& object) const;
        
        /**
         * @brief Test bounding box against current frustum
         * @param boundingBox Bounding box to test
         * @param modelMatrix Transformation matrix
         * @return True if visible
         */
        bool isVisible(const BoundingBox& boundingBox, const glm::mat4& modelMatrix = glm::mat4(1.0f)) const;
        
        /**
         * @brief Get current culling statistics
         * @return Reference to culling stats
         */
        const CullingStats& getStats() const { return stats; }
        
        /**
         * @brief Get current frustum
         * @return Reference to current frustum
         */
        const Frustum& getFrustum() const { return frustum; }
        
        /**
         * @brief Enable/disable frustum culling optimizations
         * @param enabled True to enable matrix change detection
         */
        void setOptimizationEnabled(bool enabled) { frustumNeedsUpdate = !enabled; }
    };

} // namespace IKore
