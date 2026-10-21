#include "lim/section_graph.hpp"

#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/limits.hpp"

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr f32 TAU = 2.f * TKit::Math::Pi<f32>();
constexpr f32 QUARTER_TURN = 0.25f;
constexpr f32 MIN_SEPARATION = 1e-6f;
constexpr f32 MIN_GRADIENT = 1e-12f;
constexpr f32 PARALLEL_LIMIT = 0.99f;
constexpr f32v3 FALLBACK_AXIS{0.f, 0.f, 1.f};
constexpr f32v3 FALLBACK_NORMAL{0.f, 1.f, 0.f};
constexpr f32v3 UP{0.f, 1.f, 0.f};
constexpr f32v3 RIGHT{1.f, 0.f, 0.f};
constexpr TKit::FixedArray<f32, 5> SINE_SERIES{-1.f / 6.f, 1.f / 120.f, -1.f / 5040.f, 1.f / 362880.f,
                                               -1.f / 39916800.f};
constexpr TKit::FixedArray<u8, ConvexHull::MAX_POINTS> VERTEX_SPREAD_ORDER{0, 8, 4, 12, 2, 10, 6, 14,
                                                                           1, 9, 5, 13, 3, 11, 7, 15};

static_assert(SectionGraph::MAX_SECTIONS == RECORD_SECTION_SLOT_COUNT);
static_assert(2 * MAX_PROFILE_VERTICES <= ConvexHull::MAX_POINTS);

struct Candidate
{
    f32 Distance;
    u8 First;
    u8 Second;
};

/// The sine of a turn count, from a fixed polynomial so every machine evolves the same shape.
[[nodiscard]] f32 TurnSine(const f32 turns)
{
    const f32 offset = turns - TKit::Math::Round(turns);
    const f32 folded = 2.f * TKit::Math::Clamp(offset, -QUARTER_TURN, QUARTER_TURN) - offset;
    const f32 angle = TAU * folded;
    const f32 squared = angle * angle;
    f32 series = 0.f;
    for (usize term = SINE_SERIES.GetSize(); term > 0; --term)
        series = (series + SINE_SERIES[term - 1]) * squared;
    return angle * (1.f + series);
}

/// Orders by distance, then by the slots of the pair, which follow section order.
void SortCandidates(TKit::StaticArray<Candidate, SectionGraph::MAX_LINKS> &candidates)
{
    for (u32 i = 1; i < candidates.GetSize(); ++i)
    {
        const Candidate candidate = candidates[i];
        u32 position = i;
        while (position > 0)
        {
            const Candidate &previous = candidates[position - 1];
            const bool isFirst = candidate.Distance < previous.Distance ||
                                 (candidate.Distance == previous.Distance &&
                                  (candidate.First < previous.First ||
                                   (candidate.First == previous.First && candidate.Second < previous.Second)));
            if (!isFirst)
                break;
            candidates[position] = previous;
            --position;
        }
        candidates[position] = candidate;
    }
}
} // namespace

SectionGraph::SectionGraph(const FormRecipe &recipe, const GraphParams &graph, const LinkRule rule)
    : m_Profile(recipe.Profile), m_Sections(recipe.Sections), m_Size(recipe.Size),
      m_ReachScale(recipe.Size * recipe.ReachBias), m_ClosedChain(recipe.ClosedChain),
      m_SlabHalfDepth(0.5f * graph.SlabDepth * recipe.Size)
{
    TKIT_ASSERT(!m_Sections.IsEmpty(), "[LIM][GRAPH] A section graph needs at least one section");
    for (u32 i = 0; i < m_Sections.GetSize(); ++i)
    {
        const f32 roll = m_Sections[i].Roll;
        m_Rolls[i] = f32v2{TurnSine(roll + QUARTER_TURN), TurnSine(roll)};
        m_RestCentroid += m_Sections[i].RestCenter;
    }
    m_RestCentroid /= f32(TKit::Math::Max(m_Sections.GetSize(), usize(1)));

    Evolve(0.f, graph.PinchScale);
    Relink(graph, rule);
    RebuildCells();
    m_RestVolume = MeasureRestVolume();
}

