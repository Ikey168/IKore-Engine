#pragma once

#include "../Component.h"
#include "../../Model.h"
#include <memory>
#include <vector>

namespace IKore {

    /**
     * @brief Component for storing mesh data directly in entities
     * 
     * The MeshComponent allows entities to store mesh data (vertices, indices, materials)
     * directly without requiring file loading. This is useful for:
     * - Procedurally generated meshes
     * - Dynamic mesh modifications
     * - In-memory mesh operations
     * - Runtime mesh creation
     * 
     * Unlike RenderableComponent which stores file paths, MeshComponent stores
     * the actual mesh data for immediate rendering.
     */
    class MeshComponent : public Component {
    public:
        MeshComponent() = default;
        virtual ~MeshComponent() = default;

        // === Mesh Data Storage ===
        
        /**
         * @brief Set mesh data from vertices and indices
         * @param vertices Vertex data (position, normal, texCoords, etc.)
         * @param indices Index data for element buffer
         * @param material Material properties for rendering
         */
        void setMeshData(const std::vector<Vertex>& vertices, 
                        const std::vector<unsigned int>& indices,
                        const Material& material = Material{});

        /**
         * @brief Set mesh data from an existing Mesh object
         * @param mesh Mesh object to copy data from
         */
        void setMeshData(const Mesh& mesh);

        /**
         * @brief Add a mesh to this component (supports multiple meshes)
         * @param vertices Vertex data
         * @param indices Index data
         * @param material Material properties
         * @return Index of the added mesh
         */
        size_t addMesh(const std::vector<Vertex>& vertices,
                      const std::vector<unsigned int>& indices,
                      const Material& material = Material{});

        /**
         * @brief Clear all mesh data
         */
        void clearMeshes();

        // === Mesh Access ===

        /**
         * @brief Get the primary mesh (first mesh if multiple exist)
         * @return Pointer to the primary mesh, or nullptr if no meshes
         */
        const Mesh* getPrimaryMesh() const;

        /**
         * @brief Get mesh by index
         * @param index Index of the mesh to retrieve
         * @return Pointer to the mesh, or nullptr if index is invalid
         */
        const Mesh* getMesh(size_t index) const;

        /**
         * @brief Get all meshes
         * @return Vector of mesh pointers
         */
        const std::vector<std::unique_ptr<Mesh>>& getMeshes() const { return meshes_; }

        /**
         * @brief Get the number of meshes
         * @return Number of meshes stored in this component
         */
        size_t getMeshCount() const { return meshes_.size(); }

        /**
         * @brief Check if this component has any meshes
         * @return True if at least one mesh is stored
         */
        bool hasMeshes() const { return !meshes_.empty(); }

        // === Rendering Properties ===

        /**
         * @brief Whether this mesh should be rendered
         */
        bool visible = true;

        /**
         * @brief Render all meshes with the given shader
         * @param shader Shader to use for rendering
         */
        void render(std::shared_ptr<Shader> shader) const;

        // === Utility Functions ===

        /**
         * @brief Get the combined bounding box of all meshes
         * @return BoundingBox that encompasses all meshes
         */
        BoundingBox getBoundingBox() const;

        /**
         * @brief Get total vertex count across all meshes
         * @return Total number of vertices
         */
        size_t getTotalVertexCount() const;

        /**
         * @brief Get total index count across all meshes
         * @return Total number of indices
         */
        size_t getTotalIndexCount() const;

        // === Static Factory Methods ===

        /**
         * @brief Create a cube mesh component
         * @param size Size of the cube (default 1.0f)
         * @param material Material to apply (default material if not specified)
         * @return MeshComponent with cube geometry
         */
        static std::shared_ptr<MeshComponent> createCube(float size = 1.0f, const Material& material = Material{});

        /**
         * @brief Create a plane mesh component
         * @param width Width of the plane (default 1.0f)
         * @param height Height of the plane (default 1.0f)
         * @param material Material to apply (default material if not specified)
         * @return MeshComponent with plane geometry
         */
        static std::shared_ptr<MeshComponent> createPlane(float width = 1.0f, float height = 1.0f, const Material& material = Material{});

        /**
         * @brief Create a sphere mesh component
         * @param radius Radius of the sphere (default 1.0f)
         * @param segments Number of segments for sphere tessellation (default 32)
         * @param material Material to apply (default material if not specified)
         * @return MeshComponent with sphere geometry
         */
        static std::shared_ptr<MeshComponent> createSphere(float radius = 1.0f, int segments = 32, const Material& material = Material{});

    protected:
        void onAttach() override;
        void onDetach() override;

    private:
        /// Storage for multiple meshes
        std::vector<std::unique_ptr<Mesh>> meshes_;

        /// Helper function to create cube vertices
        static std::vector<Vertex> createCubeVertices(float size);
        
        /// Helper function to create cube indices
        static std::vector<unsigned int> createCubeIndices();

        /// Helper function to create plane vertices
        static std::vector<Vertex> createPlaneVertices(float width, float height);
        
        /// Helper function to create plane indices
        static std::vector<unsigned int> createPlaneIndices();

        /// Helper function to create sphere vertices
        static std::vector<Vertex> createSphereVertices(float radius, int segments);
        
        /// Helper function to create sphere indices
        static std::vector<unsigned int> createSphereIndices(int segments);
    };

} // namespace IKore
