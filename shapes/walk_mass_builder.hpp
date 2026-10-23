#pragma once

#include "alias/alias.hpp"
#include "shapes/walk_mass.hpp"
#include "shapes/walk_mass_params.hpp"

#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

#include <cstddef>

namespace Lim
{
/// Walks a record's sections into rigid members and its joint bytes into motions; the same input bakes the same bits.
class WalkMassBuilder
{
  public:
    WalkMassBuilder(TKit::Span<const std::byte> record, const WalkMassParams &params, TKit::Optional<f32> sizeOverride);

    [[nodiscard]] TKit::Scope<WalkMass> Build() const;

  private:
    static constexpr u32 MAX_PROFILE_CORNERS = 8;

    /// A section's unrolled profile corners, its width, its roll as a cosine and sine, and its start face scale.
    struct Section
    {
        TKit::StaticArray<f32v2, MAX_PROFILE_CORNERS> Corners;
        f32 Width;
        f32v2 Roll;
        f32 Taper;
    };

    [[nodiscard]] Section ReadSection(u32 section) const;
    [[nodiscard]] JointMotion ReadMotion(u32 joint, f32 length) const;
    [[nodiscard]] f32 ReadLength(u8 lengthNibble) const;
    [[nodiscard]] f32m4 ComputeRest(u8 lead, f32 previousWidth, f32 previousLength) const;
    void AddMember(WalkMass &mass, const Section &previous, const Section &current, f32 length) const;
    /// Appends a piece to the newest joint and builds its hull from the points.
    static const ConvexHull &AddPiece(WalkMass &mass, TKit::Span<const f32v3> points, bool followsMotion);
    void AddRod(WalkMass &mass, const ConvexHull &sleeve, const Section &current, f32 length, f32 reach) const;
    void AddSlab(WalkMass &mass, const Section &root) const;
    void Seal(WalkMass &mass) const;
    [[nodiscard]] u8 ReadByte(usize index) const;

    TKit::Span<const std::byte> m_Record;
    WalkMassParams m_Params;
    f32 m_Size = 0.f;
    f32 m_LatticeStep = 0.f;
};
} // namespace Lim
