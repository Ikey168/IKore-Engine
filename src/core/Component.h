#pragma once

#include <memory>
#include <typeindex>

// Forward declaration
namespace IKore {
    class Entity;
}

/**
 * @brief Base Component class for the Entity-Component System
 */
class Component {
public:
    Component();
    virtual ~Component();
    
    void setEntity(std::weak_ptr<IKore::Entity> entity);
    std::weak_ptr<IKore::Entity> getEntity() const;

protected:
    virtual void onAttach();
    virtual void onDetach();

private:
    std::weak_ptr<IKore::Entity> entity_;
    friend class IKore::Entity;
};
