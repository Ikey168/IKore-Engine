#pragma once

#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>

#include "AudioStreaming.h" // audio::Crossfade (issue #258)

namespace IKore {

// Forward declaration to avoid circular dependency
class AudioSystem3D;

/**
 * @brief Represents an ambient sound zone in 3D space
 * 
 * Ambient sound zones define areas where specific background audio should play.
 * They support smooth transitions when entering/leaving zones and can overlap.
 */
struct AmbientSoundZone {
    std::string name;                       // Name identifier for the zone
    glm::vec3 center{0.0f, 0.0f, 0.0f};    // Center position of the zone
    float radius{10.0f};                    // Radius of the zone
    std::string audioFile;                  // Path to the audio file
    float volume{1.0f};                     // Base volume (0.0 to 1.0)
    float fadeDistance{2.0f};              // Distance over which volume fades
    int priority{0};                        // Higher priority zones override lower ones
    bool isActive{true};                   // Whether this zone is currently active
    
    /**
     * @brief Calculate the volume based on distance from center
     * @param position The position to calculate volume for
     * @return Volume multiplier (0.0 to 1.0)
     */
    float getVolumeAtPosition(const glm::vec3& position) const;
    
    /**
     * @brief Check if a position is within this zone
     * @param position The position to check
     * @return True if position is within the zone
     */
    bool isPositionInZone(const glm::vec3& position) const;
};

/**
 * @brief Manages multiple ambient sound zones and handles transitions
 * 
 * The AmbientSoundZoneManager tracks player position and manages
 * smooth transitions between different ambient zones.
 */
class AmbientSoundZoneManager {
private:
    std::vector<AmbientSoundZone> zones;
    glm::vec3 listenerPosition{0.0f, 0.0f, 0.0f};
    AmbientSoundZone* currentZone{nullptr};
    AmbientSoundZone* previousZone{nullptr}; // zone being faded out (issue #258)
    float transitionSpeed{2.0f};           // Speed of volume transitions
    audio::Crossfade transition{0.5f};     // Cross-fade between previous and current zone
    AudioSystem3D* audioSystem{nullptr};   // Reference to audio system
    
public:
    /**
     * @brief Constructor
     * @param audioSys Pointer to the audio system
     */
    explicit AmbientSoundZoneManager(AudioSystem3D* audioSys);
    
    /**
     * @brief Add a new ambient zone
     * @param zone The zone to add
     */
    void addZone(const AmbientSoundZone& zone);
    
    /**
     * @brief Remove all zones
     */
    void clearZones();
    
    /**
     * @brief Update the manager with current listener position
     * @param position Current listener position
     * @param deltaTime Time since last update
     */
    void update(const glm::vec3& position, float deltaTime);
    
    /**
     * @brief Get the currently active zone
     * @return Pointer to active zone or nullptr
     */
    const AmbientSoundZone* getCurrentZone() const { return currentZone; }

    /**
     * @brief The zone being faded out during a transition (issue #258).
     */
    const AmbientSoundZone* getPreviousZone() const { return previousZone; }

    /**
     * @brief Gain [0,1] for the incoming (current) zone during a cross-fade.
     */
    float getCurrentZoneGain() const { return transition.gains().toGain; }

    /**
     * @brief Gain [0,1] for the outgoing (previous) zone during a cross-fade.
     */
    float getPreviousZoneGain() const { return transition.gains().fromGain; }

    /**
     * @brief True while a zone cross-fade is still in progress.
     */
    bool isTransitioning() const { return !transition.done(); }
    
    /**
     * @brief Get all zones
     * @return Vector of all zones
     */
    const std::vector<AmbientSoundZone>& getZones() const { return zones; }
    
    /**
     * @brief Set transition speed
     * @param speed Speed multiplier for transitions
     */
    void setTransitionSpeed(float speed) { transitionSpeed = speed; }

private:
    /**
     * @brief Find the best zone for current position
     * @return Pointer to best zone or nullptr
     */
    AmbientSoundZone* findBestZone();
    
    /**
     * @brief Transition to a new zone
     * @param newZone The zone to transition to
     * @param deltaTime Time for transition calculation
     */
    void transitionToZone(AmbientSoundZone* newZone, float deltaTime);
};

} // namespace IKore
