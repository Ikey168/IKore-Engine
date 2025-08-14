#include "Frustum.h"
#include "core/Logger.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace IKore {

    // ============================================================================
    // BoundingBox Implementation
    // ============================================================================

    BoundingBox BoundingBox::transform(const glm::mat4& transform) const {
        // Transform all 8 corners of the box and find new min/max
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };
        
        BoundingBox result;
        bool first = true;
        
        for (int i = 0; i < 8; ++i) {
            glm::vec4 transformedCorner = transform * glm::vec4(corners[i], 1.0f);
            glm::vec3 corner = glm::vec3(transformedCorner) / transformedCorner.w;
            
            if (first) {
                result.min = result.max = corner;
                first = false;
            } else {
                result.expand(corner);
            }
        }
        
        return result;
    }

    void BoundingBox::expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void BoundingBox::expand(const BoundingBox& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    bool BoundingBox::contains(const glm::vec3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool BoundingBox::intersects(const BoundingBox& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }

    // ============================================================================
    // Plane Implementation
    // ============================================================================

    Plane Plane::fromPoints(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
        glm::vec3 v1 = p2 - p1;
        glm::vec3 v2 = p3 - p1;
        glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
        float distance = -glm::dot(normal, p1);
        return Plane(normal, distance);
    }

    void Plane::normalize() {
        float length = glm::length(normal);
        if (length > 0.0f) {
            normal /= length;
            distance /= length;
        }
    }

    // ============================================================================
    // Frustum Implementation
    // ============================================================================

    void Frustum::extractFromMatrix(const glm::mat4& viewProjectionMatrix) {
        // Extract frustum planes from combined view-projection matrix
        // Based on Gribb & Hartmann method
        const glm::mat4& m = viewProjectionMatrix;
        
        // Left plane: m[3] + m[0]
        planes[LEFT] = Plane(
            glm::vec3(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0]),
            m[3][3] + m[3][0]
        );
        
        // Right plane: m[3] - m[0]
        planes[RIGHT] = Plane(
            glm::vec3(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0]),
            m[3][3] - m[3][0]
        );
        
        // Top plane: m[3] - m[1]
        planes[TOP] = Plane(
            glm::vec3(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1]),
            m[3][3] - m[3][1]
        );
        
        // Bottom plane: m[3] + m[1]
        planes[BOTTOM] = Plane(
            glm::vec3(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1]),
            m[3][3] + m[3][1]
        );
        
        // Near plane: m[3] + m[2]
        planes[NEAR] = Plane(
            glm::vec3(m[0][3] + m[0][2], m[1][3] + m[1][2], m[2][3] + m[2][2]),
            m[3][3] + m[3][2]
        );
        
        // Far plane: m[3] - m[2]
        planes[FAR] = Plane(
            glm::vec3(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2]),
            m[3][3] - m[3][2]
        );
        
        // Normalize all planes
        for (auto& plane : planes) {
            plane.normalize();
        }
    }

    void Frustum::extractFromMatrices(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
        extractFromMatrix(projectionMatrix * viewMatrix);
    }

    bool Frustum::containsPoint(const glm::vec3& point) const {
        for (const auto& plane : planes) {
            if (plane.distanceToPoint(point) < 0.0f) {
                return false; // Point is behind this plane
            }
        }
        return true;
    }

    bool Frustum::containsSphere(const glm::vec3& center, float radius) const {
        for (const auto& plane : planes) {
            float distance = plane.distanceToPoint(center);
            if (distance < -radius) {
                return false; // Sphere is completely behind this plane
            }
        }
        return true;
    }

    bool Frustum::containsAABB(const BoundingBox& boundingBox) const {
        // Test each plane against the bounding box
        for (const auto& plane : planes) {
            // Find the positive vertex (furthest point in direction of plane normal)
            glm::vec3 positiveVertex = boundingBox.min;
            if (plane.normal.x >= 0) positiveVertex.x = boundingBox.max.x;
            if (plane.normal.y >= 0) positiveVertex.y = boundingBox.max.y;
            if (plane.normal.z >= 0) positiveVertex.z = boundingBox.max.z;
            
            // If positive vertex is behind plane, box is completely outside
            if (plane.distanceToPoint(positiveVertex) < 0.0f) {
                return false;
            }
        }
        return true;
    }

    bool Frustum::containsTransformedAABB(const BoundingBox& boundingBox, const glm::mat4& modelMatrix) const {
        // Transform the bounding box and test against frustum
        BoundingBox transformedBox = boundingBox.transform(modelMatrix);
        return containsAABB(transformedBox);
    }

    float Frustum::distanceToPoint(const glm::vec3& point) const {
        float minDistance = std::numeric_limits<float>::max();
        for (const auto& plane : planes) {
            float distance = plane.distanceToPoint(point);
            minDistance = std::min(minDistance, distance);
        }
        return minDistance;
    }

    bool Frustum::needsUpdate(const glm::mat4& lastViewMatrix, const glm::mat4& lastProjectionMatrix,
                             const glm::mat4& currentViewMatrix, const glm::mat4& currentProjectionMatrix) {
        // Simple matrix comparison - could be optimized with epsilon comparison
        return lastViewMatrix != currentViewMatrix || lastProjectionMatrix != currentProjectionMatrix;
    }

    // ============================================================================
    // FrustumCuller Implementation
    // ============================================================================

    void FrustumCuller::updateFrustum(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
        // Check if frustum needs updating
        if (Frustum::needsUpdate(lastViewMatrix, lastProjectionMatrix, viewMatrix, projectionMatrix)) {
            frustum.extractFromMatrices(viewMatrix, projectionMatrix);
            lastViewMatrix = viewMatrix;
            lastProjectionMatrix = projectionMatrix;
            frustumNeedsUpdate = false;
            
            LOG_INFO("Frustum updated due to camera matrix changes");
        }
    }

    void FrustumCuller::cullObjects(std::vector<RenderableObject>& objects) {
        stats.reset();
        stats.totalObjects = objects.size();
        
        for (auto& object : objects) {
            object.isVisible = frustum.containsTransformedAABB(object.boundingBox, object.modelMatrix);
            
            if (object.isVisible) {
                stats.visibleObjects++;
            } else {
                stats.culledObjects++;
            }
        }
        
        stats.update();
        
        if (stats.totalObjects > 0) {
            LOG_INFO("Frustum culling: " + std::to_string(stats.visibleObjects) + "/" + 
                     std::to_string(stats.totalObjects) + " objects visible (" + 
                     std::to_string(stats.cullingRatio * 100.0f) + "% culled)");
        }
    }

    bool FrustumCuller::isVisible(const RenderableObject& object) const {
        return frustum.containsTransformedAABB(object.boundingBox, object.modelMatrix);
    }

    bool FrustumCuller::isVisible(const BoundingBox& boundingBox, const glm::mat4& modelMatrix) const {
        return frustum.containsTransformedAABB(boundingBox, modelMatrix);
    }

} // namespace IKore
