#pragma once

#include "alias/alias.hpp"
#include "shapes/convex_hull.hpp"
#include "shapes/walk_mass.hpp"
#include "shapes/walk_mass_params.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

#include <cstddef>

namespace Lim
{
/// Walks a record's sections into plates and its joint bytes into motions; the same input bakes the same bits.
class WalkMassBuilder
{
  public:
    WalkMassBuilder(TKit::Span<const std::byte> record, const WalkMassParams &params, TKit::Optional<f32> sizeOverride);

    [[nodiscard]] TKit::Scope<WalkMass> Build() const;

  private:
    static constexpr u32 MAX_PROFILE_CORNERS = 8;

    /// A section's unrolled profile corners, its breadth, its roll as a cosine and sine, and its start face scale.
    struct Section
    {
        TKit::StaticArray<f32v2, MAX_PROFILE_CORNERS> Corners;
        f32 Breadth;
        f32v2 Roll;
        f32 Taper;
    };

    /// What a new member needs from the spine so far: the member before it and every joint's rest frame.
    struct Spine
    {
        TKit::FixedArray<f32m4, WalkMass::MAX_JOINTS> Frames;
        Section Previous;
        f32 PreviousLength;
        f32 RootBreadth;
    };

    [[nodiscard]] Section ReadSection(u32 section, f32 rootBreadth) const;
    [[nodiscard]] JointMotion ReadMotion(u32 joint, f32 length, const f32v2 &shearReach) const;
    /// Adds the section's member and those after it; false when a member the mass must keep found no room.
    bool Grow(WalkMass &mass, const Spine &spine, u32 section, u32 &budget) const;
    [[nodiscard]] u32 CountClearanceSteps(const WalkMass &mass, u32 section, const f32m4 &rest) const;
    [[nodiscard]] u32 CountLengthSteps(u8 lengthNibble, const Section &previous, const Section &current) const;
    [[nodiscard]] f32m4 ComputeRest(u32 turn, bool isOffset, const Section &previous, f32 previousLength) const;
    /// The previous profile, tapered, at the joint and this section's profile at the member's end, both rolled.
    [[nodiscard]] static TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> CollectMemberCorners(const Section &previous,
                                                                                               const Section &current,
                                                                                               f32 length);
    [[nodiscard]] bool HasRestContact(const WalkMass &mass, const Spine &spine, u32 section, const ConvexHull &hull,
                                      const f32m4 &frame) const;
    [[nodiscard]] f32v2 ComputeShearReach(const WalkMass &mass, const Section &previous, const Section &current,
                                          const f32m4 &rest, u32 section) const;
    void AddMember(WalkMass &mass, const Spine &spine, const Section &current, const f32m4 &rest, f32 length,
                   u32 section) const;
    /// Appends a piece to the newest joint and builds its hull from the points.
    static const ConvexHull &AddPiece(WalkMass &mass, TKit::Span<const f32v3> points, bool followsMotion);
    void AddRod(WalkMass &mass, const ConvexHull &sleeve, const Section &current, f32 length, f32 reach) const;
    void Seal(WalkMass &mass) const;
    [[nodiscard]] u8 ReadByte(usize index) const;

    TKit::Span<const std::byte> m_Record;
    WalkMassParams m_Params;
    f32 m_Size = 0.f;
    f32 m_LatticeStep = 0.f;
};
} // namespace Lim
