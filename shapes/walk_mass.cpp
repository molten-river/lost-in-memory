#include "shapes/walk_mass.hpp"
#include "shapes/motion_factory.hpp"

#include "tkit/utils/limits.hpp"

namespace Lim
{
void WalkMass::Evolve(const f32 seconds)
{
    TKit::FixedArray<f32m4, MAX_JOINTS> rests{};
    TKit::FixedArray<f32m4, MAX_JOINTS> moved{};
    for (u32 i = 0; i < m_Joints.GetSize(); ++i)
    {
        const Joint &joint = m_Joints[i];
        rests[i] = i == 0 ? joint.Rest : moved[i - 1] * joint.Rest;
        moved[i] = rests[i] * MotionFactory::Get(joint.Motion.Kind).Evaluate(joint.Motion, seconds).Transform;
    }
    for (u32 i = 0; i < m_Pieces.GetSize(); ++i)
    {
        const Piece &piece = m_Pieces[i];
        m_Poses[i] = piece.FollowsMotion ? moved[piece.Joint] : rests[piece.Joint];
    }
}

bool WalkMass::Emit(MeshBuilder &builder, const f32m4 &transform, const u32 packedColor) const
{
    if (builder.GetVertexCount() + m_VertexCount > builder.GetCapacity())
        return false;
    for (u32 i = 0; i < m_Pieces.GetSize(); ++i)
        if (!builder.AppendHull(m_Pieces[i].Hull, transform * m_Poses[i], packedColor))
            return false;
    return true;
}

f32 WalkMass::Distance(const f32v3 &local) const
{
    return FindNearestPiece(local).Distance;
}

f32v3 WalkMass::Gradient(const f32v3 &local) const
{
    const NearestPiece nearest = FindNearestPiece(local);
    const f32v3 gradient = m_Pieces[nearest.Index].Hull.Gradient(nearest.Local);
    return f32v3(m_Poses[nearest.Index] * f32v4{gradient, 0.f});
}

f32 WalkMass::GetBoundingRadius() const
{
    return m_BoundingRadius;
}

/// Poses are rigid, so a point reaches a piece's frame through the transposed rotation of its offset.
WalkMass::NearestPiece WalkMass::FindNearestPiece(const f32v3 &local) const
{
    NearestPiece nearest{.Index = 0, .Distance = TKit::Limits<f32>::Max(), .Local = local};
    for (u32 i = 0; i < m_Pieces.GetSize(); ++i)
    {
        const f32m4 &pose = m_Poses[i];
        const f32v3 offset = local - f32v3(pose[3]);
        const f32v3 pieceLocal{TKit::Math::Dot(f32v3(pose[0]), offset), TKit::Math::Dot(f32v3(pose[1]), offset),
                               TKit::Math::Dot(f32v3(pose[2]), offset)};
        const f32 distance = m_Pieces[i].Hull.Distance(pieceLocal);
        if (distance < nearest.Distance)
            nearest = NearestPiece{.Index = i, .Distance = distance, .Local = pieceLocal};
    }
    return nearest;
}
} // namespace Lim
