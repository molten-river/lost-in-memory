#include "shapes/walk_mass.hpp"
#include "shapes/motion_factory.hpp"

#include "tkit/utils/limits.hpp"

namespace Lim
{
namespace
{
[[nodiscard]] bool AreEqual(const f32m4 &first, const f32m4 &second)
{
    for (u32 i = 0; i < f32m4::Size; ++i)
        if (first.GetData()[i] != second.GetData()[i])
            return false;
    return true;
}
} // namespace

void WalkMass::Evolve(const f32 seconds)
{
    const u32 target = seconds > 0.f ? u32(seconds / STEP_SECONDS) : 0;
    if (target < m_Step)
        Restore(target);
    const u32 steps = TKit::Math::Min(target - m_Step, MAX_STEPS_PER_EVOLVE);
    for (u32 i = 0; i < steps; ++i)
        Advance();

    const f32m4 root = EvaluateMotion(0, i32(m_Step));
    for (u32 i = 0; i < m_Pieces.GetSize(); ++i)
        m_Poses[i] = root * SelectPose(m_Pieces[i], m_Frames);
    for (u32 i = 0; i < m_SampleSources.GetSize(); ++i)
        m_Samples[i] = f32v3(m_Poses[m_SampleSources[i].Piece] * f32v4{m_SampleSources[i].Local, 1.f});
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

TKit::Span<const f32v3> WalkMass::GetSamplePoints() const
{
    return m_Samples;
}

f32 WalkMass::GetVolume() const
{
    return m_Volume;
}

f32 WalkMass::GetSimulatedSeconds() const
{
    return f32(m_Step) * STEP_SECONDS;
}

void WalkMass::Reset()
{
    m_Clocks = CreateRestClocks();
    for (Snapshot &snapshot : m_Snapshots)
        snapshot = Snapshot{.Step = 0, .State = m_Clocks};
    m_Step = 0;
    ComposeFrames();
}

/// Snapshots are tagged with their step, so a slot that was overwritten or never filled falls back to step 0.
void WalkMass::Restore(const u32 step)
{
    const u32 index = step / STEPS_PER_SNAPSHOT;
    const Snapshot &snapshot = m_Snapshots[index % SNAPSHOT_COUNT];
    const bool isKept = snapshot.Step == index * STEPS_PER_SNAPSHOT;
    m_Clocks = isKept ? snapshot.State : CreateRestClocks();
    m_Step = isKept ? snapshot.Step : 0;
    ComposeFrames();
}

WalkMass::Clocks WalkMass::CreateRestClocks()
{
    Clocks clocks{};
    for (i32 &direction : clocks.Directions)
        direction = 1;
    return clocks;
}

void WalkMass::ComposeFrames()
{
    m_Frames.Rests[0] = f32m4::Identity();
    m_Frames.Moved[0] = f32m4::Identity();
    for (u32 joint = 1; joint < m_Joints.GetSize(); ++joint)
        m_Motions[joint] = EvaluateMotion(joint, m_Clocks.Ticks[joint]);
    ChainFrames(m_Frames, 0);
}

void WalkMass::ChainFrames(Frames &frames, const u32 joint) const
{
    for (u32 next = joint + 1; next < m_Joints.GetSize(); ++next)
    {
        frames.Rests[next] = frames.Moved[next - 1] * m_Joints[next].Rest;
        frames.Moved[next] = frames.Rests[next] * m_Motions[next];
    }
}

/// Each joint tries one tick its own way; a tick that changes nothing skips the test, one that hits undoes and turns.
void WalkMass::Advance()
{
    ++m_Step;
    for (u32 joint = 1; joint < m_Joints.GetSize(); ++joint)
    {
        const i32 tick = m_Clocks.Ticks[joint] + m_Clocks.Directions[joint];
        if (tick < 0)
        {
            m_Clocks.Directions[joint] = 1;
            continue;
        }
        const f32m4 motion = EvaluateMotion(joint, tick);
        if (AreEqual(motion, m_Motions[joint]))
        {
            m_Clocks.Ticks[joint] = tick;
            continue;
        }

        Frames trial = m_Frames;
        trial.Moved[joint] = trial.Rests[joint] * motion;
        ChainFrames(trial, joint);
        if (HasContact(joint, trial))
        {
            m_Clocks.Directions[joint] = -m_Clocks.Directions[joint];
            continue;
        }
        m_Clocks.Ticks[joint] = tick;
        m_Motions[joint] = motion;
        m_Frames = trial;
    }
    if (m_Step % STEPS_PER_SNAPSHOT == 0)
        m_Snapshots[(m_Step / STEPS_PER_SNAPSHOT) % SNAPSHOT_COUNT] = Snapshot{.Step = m_Step, .State = m_Clocks};
}

/// Pieces of one member or of neighbouring members are joined; bounding spheres reject before the exact hull test.
bool WalkMass::HasContact(const u32 joint, const Frames &frames) const
{
    const auto isCarried = [joint](const Piece &piece) {
        return piece.Joint > joint || (piece.Joint == joint && piece.FollowsMotion);
    };
    for (const Piece &carried : m_Pieces)
    {
        if (!isCarried(carried))
            continue;
        const f32m4 &carriedPose = SelectPose(carried, frames);
        const f32v3 carriedCenter = f32v3(carriedPose * f32v4{carried.Hull.GetCenter(), 1.f});
        for (const Piece &still : m_Pieces)
        {
            if (still.Joint + 2 > carried.Joint || isCarried(still))
                continue;
            const f32m4 &stillPose = SelectPose(still, frames);
            const f32v3 stillCenter = f32v3(stillPose * f32v4{still.Hull.GetCenter(), 1.f});
            const f32 reach = carried.Hull.GetRadius() + still.Hull.GetRadius();
            if (TKit::Math::NormSquared(carriedCenter - stillCenter) > reach * reach)
                continue;
            if (carried.Hull.IsOverlapping(carriedPose, still.Hull, stillPose))
                return true;
        }
    }
    return false;
}

const f32m4 &WalkMass::SelectPose(const Piece &piece, const Frames &frames)
{
    return piece.FollowsMotion ? frames.Moved[piece.Joint] : frames.Rests[piece.Joint];
}

f32m4 WalkMass::EvaluateMotion(const u32 joint, const i32 tick) const
{
    const JointMotion &motion = m_Joints[joint].Motion;
    return MotionFactory::Get(motion.Kind).Evaluate(motion, f32(tick) * STEP_SECONDS).Transform;
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
