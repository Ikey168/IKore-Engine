#include "AmbientSoundZone.h"
#include "AudioSystem3D.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace IKore {

float AmbientSoundZone::getVolumeAtPosition(const glm::vec3& position) const {
    float distance = glm::length(position - center);
    
    if (distance <= radius - fadeDistance) {
        // Inside the full volume zone
        return volume;
    } else if (distance <= radius) {
        // In the fade zone
        float fadeRatio = (radius - distance) / fadeDistance;
        return volume * fadeRatio;
    } else {
        // Outside the zone
        return 0.0f;
    }
}

bool AmbientSoundZone::isPositionInZone(const glm::vec3& position) const {
    float distance = glm::length(position - center);
    return distance <= radius;
}

AmbientSoundZoneManager::AmbientSoundZoneManager(AudioSystem3D* audioSys) 
    : audioSystem(audioSys) {
}

void AmbientSoundZoneManager::addZone(const AmbientSoundZone& zone) {
    zones.push_back(zone);
}

void AmbientSoundZoneManager::clearZones() {
    zones.clear();
    currentZone = nullptr;
}

void AmbientSoundZoneManager::update(const glm::vec3& position, float deltaTime) {
    listenerPosition = position;
    
    // Find the best zone for current position
    AmbientSoundZone* bestZone = findBestZone();
    
    // If we need to transition to a different zone
    if (bestZone != currentZone) {
        transitionToZone(bestZone, deltaTime);
    }
}

AmbientSoundZone* AmbientSoundZoneManager::findBestZone() {
    AmbientSoundZone* bestZone = nullptr;
    float bestVolume = 0.0f;
    int highestPriority = -1;
    
    for (auto& zone : zones) {
        if (!zone.isActive) continue;
        
        float volumeAtPosition = zone.getVolumeAtPosition(listenerPosition);
        
        if (volumeAtPosition > 0.0f) {
            // Consider both volume and priority
            if (zone.priority > highestPriority || 
                (zone.priority == highestPriority && volumeAtPosition > bestVolume)) {
                bestZone = &zone;
                bestVolume = volumeAtPosition;
                highestPriority = zone.priority;
            }
        }
    }
    
    return bestZone;
}

void AmbientSoundZoneManager::transitionToZone(AmbientSoundZone* newZone, float /* deltaTime */) {
    // For now we just swap the current zone pointer; cross-faded transitions need
    // streaming ambient audio, tracked in #258.
    currentZone = newZone;

    if (newZone) {
        // Once streaming ambient audio lands (#258), this per-position volume drives playback.
        [[maybe_unused]] float volume = newZone->getVolumeAtPosition(listenerPosition);
    }
}

} // namespace IKore