SectionGraph::SectionGraph(const PieceSeed &seed)
    : m_Profile(seed.Parent.m_Profile), m_RestCentroid(seed.Parent.m_RestCentroid - seed.Centroid),
      m_Size(seed.Parent.m_Size), m_ReachScale(seed.Parent.m_ReachScale), m_ClosedChain(seed.Parent.m_ClosedChain),
      m_SlabHalfDepth(seed.Parent.m_SlabHalfDepth)
{
}

void SectionGraph::Update(const f32 time, const GraphParams &graph, const EvolutionParams &evolution,
                          const LinkRule rule)
{
    m_SlabHalfDepth = 0.5f * graph.SlabDepth * m_Size;
    Evolve(time * evolution.Speed, graph.PinchScale);
    Relink(graph, rule);
    RebuildCells();
}

f32 SectionGraph::Distance(const f32v3 &local) const
{
    const TKit::Optional<CellHit> hit = FindNearestCell(local);
    return hit ? hit->Distance : TKit::Limits<f32>::Max();
}

f32v3 SectionGraph::Gradient(const f32v3 &local) const
{
    const TKit::Optional<CellHit> hit = FindNearestCell(local);
    if (hit)
        return m_Cells[hit->Cell].Gradient(local);
    if (TKit::Math::NormSquared(local) > MIN_GRADIENT)
        return TKit::Math::Normalize(local);
    return FALLBACK_NORMAL;
}

TKit::Span<const f32v3> SectionGraph::GetSamplePoints() const
{
    return m_SamplePoints;
}

f32 SectionGraph::GetBoundingRadius() const
{
    return m_BoundingRadius;
}

f32 SectionGraph::GetRestVolume() const
{
    return m_RestVolume;
}

u32 SectionGraph::CountComponents() const
{
    TKit::FixedArray<u8, MAX_SECTIONS> labels{};
    return LabelComponents(labels);
}

void SectionGraph::Split(TKit::StaticArray<SectionGraph, MAX_SECTIONS> &pieces,
                         TKit::StaticArray<f32v3, MAX_SECTIONS> &offsets) const
{
    pieces.Clear();
    offsets.Clear();
    TKit::FixedArray<u8, MAX_SECTIONS> labels{};
    const u32 pieceCount = TKit::Math::Max(LabelComponents(labels), 1U);
    for (u32 label = 0; label < pieceCount; ++label)
    {
        f32v3 centroid{0.f};
        u32 members = 0;
        for (u32 i = 0; i < m_Sections.GetSize(); ++i)
            if (labels[i] == label)
            {
                centroid += m_Sections[i].RestCenter;
                ++members;
            }
        centroid /= f32(members);

        SectionGraph &piece = pieces.Append(PieceSeed{.Parent = *this, .Centroid = centroid});
        TKit::FixedArray<u8, MAX_SECTIONS> remap{};
        for (u32 i = 0; i < m_Sections.GetSize(); ++i)
        {
            if (labels[i] != label)
                continue;
            const u32 index = u32(piece.m_Sections.GetSize());
            remap[i] = u8(index);
            piece.m_Sections.Append(m_Sections[i]).RestCenter -= centroid;
            piece.m_Rolls[index] = m_Rolls[i];
            piece.m_Pose.Centers[index] = m_Pose.Centers[i] - centroid;
            piece.m_Pose.Scales[index] = m_Pose.Scales[i];
            piece.m_Pose.Pinched[index] = m_Pose.Pinched[i];
        }
        for (const Link &link : m_Links)
            if (labels[link.First] == label)
                piece.m_Links.Append(Link{.First = remap[link.First], .Second = remap[link.Second]});

        piece.RebuildCells();
        piece.m_RestVolume = piece.MeasureRestVolume();
        offsets.Append(centroid);
    }
}

u32 SectionGraph::GetSectionCount() const
{
    return u32(m_Sections.GetSize());
}

u8 SectionGraph::GetLowestSlot() const
{
    return m_Sections[0].Slot;
}

