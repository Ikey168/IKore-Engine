#include "EventSystem.h"

namespace IKore {

EventSystem& EventSystem::getInstance() {
    static EventSystem instance;
    return instance;
}

EventSubscription EventSystem::subscribe(const std::string& eventType, EventCallback callback) {
    // Generate a unique ID for this subscription
    size_t subscriptionId = nextSubscriptionId++;
    
    // Add the callback to the subscribers list
    subscribers[eventType].push_back({subscriptionId, callback});
    
    // Return a subscription token that can be used to unsubscribe
    return EventSubscription{eventType, subscriptionId};
}

void EventSystem::unsubscribe(const EventSubscription& subscription) {
    // Find the event type
    auto it = subscribers.find(subscription.eventType);
    if (it == subscribers.end()) {
        return;
    }
    
    // Find and remove the subscription with the matching ID
    auto& callbacks = it->second;
    auto callbackIt = std::find_if(callbacks.begin(), callbacks.end(),
        [&subscription](const auto& item) {
            return item.first == subscription.id;
        });
    
    if (callbackIt != callbacks.end()) {
        callbacks.erase(callbackIt);
    }
    
    // If no subscribers left for this event type, remove the entry
    if (callbacks.empty()) {
        subscribers.erase(it);
    }
}

void EventSystem::publish(const std::string& eventType, const std::shared_ptr<EventData>& data) {
    // Find subscribers for this event type
    auto it = subscribers.find(eventType);
    if (it == subscribers.end()) {
        return;  // No subscribers for this event
    }
    
    // Call each subscriber callback
    const auto& callbacks = it->second;
    for (const auto& [id, callback] : callbacks) {
        callback(data);
    }
}

void EventSystem::clear() {
    subscribers.clear();
    nextSubscriptionId = 0;
}

} // namespace IKore
