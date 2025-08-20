#include "ParticleComponent.h"
#include "../Entity.h"
#include "../TransformableEntities.h"
#include "../components/TransformComponent.h"
#include <glm/gtc/matrix_transform.hpp>

namespace IKore {

ParticleComponent::ParticleComponent() 
    : m_particleSystem(std::make_unique<ParticleSystem>())
    , m_effectType(ParticleEffectType::FIRE)
    , m_followEntity(true)
    , m_initialized(false) {
}

ParticleComponent::ParticleComponent(ParticleEffectType type)
    : m_particleSystem(std::make_unique<ParticleSystem>())
    , m_effectType(type)
    , m_followEntity(true)
    , m_initialized(false) {
}

ParticleComponent::~ParticleComponent() {
    // Particle system cleanup handled by unique_ptr
}

bool ParticleComponent::initialize(int maxParticles) {
    bool success = m_particleSystem->initialize(maxParticles);
    if (success) {
        m_particleSystem->loadPreset(m_effectType);
        m_initialized = true;
    }
    return success;
}

void ParticleComponent::onAttach() {
    if (!m_initialized) {
        initialize();
    }
    updatePosition();
}

void ParticleComponent::onDetach() {
    stop();
}

void ParticleComponent::update(float deltaTime) {
    if (m_followEntity) {
        updatePosition();
    }
    m_particleSystem->update(deltaTime);
}

void ParticleComponent::render(const glm::mat4& view, const glm::mat4& projection) {
    m_particleSystem->render(view, projection);
}

void ParticleComponent::play() {
    m_particleSystem->play();
}

void ParticleComponent::pause() {
    m_particleSystem->pause();
}

void ParticleComponent::stop() {
    m_particleSystem->stop();
}

void ParticleComponent::reset() {
    m_particleSystem->reset();
}

void ParticleComponent::burst(int count) {
    m_particleSystem->burst(count);
}

void ParticleComponent::setEffectType(ParticleEffectType type) {
    m_effectType = type;
    m_particleSystem->loadPreset(type);
}

void ParticleComponent::setTexture(const std::string& texturePath) {
    m_particleSystem->setTexture(texturePath);
}

void ParticleComponent::setEmitterConfig(const ParticleEmitterConfig& config) {
    m_particleSystem->setEmitterConfig(config);
}

const ParticleEmitterConfig& ParticleComponent::getEmitterConfig() const {
    return m_particleSystem->getEmitterConfig();
}

void ParticleComponent::setEmissionRate(float rate) {
    m_particleSystem->setEmissionRate(rate);
}

void ParticleComponent::setMaxParticles(int max) {
    m_particleSystem->setMaxParticles(max);
}

void ParticleComponent::setGravity(const glm::vec3& gravity) {
    m_particleSystem->setGravity(gravity);
}

void ParticleComponent::setColors(const glm::vec4& start, const glm::vec4& end) {
    m_particleSystem->setColors(start, end);
}

void ParticleComponent::setSizes(float start, float end) {
    m_particleSystem->setSizes(start, end);
}

void ParticleComponent::setLifetime(float life) {
    m_particleSystem->setLifetime(life);
}

bool ParticleComponent::isPlaying() const {
    return m_particleSystem->isPlaying();
}

int ParticleComponent::getActiveParticles() const {
    return m_particleSystem->getActiveParticles();
}

void ParticleComponent::updatePosition() {
    auto entity = getEntity().lock();
    if (entity) {
        glm::vec3 entityPosition(0.0f);
        
        // Try to get position from TransformComponent
        auto transformComponent = entity->getComponent<TransformComponent>();
        if (transformComponent) {
            entityPosition = transformComponent->getPosition();
        }
        
        // Update particle system position
        auto config = m_particleSystem->getEmitterConfig();
        config.position = entityPosition;
        m_particleSystem->setEmitterConfig(config);
    }
}

} // namespace IKore