bool SectionGraph::AppendTriangles(const f32m4 &transform, const u32 packedColor,
                                   const TKit::Span<Onyx::DynamicVertex<D3>> out, u32 &written) const
{
    u32 triangleCount = 0;
    for (u32 cell = 0; cell < m_CellCount; ++cell)
        m_Cells[cell].ForEachTriangle(
            [&triangleCount](const f32v3 &, const f32v3 &, const f32v3 &, const f32v3 &) { ++triangleCount; });
    if (written + 3 * triangleCount > out.GetSize())
        return false;

    for (u32 cell = 0; cell < m_CellCount; ++cell)
        m_Cells[cell].ForEachTriangle(
            [&](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &normal) {
                const TKit::FixedArray<f32v3, 3> corners{f32v3(transform * f32v4{first, 1.f}),
                                                         f32v3(transform * f32v4{second, 1.f}),
                                                         f32v3(transform * f32v4{third, 1.f})};
                const f32v3 worldNormal = TKit::Math::Normalize(f32v3(transform * f32v4{normal, 0.f}));
                const f32v4 tangent{TKit::Math::Normalize(corners[1] - corners[0]), 1.f};
                for (const f32v3 &corner : corners)
                    out[written++] = Onyx::DynamicVertex<D3>{.Position = corner,
                                                             .TexCoord = f32v2{0.f},
                                                             .Normal = worldNormal,
                                                             .Tangent = tangent,
                                                             .Color = packedColor};
            });
    return true;
}

void SectionGraph::Evolve(const f32 elapsed, const f32 pinchScale)
{
    for (u32 i = 0; i < m_Sections.GetSize(); ++i)
    {
        const SectionSlot &section = m_Sections[i];
        const SectionMotion &motion = section.Motion;
        const f32 wave = TurnSine(elapsed / motion.Period + motion.Phase);
        m_Pose.Centers[i] = section.RestCenter + motion.SwayDirection * (motion.Sway * wave);
        m_Pose.Scales[i] = section.RestScale * (1.f - motion.Pulse * (0.5f + 0.5f * wave));
        m_Pose.Pinched[i] = m_Pose.Scales[i] < pinchScale * section.RestScale;
    }
}

/// Keeps the surviving links first, then joins new pairs, both closest first, while each section has room for links.
void SectionGraph::Relink(const GraphParams &graph, const LinkRule rule)
{
    const u32 count = u32(m_Sections.GetSize());
    const auto isCandidate = [&](const u32 first, const u32 second) {
        if (m_Pose.Pinched[first] || m_Pose.Pinched[second])
            return false;
        if (rule == LinkRule::Proximity)
            return true;
        return second == first + 1 || (m_ClosedChain && count > 2 && first == 0 && second == count - 1);
    };

    TKit::FixedArray<u8, MAX_SECTIONS> wasLinked{};
    TKit::StaticArray<Candidate, MAX_LINKS> surviving{};
    const f32 breakReach = graph.BreakReach * m_ReachScale;
    for (const Link &link : m_Links)
    {
        wasLinked[link.First] |= u8(1U << link.Second);
        const f32 distance = TKit::Math::Norm(m_Pose.Centers[link.Second] - m_Pose.Centers[link.First]);
        if (isCandidate(link.First, link.Second) && distance <= breakReach)
            surviving.Append(Candidate{.Distance = distance, .First = link.First, .Second = link.Second});
    }

    TKit::StaticArray<Candidate, MAX_LINKS> joining{};
    const f32 joinReach = graph.JoinReach * m_ReachScale;
    for (u32 first = 0; first < count; ++first)
        for (u32 second = first + 1; second < count; ++second)
        {
            if ((wasLinked[first] & (1U << second)) != 0 || !isCandidate(first, second))
                continue;
            const f32 distance = TKit::Math::Norm(m_Pose.Centers[second] - m_Pose.Centers[first]);
            if (distance < joinReach)
                joining.Append(Candidate{.Distance = distance, .First = u8(first), .Second = u8(second)});
        }

    SortCandidates(surviving);
    SortCandidates(joining);
    m_Links.Clear();
    TKit::FixedArray<u32, MAX_SECTIONS> linkCounts{};
    const auto admit = [&](const TKit::StaticArray<Candidate, MAX_LINKS> &candidates) {
        for (const Candidate &candidate : candidates)
            if (linkCounts[candidate.First] < graph.MaxLinks && linkCounts[candidate.Second] < graph.MaxLinks)
            {
                m_Links.Append(Link{.First = candidate.First, .Second = candidate.Second});
                ++linkCounts[candidate.First];
                ++linkCounts[candidate.Second];
            }
    };
    admit(surviving);
    admit(joining);
}

