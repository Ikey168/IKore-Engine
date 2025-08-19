#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <any>
#include <typeindex>
#include <algorithm>

namespace IKore {

// Forward declaration
class Entity;

/**
 * @brief Base class for all event data
 */
class EventData {
public:
    virtual ~EventData() = default;
    
    // Optional sender entity
    Entity* sender = nullptr;
};

/**
 * @brief Template class for typed event data
 */
template<typename T>
class TypedEventData : public EventData {
public:
    T data;
    
    TypedEventData(const T& value) : data(value) {}
};

/**
 * @brief Event handler function type
 */
using EventCallback = std::function<void(const std::shared_ptr<EventData>&)>;

/**
 * @brief Event subscription token for unsubscribing
 */
struct EventSubscription {
    std::string eventType;
    size_t id;
    
    bool operator==(const EventSubscription& other) const {
        return eventType == other.eventType && id == other.id;
    }
};

/**
 * @brief Global event system for entity communication
 * 
 * The EventSystem provides a way for entities and systems to communicate
 * without direct coupling. It implements a publish-subscribe pattern.
 */
class EventSystem {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the EventSystem instance
     */
    static EventSystem& getInstance();
    
    /**
     * @brief Subscribe to an event type
     * @param eventType String identifier for the event
     * @param callback Function to call when event is triggered
     * @return Subscription token used for unsubscribing
     */
    EventSubscription subscribe(const std::string& eventType, EventCallback callback);
    
    /**
     * @brief Unsubscribe from an event
     * @param subscription The subscription token returned from subscribe()
     */
    void unsubscribe(const EventSubscription& subscription);
    
    /**
     * @brief Publish an event to all subscribers
     * @param eventType String identifier for the event
     * @param data Shared pointer to event data
     */
    void publish(const std::string& eventType, const std::shared_ptr<EventData>& data);
    
    /**
     * @brief Helper template method to publish typed event data
     * @tparam T Type of the event data
     * @param eventType String identifier for the event
     * @param data The data to publish
     * @param sender Optional pointer to the entity sending the event
     */
    template<typename T>
    void publish(const std::string& eventType, const T& data, Entity* sender = nullptr) {
        auto eventData = std::make_shared<TypedEventData<T>>(data);
        eventData->sender = sender;
        publish(eventType, std::static_pointer_cast<EventData>(eventData));
    }
    
    /**
     * @brief Clear all subscriptions
     */
    void clear();
    
private:
    EventSystem() = default;
    ~EventSystem() = default;
    
    // Disable copy/move
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;
    EventSystem(EventSystem&&) = delete;
    EventSystem& operator=(EventSystem&&) = delete;
    
    // Map of event types to callbacks
    std::unordered_map<std::string, std::vector<std::pair<size_t, EventCallback>>> subscribers;
    
    // Counter for generating unique subscription IDs
    size_t nextSubscriptionId = 0;
};

} // namespace IKore
