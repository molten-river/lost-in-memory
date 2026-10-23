#pragma once

#include "alias/alias.hpp"
#include "shapes/convex_hull.hpp"
#include "shapes/motion_strategy.hpp"
#include "shapes/shape.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// One connected spine of rigid members walked out of a record, each joint posing its member and everything outboard.
class WalkMass final : public Shape
{
  public:
    static constexpr u32 MAX_MEMBERS = 7;
    static constexpr u32 MAX_JOINTS = MAX_MEMBERS + 1;
    static constexpr u32 MAX_PIECES = 2 * MAX_MEMBERS + 1;

    void Evolve(f32 seconds) override;
    [[nodiscard]] bool Emit(MeshBuilder &builder, const f32m4 &transform, u32 packedColor) const override;
    [[nodiscard]] f32 Distance(const f32v3 &local) const override;
    [[nodiscard]] f32v3 Gradient(const f32v3 &local) const override;
    [[nodiscard]] f32 GetBoundingRadius() const override;

  private:
    friend class WalkMassBuilder;

    /// A rigid hull in its joint's frame, carried by the joint's rest frame or by its moved frame.
    struct Piece
    {
        ConvexHull Hull{};
        u8 Joint = 0;
        bool FollowsMotion = true;
    };

    /// Rest is the joint's frame in its parent joint's frame; joint 0's parent is the mass origin.
    struct Joint
    {
        f32m4 Rest;
        JointMotion Motion;
    };

    struct NearestPiece
    {
        u32 Index;
        f32 Distance;
        f32v3 Local;
    };

    [[nodiscard]] NearestPiece FindNearestPiece(const f32v3 &local) const;

    TKit::StaticArray<Piece, MAX_PIECES> m_Pieces{};
    TKit::StaticArray<Joint, MAX_JOINTS> m_Joints{};
    TKit::FixedArray<f32m4, MAX_PIECES> m_Poses{};
    u32 m_VertexCount = 0;
    f32 m_BoundingRadius = 0.f;
};

static_assert(WalkMass::MAX_PIECES <= Shape::MAX_HULLS);
} // namespace Lim
