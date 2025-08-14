#include "Component.h"
#include "Entity.h"

Component::Component() : entity_() {}

Component::~Component() = default;

void Component::setEntity(std::weak_ptr<IKore::Entity> entity) {
    entity_ = entity;
    onAttach();
}

std::weak_ptr<IKore::Entity> Component::getEntity() const {
    return entity_;
}

void Component::onAttach() {
    // Default implementation - override in derived classes
}

void Component::onDetach() {
    // Default implementation - override in derived classes
}