void SectionGraph::RebuildCells()
{
    m_CellCount = 0;
    ForEachCell(m_Pose, [this](const TKit::Span<const f32v3> points) {
        ConvexHull &cell = m_Cells[m_CellCount];
        cell.Build(points);
        const TKit::Span<const f32v3> vertices = cell.GetVertices();
        if (vertices.IsEmpty())
            return;

        f32v3 center{0.f};
        for (const f32v3 &vertex : vertices)
            center += vertex;
        center /= f32(vertices.GetSize());
        f32 radius = 0.f;
        for (const f32v3 &vertex : vertices)
            radius = TKit::Math::Max(radius, TKit::Math::Norm(vertex - center));
        m_CellBounds[m_CellCount] = CellBounds{.Center = center, .Radius = radius};
        ++m_CellCount;
    });
    SpreadSamplePoints();

    f32 profileReach = 0.f;
    for (const f32v2 &vertex : m_Profile)
        profileReach = TKit::Math::Max(profileReach, TKit::Math::Norm(vertex));
    m_BoundingRadius = 0.f;
    for (const SectionSlot &section : m_Sections)
    {
        const f32 polygonReach = section.RestScale * profileReach;
        const f32 sway = TKit::Math::Absolute(section.Motion.Sway) * TKit::Math::Norm(section.Motion.SwayDirection);
        const f32 cellReach = TKit::Math::SquareRoot(polygonReach * polygonReach + m_SlabHalfDepth * m_SlabHalfDepth);
        m_BoundingRadius = TKit::Math::Max(m_BoundingRadius, TKit::Math::Norm(section.RestCenter) + sway + cellReach);
    }
}

/// Shares the budget evenly between cells; each takes its vertices in a spread order, then its face centres.
void SectionGraph::SpreadSamplePoints()
{
    m_SamplePoints.Clear();
    for (u32 cellIndex = 0; cellIndex < m_CellCount; ++cellIndex)
    {
        const ConvexHull &cell = m_Cells[cellIndex];
        const u32 budget = (MAX_SAMPLE_POINTS - u32(m_SamplePoints.GetSize())) / (m_CellCount - cellIndex);
        const TKit::Span<const f32v3> vertices = cell.GetVertices();
        u32 taken = 0;
        for (const u8 order : VERTEX_SPREAD_ORDER)
            if (order < vertices.GetSize() && taken < budget)
            {
                m_SamplePoints.Append(vertices[order]);
                ++taken;
            }
        if (taken == budget)
            continue;

        u32 triangleCount = 0;
        cell.ForEachTriangle(
            [&triangleCount](const f32v3 &, const f32v3 &, const f32v3 &, const f32v3 &) { ++triangleCount; });
        const u32 stride = TKit::Math::Max((triangleCount + budget - taken - 1) / (budget - taken), 1U);
        u32 triangle = 0;
        cell.ForEachTriangle([&](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &) {
            if (taken < budget && triangle % stride == 0)
            {
                m_SamplePoints.Append((first + second + third) / 3.f);
                ++taken;
            }
            ++triangle;
        });
    }
}

f32 SectionGraph::MeasureRestVolume() const
{
    Pose rest{};
    for (u32 i = 0; i < m_Sections.GetSize(); ++i)
    {
        rest.Centers[i] = m_Sections[i].RestCenter;
        rest.Scales[i] = m_Sections[i].RestScale;
    }
    ConvexHull cell{};
    f32 volume = 0.f;
    ForEachCell(rest, [&](const TKit::Span<const f32v3> points) {
        cell.Build(points);
        volume += cell.GetVolume();
    });
    return volume;
}

