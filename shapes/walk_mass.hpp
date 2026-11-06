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
/// A connected spine of rigid plates from a record; each joint steps on its own clock and turns back on contact.
class WalkMass final : public Shape
{
  public:
    static constexpr u32 MAX_MEMBERS = 7;
    static constexpr u32 MAX_JOINTS = MAX_MEMBERS + 1;
    static constexpr u32 MAX_PIECES = 2 * MAX_MEMBERS;
    static constexpr f32 STEP_SECONDS = 1.f / 120.f;
    static constexpr u32 STEPS_PER_SNAPSHOT = 120;
    static constexpr u32 SNAPSHOT_COUNT = 128;
    static constexpr u32 MAX_STEPS_PER_EVOLVE = 240;

    /// Simulates forward from the current step, or restores the newest snapshot at or before seconds and replays.
    void Evolve(f32 seconds) override;
    [[nodiscard]] u32 GetPieceCount() const override;
    [[nodiscard]] const ConvexHull &GetPieceHull(u32 piece) const override;
    [[nodiscard]] const f32m4 &GetPiecePose(u32 piece) const override;
    [[nodiscard]] f32 Distance(const f32v3 &local) const override;
    [[nodiscard]] f32v3 Gradient(const f32v3 &local) const override;
    [[nodiscard]] f32 GetBoundingRadius() const override;
    [[nodiscard]] TKit::Span<const f32v3> GetSamplePoints() const override;
    [[nodiscard]] f32 GetVolume() const override;
    /// The time the pose shows; it trails the requested time only while a jump longer than one call catches up.
    [[nodiscard]] f32 GetSimulatedSeconds() const;

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

    /// Each joint's own clock in steps and the way it runs; joint 0 follows the mass clock instead.
    struct Clocks
    {
        TKit::FixedArray<i32, MAX_JOINTS> Ticks{};
        TKit::FixedArray<i32, MAX_JOINTS> Directions{};
    };

    struct Snapshot
    {
        u32 Step = 0;
        Clocks State{};
    };

    /// Every joint's rest and moved frame relative to joint 0's moved frame, which carries the whole mass.
    struct Frames
    {
        TKit::FixedArray<f32m4, MAX_JOINTS> Rests{};
        TKit::FixedArray<f32m4, MAX_JOINTS> Moved{};
    };

    /// A sample point fixed in its piece's frame.
    struct SampleSource
    {
        f32v3 Local;
        u8 Piece;
    };

    struct NearestPiece
    {
        u32 Index;
        f32 Distance;
        f32v3 Local;
    };

    /// Clears every clock and snapshot back to the rest pose at step 0.
    void Reset();
    void Restore(u32 step);
    [[nodiscard]] static Clocks CreateRestClocks();
    void ComposeFrames();
    /// Recomposes every joint after the given one from its parent's moved frame and its current motion.
    void ChainFrames(Frames &frames, u32 joint) const;
    void Advance();
    /// Whether a piece the joint carries now intersects a piece that stayed put and is not joined to it.
    [[nodiscard]] bool HasContact(u32 joint, const Frames &frames) const;
    [[nodiscard]] static const f32m4 &SelectPose(const Piece &piece, const Frames &frames);
    [[nodiscard]] f32m4 EvaluateMotion(u32 joint, i32 tick) const;
    [[nodiscard]] NearestPiece FindNearestPiece(const f32v3 &local) const;

    TKit::StaticArray<Piece, MAX_PIECES> m_Pieces{};
    TKit::StaticArray<Joint, MAX_JOINTS> m_Joints{};
    Clocks m_Clocks{};
    TKit::FixedArray<f32m4, MAX_JOINTS> m_Motions{};
    Frames m_Frames{};
    TKit::FixedArray<Snapshot, SNAPSHOT_COUNT> m_Snapshots{};
    TKit::FixedArray<f32m4, MAX_PIECES> m_Poses{};
    TKit::StaticArray<SampleSource, MAX_SAMPLE_POINTS> m_SampleSources{};
    TKit::StaticArray<f32v3, MAX_SAMPLE_POINTS> m_Samples{};
    u32 m_Step = 0;
    f32 m_BoundingRadius = 0.f;
    f32 m_Volume = 0.f;
};

static_assert(WalkMass::MAX_PIECES <= Shape::MAX_HULLS);
} // namespace Lim
