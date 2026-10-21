#pragma once

#include "lim/alias.hpp"
#include "lim/convex_hull.hpp"
#include "lim/form_recipe.hpp"
#include "lim/formula_params.hpp"

#include "onyx/vertex.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// The freestyle shape of one body in body-local space: evolving sections joined by convex cells along their links.
class SectionGraph
{
    struct PieceSeed;

  public:
    static constexpr u32 MAX_SECTIONS = 8;
    static constexpr u32 MAX_LINKS = MAX_SECTIONS * (MAX_SECTIONS - 1) / 2;
    static constexpr u32 MAX_CELLS = MAX_LINKS + MAX_SECTIONS;
    static constexpr u32 MAX_SAMPLE_POINTS = 128;

    SectionGraph(const FormRecipe &recipe, const GraphParams &graph, LinkRule rule);
    /// An empty piece that keeps its parent's frame; the seed type is private, so only Split can build one.
    explicit SectionGraph(const PieceSeed &seed);

    void Update(f32 time, const GraphParams &graph, const EvolutionParams &evolution, LinkRule rule);

    [[nodiscard]] f32 Distance(const f32v3 &local) const;
    [[nodiscard]] f32v3 Gradient(const f32v3 &local) const;
    [[nodiscard]] TKit::Span<const f32v3> GetSamplePoints() const;
    [[nodiscard]] f32 GetBoundingRadius() const;
    [[nodiscard]] f32 GetRestVolume() const;
    [[nodiscard]] u32 CountComponents() const;
    /// One piece per component, its sections re-centred on its rest centroid, and that centroid as its offset.
    void Split(TKit::StaticArray<SectionGraph, MAX_SECTIONS> &pieces,
               TKit::StaticArray<f32v3, MAX_SECTIONS> &offsets) const;
    [[nodiscard]] u32 GetSectionCount() const;
    [[nodiscard]] u8 GetLowestSlot() const;
    /// Appends flat-shaded world-space triangles; returns false (writing nothing) when out has too little room.
    [[nodiscard]] bool AppendTriangles(const f32m4 &transform, u32 packedColor,
                                       TKit::Span<Onyx::DynamicVertex<Onyx::D3>> out, u32 &written) const;

  private:
    struct PieceSeed
    {
        const SectionGraph &Parent;
        f32v3 Centroid;
    };

    struct Link
    {
        u8 First;
        u8 Second;
    };

    struct Pose
    {
        TKit::FixedArray<f32v3, MAX_SECTIONS> Centers;
        TKit::FixedArray<f32, MAX_SECTIONS> Scales;
        TKit::FixedArray<bool, MAX_SECTIONS> Pinched;
    };

    struct CellBounds
    {
        f32v3 Center;
        f32 Radius;
    };

    struct CellHit
    {
        u32 Cell;
        f32 Distance;
    };

    void Evolve(f32 elapsed, f32 pinchScale);
    void Relink(const GraphParams &graph, LinkRule rule);
    /// Rebuilds the cells of the current pose with their bounds, sample points and the bounding radius.
    void RebuildCells();
    void SpreadSamplePoints();
    [[nodiscard]] f32 MeasureRestVolume() const;
    /// Visits the points of every cell of a pose: one per link, then one slab per lone present section.
    template <typename Visit> void ForEachCell(const Pose &pose, Visit &&visit) const;
    /// The rolled, scaled polygons of both sections, placed halfLength before and after middle along axis.
    [[nodiscard]] TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> GatherCell(const Pose &pose, u32 first, u32 second,
                                                                              const f32v3 &middle, const f32v3 &axis,
                                                                              f32 halfLength) const;
    [[nodiscard]] TKit::Optional<CellHit> FindNearestCell(const f32v3 &local) const;
    /// Labels the components of present sections in slot order; a pinched section joins its nearest present one.
    [[nodiscard]] u32 LabelComponents(TKit::FixedArray<u8, MAX_SECTIONS> &labels) const;

    TKit::StaticArray<f32v2, MAX_PROFILE_VERTICES> m_Profile{};
    TKit::StaticArray<SectionSlot, MAX_SECTIONS> m_Sections{};
    TKit::FixedArray<f32v2, MAX_SECTIONS> m_Rolls{};
    Pose m_Pose{};
    TKit::StaticArray<Link, MAX_LINKS> m_Links{};
    TKit::FixedArray<ConvexHull, MAX_CELLS> m_Cells{};
    TKit::FixedArray<CellBounds, MAX_CELLS> m_CellBounds{};
    u32 m_CellCount = 0;
    TKit::StaticArray<f32v3, MAX_SAMPLE_POINTS> m_SamplePoints{};
    f32v3 m_RestCentroid{0.f};
    f32 m_Size;
    f32 m_ReachScale;
    bool m_ClosedChain;
    f32 m_SlabHalfDepth;
    f32 m_BoundingRadius = 0.f;
    f32 m_RestVolume = 0.f;
};
} // namespace Lim
