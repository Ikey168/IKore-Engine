#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <algorithm>
#include <mutex>

namespace IKore {

    /**
     * @brief Unique identifier type for entities
     */
    using EntityID = uint64_t;

    /**
     * @brief Invalid entity ID constant
     */
    constexpr EntityID INVALID_ENTITY_ID = 0;

    /**
     * @brief Base Entity class with unique ID
     * 
     * This is the base class for all entities in the IKore Engine.
     * Each entity has a unique ID that persists throughout its lifetime.
     */
    class Entity {
    public:
        /**
         * @brief Constructor that assigns a unique ID
         */
        Entity();

        /**
         * @brief Virtual destructor for proper inheritance
         */
        virtual ~Entity() = default;

        /**
         * @brief Copy constructor (creates new entity with new ID)
         */
        Entity(const Entity& other);

        /**
         * @brief Move constructor (transfers ID)
         */
        Entity(Entity&& other) noexcept;

        /**
         * @brief Copy assignment operator (creates new ID)
         */
        Entity& operator=(const Entity& other);

        /**
         * @brief Move assignment operator (transfers ID)
         */
        Entity& operator=(Entity&& other) noexcept;

        /**
         * @brief Get the unique ID of this entity
         * @return The entity's unique ID
         */
        EntityID getID() const { return m_id; }

        /**
         * @brief Check if this entity is valid (has a valid ID)
         * @return True if the entity has a valid ID
         */
        bool isValid() const { return m_id != INVALID_ENTITY_ID; }

        /**
         * @brief Mark this entity as invalid
         */
        void invalidate() { m_id = INVALID_ENTITY_ID; }

        /**
         * @brief Virtual update method for entity-specific logic
         * @param deltaTime Time elapsed since last update
         */
        virtual void update([[maybe_unused]] float deltaTime) {}

        /**
         * @brief Virtual initialization method
         */
        virtual void initialize() {}

        /**
         * @brief Virtual cleanup method
         */
        virtual void cleanup() {}

        /**
         * @brief Equality operator based on entity ID
         */
        bool operator==(const Entity& other) const { return m_id == other.m_id; }

        /**
         * @brief Inequality operator based on entity ID
         */
        bool operator!=(const Entity& other) const { return m_id != other.m_id; }

        /**
         * @brief Less than operator for sorting/ordering
         */
        bool operator<(const Entity& other) const { return m_id < other.m_id; }

    private:
        EntityID m_id;
        static std::atomic<EntityID> s_nextID;

        /**
         * @brief Generate the next unique entity ID
         * @return A new unique entity ID
         */
        static EntityID generateID();
    };

    /**
     * @brief Global Entity Manager for handling entity lifecycle
     * 
     * This singleton class manages all entities in the engine, providing
     * creation, deletion, and iteration capabilities.
     */
    class EntityManager {
    public:
        /**
         * @brief Get the singleton instance
         * @return Reference to the EntityManager instance
         */
        static EntityManager& getInstance();

        /**
         * @brief Create a new entity of the specified type
         * @tparam T Entity type (must inherit from Entity)
         * @tparam Args Constructor argument types
         * @param args Constructor arguments
         * @return Shared pointer to the created entity
         */
        template<typename T, typename... Args>
        std::shared_ptr<T> createEntity(Args&&... args) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            auto entity = std::make_shared<T>(std::forward<Args>(args)...);
            addEntity(entity);
            return entity;
        }

        /**
         * @brief Add an existing entity to the manager
         * @param entity Shared pointer to the entity
         */
        void addEntity(std::shared_ptr<Entity> entity);

        /**
         * @brief Remove an entity by ID
         * @param id The entity ID to remove
         * @return True if the entity was found and removed
         */
        bool removeEntity(EntityID id);

        /**
         * @brief Remove an entity by pointer
         * @param entity Shared pointer to the entity to remove
         * @return True if the entity was found and removed
         */
        bool removeEntity(std::shared_ptr<Entity> entity);

        /**
         * @brief Get an entity by ID
         * @param id The entity ID to find
         * @return Shared pointer to the entity, or nullptr if not found
         */
        std::shared_ptr<Entity> getEntity(EntityID id) const;

        /**
         * @brief Get an entity by ID with specific type
         * @tparam T The expected entity type
         * @param id The entity ID to find
         * @return Shared pointer to the entity cast to type T, or nullptr if not found or wrong type
         */
        template<typename T>
        std::shared_ptr<T> getEntity(EntityID id) const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            auto entity = getEntity(id);
            return std::dynamic_pointer_cast<T>(entity);
        }

        /**
         * @brief Get all entities
         * @return Vector of shared pointers to all entities
         */
        std::vector<std::shared_ptr<Entity>> getAllEntities() const;

        /**
         * @brief Get all entities of a specific type
         * @tparam T The entity type to filter by
         * @return Vector of shared pointers to entities of type T
         */
        template<typename T>
        std::vector<std::shared_ptr<T>> getEntitiesOfType() const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            std::vector<std::shared_ptr<T>> result;
            for (const auto& pair : m_entities) {
                if (auto typedEntity = std::dynamic_pointer_cast<T>(pair.second)) {
                    result.push_back(typedEntity);
                }
            }
            return result;
        }

        /**
         * @brief Iterate over all entities with a callback
         * @param callback Function to call for each entity
         */
        void forEach(std::function<void(std::shared_ptr<Entity>)> callback) const;

        /**
         * @brief Iterate over entities of a specific type with a callback
         * @tparam T The entity type to filter by
         * @param callback Function to call for each entity of type T
         */
        template<typename T>
        void forEach(std::function<void(std::shared_ptr<T>)> callback) const {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            
            for (const auto& pair : m_entities) {
                if (auto typedEntity = std::dynamic_pointer_cast<T>(pair.second)) {
                    callback(typedEntity);
                }
            }
        }

        /**
         * @brief Get the number of entities managed
         * @return The total number of entities
         */
        size_t getEntityCount() const { return m_entities.size(); }

        /**
         * @brief Check if an entity exists by ID
         * @param id The entity ID to check
         * @return True if the entity exists
         */
        bool hasEntity(EntityID id) const;

        /**
         * @brief Clear all entities
         */
        void clear();

        /**
         * @brief Update all entities
         * @param deltaTime Time elapsed since last update
         */
        void updateAll(float deltaTime);

        /**
         * @brief Initialize all entities
         */
        void initializeAll();

        /**
         * @brief Cleanup all entities
         */
        void cleanupAll();

        /**
         * @brief Get entity statistics
         */
        struct EntityStats {
            size_t totalEntities = 0;
            size_t validEntities = 0;
            size_t invalidEntities = 0;
            EntityID highestID = 0;
            EntityID lowestID = INVALID_ENTITY_ID;
        };

        /**
         * @brief Get current entity statistics
         * @return Entity statistics structure
         */
        EntityStats getStats() const;

    private:
        EntityManager() = default;
        ~EntityManager() = default;

        // Non-copyable and non-movable
        EntityManager(const EntityManager&) = delete;
        EntityManager& operator=(const EntityManager&) = delete;
        EntityManager(EntityManager&&) = delete;
        EntityManager& operator=(EntityManager&&) = delete;

        std::unordered_map<EntityID, std::shared_ptr<Entity>> m_entities;
        mutable std::mutex m_mutex; // For thread safety
    };

    /**
     * @brief Convenience function to get the EntityManager instance
     * @return Reference to the EntityManager singleton
     */
    inline EntityManager& getEntityManager() {
        return EntityManager::getInstance();
    }

} // namespace IKore
