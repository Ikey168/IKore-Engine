#pragma once

#include "render/Skinning.h" // render::skin::kMaxInfluences, skinPosition (the mirrored core)

/**
 * @file ShadowSkinning.h
 * @brief Shadow-pass program selection and palette-upload policy for skinned meshes
 *        (issue #266).
 *
 * The GPU wiring for skinned shadows has two small decisions that are worth pulling
 * out of the render loop so they are unit-testable and shared between the directional
 * and point shadow passes:
 *
 *   - shadowProgramFor(hasBones): pick the skinned depth program for a mesh that has
 *     bones, otherwise the static one. This is exactly the selection the forward pass
 *     makes between skinned_phong.vert and the static vertex path, applied to the
 *     shadow depth programs (skinned_shadow_depth.vert vs shadow_depth.vert).
 *   - shadowPaletteUploadCount(count): how many bone matrices to upload, clamped to
 *     the shader's finalBonesMatrices[MAX_BONES] capacity. Mirrors the MAX_BONES
 *     bound the skinned shaders guard against (a bone id >= MAX_BONES is skipped), so
 *     the CPU never overruns the uniform array.
 *
 * The skinning math itself is not duplicated here - it lives in the tested
 * render::skin core, which both skinned_phong.vert and skinned_shadow_depth.vert
 * mirror line for line, so the shadow pose matches the lit pose. Header-only.
 */
namespace IKore {
namespace render {

/// Which shadow depth program a mesh should render with.
enum class ShadowProgram {
    Static,  ///< shadow_depth.vert / shadow_point.vert
    Skinned  ///< skinned_shadow_depth.vert / skinned_shadow_point.vert
};

/// The shadow depth program to use for a mesh, given whether it has bones. Matches
/// the forward pass's skinned-vs-static shader choice, so shadows deform with the pose.
inline ShadowProgram shadowProgramFor(bool hasBones) {
    return hasBones ? ShadowProgram::Skinned : ShadowProgram::Static;
}

/// The bone palette shader capacity (skinned_shadow_depth.vert's finalBonesMatrices).
inline constexpr int kShadowMaxBones = 100;

/**
 * @brief Clamp a palette size to the shader's uniform-array capacity for upload.
 * @param boneCount Bones the animation produced (may be 0, or exceed the cap).
 * @param maxBones  Shader capacity (defaults to kShadowMaxBones, the shader constant).
 * @return A count in [0, maxBones] safe to upload to finalBonesMatrices.
 *
 * A negative count clamps to 0 (nothing to upload -> the shaders' rigid fallback), and
 * a count past the cap clamps down so the upload never overruns the uniform array; the
 * shaders already skip any bone id >= MAX_BONES, so clamping here keeps CPU and GPU
 * consistent about how many bones exist.
 */
inline int shadowPaletteUploadCount(int boneCount, int maxBones = kShadowMaxBones) {
    if (boneCount < 0) return 0;
    if (boneCount > maxBones) return maxBones;
    return boneCount;
}

} // namespace render
} // namespace IKore