template <typename Visit> void SectionGraph::ForEachCell(const Pose &pose, Visit &&visit) const
{
    TKit::FixedArray<bool, MAX_SECTIONS> isLinked{};
    for (const Link &link : m_Links)
    {
        const f32v3 &first = pose.Centers[link.First];
        const f32v3 &second = pose.Centers[link.Second];
        const f32 separation = TKit::Math::Norm(second - first);
        const f32v3 axis = separation > MIN_SEPARATION ? (second - first) / separation : FALLBACK_AXIS;
        visit(GatherCell(pose, link.First, link.Second, (first + second) * 0.5f, axis,
                         TKit::Math::Max(0.5f * separation, m_SlabHalfDepth)));
        isLinked[link.First] = true;
        isLinked[link.Second] = true;
    }

    for (u32 i = 0; i < m_Sections.GetSize(); ++i)
    {
        if (isLinked[i] || pose.Pinched[i])
            continue;
        const f32v3 outward = m_Sections[i].RestCenter - m_RestCentroid;
        const f32 length = TKit::Math::Norm(outward);
        visit(GatherCell(pose, i, i, pose.Centers[i], length > MIN_SEPARATION ? outward / length : FALLBACK_AXIS,
                         m_SlabHalfDepth));
    }
}

TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> SectionGraph::GatherCell(const Pose &pose, const u32 first,
                                                                          const u32 second, const f32v3 &middle,
                                                                          const f32v3 &axis, const f32 halfLength) const
{
    const f32v3 reference = TKit::Math::Absolute(axis[1]) < PARALLEL_LIMIT ? UP : RIGHT;
    const f32v3 across = TKit::Math::Normalize(TKit::Math::Cross(axis, reference));
    const f32v3 along = TKit::Math::Cross(axis, across);
    const TKit::FixedArray<u32, 2> sections{first, second};
    const TKit::FixedArray<f32, 2> sides{-halfLength, halfLength};

    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> points{};
    for (u32 end = 0; end < 2; ++end)
    {
        const f32v2 &roll = m_Rolls[sections[end]];
        const f32 scale = pose.Scales[sections[end]];
        const f32v3 center = middle + axis * sides[end];
        for (const f32v2 &vertex : m_Profile)
        {
            const f32 acrossOffset = (vertex[0] * roll[0] - vertex[1] * roll[1]) * scale;
            const f32 alongOffset = (vertex[0] * roll[1] + vertex[1] * roll[0]) * scale;
            points.Append(center + across * acrossOffset + along * alongOffset);
        }
    }
    return points;
}

TKit::Optional<SectionGraph::CellHit> SectionGraph::FindNearestCell(const f32v3 &local) const
{
    TKit::Optional<CellHit> nearest = TKit::Optional<CellHit>::None();
    f32 best = TKit::Limits<f32>::Max();
    for (u32 cell = 0; cell < m_CellCount; ++cell)
    {
        const CellBounds &bounds = m_CellBounds[cell];
        if (TKit::Math::Norm(local - bounds.Center) - bounds.Radius >= best)
            continue;
        const f32 distance = m_Cells[cell].Distance(local);
        if (distance < best)
        {
            best = distance;
            nearest = CellHit{.Cell = cell, .Distance = distance};
        }
    }
    return nearest;
}

u32 SectionGraph::LabelComponents(TKit::FixedArray<u8, MAX_SECTIONS> &labels) const
{
    const u32 count = u32(m_Sections.GetSize());
    TKit::FixedArray<u8, MAX_SECTIONS> roots{};
    for (u32 i = 0; i < count; ++i)
        roots[i] = u8(i);
    const auto findRoot = [&roots](u32 section) {
        while (roots[section] != section)
        {
            roots[section] = roots[roots[section]];
            section = roots[section];
        }
        return section;
    };
    for (const Link &link : m_Links)
    {
        const u32 first = findRoot(link.First);
        const u32 second = findRoot(link.Second);
        roots[TKit::Math::Max(first, second)] = u8(TKit::Math::Min(first, second));
    }

    u32 components = 0;
    for (u32 i = 0; i < count; ++i)
    {
        if (m_Pose.Pinched[i])
            continue;
        const u32 root = findRoot(i);
        labels[i] = root == i ? u8(components++) : labels[root];
    }

    for (u32 i = 0; i < count; ++i)
    {
        if (!m_Pose.Pinched[i])
            continue;
        labels[i] = 0;
        f32 nearest = TKit::Limits<f32>::Max();
        for (u32 j = 0; j < count; ++j)
        {
            const f32 distance = TKit::Math::NormSquared(m_Pose.Centers[j] - m_Pose.Centers[i]);
            if (!m_Pose.Pinched[j] && distance < nearest)
            {
                nearest = distance;
                labels[i] = labels[j];
            }
        }
    }
    return components;
}
} // namespace Lim
