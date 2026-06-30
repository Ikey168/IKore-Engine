# Event System Implementation

## Overview

The Event System in IKore Engine provides a global publish-subscribe mechanism for entity communication. It allows for decoupled communication between entities and systems, promoting a more modular architecture.

## Features

- **Global Singleton Access**: Easy access from anywhere in the codebase.
- **Type-Safe Events**: Uses templated event data to ensure type safety.
- **Subscription Management**: Simple subscribe/unsubscribe interface with subscription tokens.
- **Optional Sender Entity**: Events can optionally track which entity sent them.
- **High Performance**: Designed for low overhead even with many events.

## Implementation Details

The Event System is implemented using the following key classes:

1. **EventData**: Base class for all event data, contains an optional sender entity reference.
2. **TypedEventData**: Template class for typed event data, inherits from EventData.
3. **EventSystem**: Singleton class that manages subscriptions and dispatches events.

The system uses a map of event types to callbacks. Each callback is assigned a unique ID which is used to create a subscription token. This token can later be used to unsubscribe from events.

## Usage Examples

### Subscribing to Events

```cpp
EventSubscription subscription = EventSystem::getInstance().subscribe("collision", 
    [this](const std::shared_ptr<EventData>& data) {
        auto typedData = std::dynamic_pointer_cast<TypedEventData<CollisionEvent>>(data);
        if (typedData) {
            // Handle collision event
        }
    });
```

### Publishing Events

```cpp
CollisionEvent collision = {"Player", "Wall", 50.0f};
EventSystem::getInstance().publish("collision", collision);
```

### Unsubscribing from Events

```cpp
EventSystem::getInstance().unsubscribe(subscription);
```

## Performance Considerations

The Event System is designed to be efficient even with many events and subscribers. The implementation uses the following optimization techniques:

1. **Minimal Copying**: Event data is passed by shared pointer.
2. **Fast Lookup**: O(1) lookup of event subscribers using string keys.
3. **Efficient Subscriber Management**: Fast addition and removal of subscribers.

## Future Improvements

Potential future improvements to the Event System could include:

1. **Event Prioritization**: Allow subscribers to receive events in a priority order.
2. **Event Filtering**: Add the ability to filter events based on additional criteria.
3. **Event Queuing**: Queue events for processing in a dedicated update step.
4. **Event Recording/Playback**: For debugging and testing purposes.

## Integration with Entity Component System

The Event System is designed to work seamlessly with the Entity Component System (ECS) in IKore Engine. Entities and components can subscribe to events relevant to their functionality and publish events when significant state changes occur.
