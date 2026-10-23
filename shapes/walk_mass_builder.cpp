#include "shapes/walk_mass_builder.hpp"
#include "shapes/motion_factory.hpp"
#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/utils/limits.hpp"

namespace Lim
{
namespace
{
constexpr f32 MIN_MASS_SIZE = 1e-3f;
constexpr f32 ROD_FIT_TOLERANCE = 1e-4f;
constexpr f32 SLICE_TOLERANCE = 1e-4f;
constexpr u32 SHEAR_SAMPLES = 64;
constexpr u32 MAX_POLYGON_CORNERS = 3 * ConvexHull::MAX_TRIANGLES + ConvexHull::MAX_POINTS;
constexpr f32 ROD_SHRINK = 0.8f;
constexpr u32 ROD_FIT_ATTEMPTS = 8;
constexpr u32 SERIES_TERMS = 12;
constexpr f64 HALF_TURN_DEGREES = 180.0;
constexpr f64 FULL_TURN_DEGREES = 360.0;

constexpr f32v3 RIGHT{1.f, 0.f, 0.f};
constexpr f32v3 LEFT{-1.f, 0.f, 0.f};
constexpr f32v3 UP{0.f, 1.f, 0.f};
constexpr f32v3 DOWN{0.f, -1.f, 0.f};
constexpr f32v3 HEADING{0.f, 0.f, 1.f};
constexpr f32v3 BACK{0.f, 0.f, -1.f};

constexpr TKit::FixedArray<f32v2, 4> CORNER_SIGNS{f32v2{1.f, 1.f}, f32v2{-1.f, 1.f}, f32v2{-1.f, -1.f},
                                                  f32v2{1.f, -1.f}};

/// A member's frame in its parent's frame; every turn is a quarter turn, so the axes stay exact.
struct TurnBasis
{
    f32v3 Right;
    f32v3 Up;
    f32v3 Heading;
};

constexpr TKit::FixedArray<TurnBasis, WALK_MASS_TURN_CODES> BuildTurns()
{
    TKit::FixedArray<TurnBasis, WALK_MASS_TURN_CODES> turns{};
    for (TurnBasis &turn : turns)
        turn = TurnBasis{.Right = RIGHT, .Up = UP, .Heading = HEADING};
    turns[WALK_MASS_TURN_LEFT] = TurnBasis{.Right = HEADING, .Up = UP, .Heading = LEFT};
    turns[WALK_MASS_TURN_RIGHT] = TurnBasis{.Right = BACK, .Up = UP, .Heading = RIGHT};
    turns[WALK_MASS_TURN_UP] = TurnBasis{.Right = RIGHT, .Up = BACK, .Heading = UP};
    turns[WALK_MASS_TURN_DOWN] = TurnBasis{.Right = RIGHT, .Up = HEADING, .Heading = DOWN};
    return turns;
}

constexpr TKit::FixedArray<TurnBasis, WALK_MASS_TURN_CODES> TURNS = BuildTurns();

constexpr TKit::FixedArray<u32, 5> FALLBACK_TURNS{0, WALK_MASS_TURN_LEFT, WALK_MASS_TURN_RIGHT, WALK_MASS_TURN_UP,
                                                  WALK_MASS_TURN_DOWN};
constexpr u32 MAX_CANDIDATES = 2 + FALLBACK_TURNS.GetSize();
constexpr u32 MAX_PLACEMENT_TESTS = 1024;

using Polygon = TKit::StaticArray<f32v2, MAX_POLYGON_CORNERS>;

struct Candidate
{
    u32 Turn;
    bool IsOffset;
};

/// Cosine and sine of an angle in degrees by Taylor series in double precision, never calling into libm.
[[nodiscard]] f32v2 ComputeUnitTurn(const f32 degrees)
{
    f64 reduced = f64(degrees);
    while (reduced > HALF_TURN_DEGREES)
        reduced -= FULL_TURN_DEGREES;
    while (reduced < -HALF_TURN_DEGREES)
        reduced += FULL_TURN_DEGREES;

    const f64 angle = reduced * (TKit::Math::Pi<f64>() / HALF_TURN_DEGREES);
    const f64 square = angle * angle;
    f64 cosineTerm = 1.0;
    f64 sineTerm = angle;
    f64 cosine = cosineTerm;
    f64 sine = sineTerm;
    for (u32 i = 1; i <= SERIES_TERMS; ++i)
    {
        cosineTerm *= -square / f64((2 * i - 1) * (2 * i));
        sineTerm *= -square / f64((2 * i) * (2 * i + 1));
        cosine += cosineTerm;
        sine += sineTerm;
    }
    return f32v2{f32(cosine), f32(sine)};
}

void AppendFace(TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> &points, const TKit::Span<const f32v2> corners,
                const f32 scale, const f32v2 &roll, const f32 z)
{
    for (const f32v2 &corner : corners)
    {
        const f32v2 scaled = corner * scale;
        points.Append(f32v3{scaled[0] * roll[0] - scaled[1] * roll[1], scaled[0] * roll[1] + scaled[1] * roll[0], z});
    }
}
[[nodiscard]] f32 CrossProduct(const f32v2 &first, const f32v2 &second)
{
    return first[0] * second[1] - first[1] * second[0];
}

/// Andrew's monotone chain: the counter-clockwise convex hull of a few points in the plane.
[[nodiscard]] Polygon WrapPolygon(Polygon points)
{
    for (u32 i = 1; i < points.GetSize(); ++i)
    {
        const f32v2 key = points[i];
        u32 j = i;
        while (j > 0 && (points[j - 1][0] > key[0] || (points[j - 1][0] == key[0] && points[j - 1][1] > key[1])))
        {
            points[j] = points[j - 1];
            --j;
        }
        points[j] = key;
    }

    Polygon hull{};
    for (u32 pass = 0; pass < 2; ++pass)
    {
        const u32 start = u32(hull.GetSize());
        for (u32 step = 0; step < points.GetSize(); ++step)
        {
            const f32v2 &point = points[pass == 0 ? step : points.GetSize() - 1 - step];
            while (hull.GetSize() >= start + 2 && CrossProduct(hull[hull.GetSize() - 1] - hull[hull.GetSize() - 2],
                                                               point - hull[hull.GetSize() - 2]) <= 0.f)
                hull.Pop();
            hull.Append(point);
        }
        if (!hull.IsEmpty())
            hull.Pop();
    }
    return hull;
}

/// Sutherland-Hodgman: the part of a polygon inside a counter-clockwise convex one.
[[nodiscard]] Polygon ClipPolygon(const Polygon &subject, const Polygon &clip)
{
    Polygon output = subject;
    for (u32 edge = 0; edge < clip.GetSize() && !output.IsEmpty(); ++edge)
    {
        const f32v2 &from = clip[edge];
        const f32v2 side = clip[(edge + 1) % clip.GetSize()] - from;
        const Polygon input = output;
        output.Clear();
        for (u32 i = 0; i < input.GetSize(); ++i)
        {
            const f32v2 &current = input[i];
            const f32v2 &before = input[(i + input.GetSize() - 1) % input.GetSize()];
            const f32 currentSide = CrossProduct(side, current - from);
            const f32 beforeSide = CrossProduct(side, before - from);
            if ((currentSide >= 0.f) != (beforeSide >= 0.f))
                output.Append(before + (current - before) * (beforeSide / (beforeSide - currentSide)));
            if (currentSide >= 0.f)
                output.Append(current);
        }
    }
    return output;
}

[[nodiscard]] f32 MeasureExtent(const Polygon &polygon, const u32 axis)
{
    if (polygon.IsEmpty())
        return 0.f;
    f32 low = polygon[0][axis];
    f32 high = low;
    for (const f32v2 &point : polygon)
    {
        low = TKit::Math::Min(low, point[axis]);
        high = TKit::Math::Max(high, point[axis]);
    }
    return high - low;
}

/// How far along the axis the start face, shifted along it, still lies on the parent's slice; none without a slice.
[[nodiscard]] f32 MeasureContact(const Polygon &face, const Polygon &slice, const f32 shift, const u32 axis)
{
    if (slice.GetSize() < 3)
        return 0.f;
    Polygon moved = face;
    for (f32v2 &point : moved)
        point[axis] += shift;
    return MeasureExtent(ClipPolygon(moved, slice), axis);
}
} // namespace

WalkMassBuilder::WalkMassBuilder(const TKit::Span<const std::byte> record, const WalkMassParams &params,
                                 const TKit::Optional<f32> sizeOverride)
    : m_Record(record), m_Params(params)
{
    TKIT_ASSERT(record.GetSize() >= WALK_MASS_RECORD_BYTES, "[LIM][SHAPES] A walk mass record takes {} bytes, got {}",
                WALK_MASS_RECORD_BYTES, record.GetSize());
    const f32 sizeFraction = f32(ReadByte(WALK_MASS_SIZE_BYTE)) / f32(WALK_MASS_BYTE_MAX);
    const f32 size =
        sizeOverride ? *sizeOverride : TKit::Math::LinearLerp(m_Params.SizeMin, m_Params.SizeMax, sizeFraction);
    m_Size = TKit::Math::Max(size, MIN_MASS_SIZE);

    const f32 latticeFraction = f32(ReadByte(WALK_MASS_LATTICE_BYTE)) / f32(WALK_MASS_BYTE_MAX);
    m_LatticeStep = m_Size * TKit::Math::LinearLerp(m_Params.LatticeStepMin, m_Params.LatticeStepMax, latticeFraction);
}

/// Section 0 is the plinth profile; each later section adds a plate until its length nibble is 0 past MinMembers.
TKit::Scope<WalkMass> WalkMassBuilder::Build() const
{
    TKit::Scope<WalkMass> mass = TKit::Scope<WalkMass>::Create();
    const Section root = ReadSection(0, 0.f);
    const f32v2 unbounded{TKit::Limits<f32>::Max()};
    mass->m_Joints.Append(WalkMass::Joint{.Rest = f32m4::Identity(), .Motion = ReadMotion(0, m_Size, unbounded)});

    Spine spine{.Frames = {}, .Previous = root, .PreviousLength = 0.f, .RootBreadth = root.Breadth};
    spine.Frames[0] = f32m4::Identity();
    u32 budget = MAX_PLACEMENT_TESTS;
    Grow(*mass, spine, 1, budget);

    Seal(*mass);
    return mass;
}

/// The root reads the plinth ranges; members read the plate ranges and never fall below a share of the root breadth.
WalkMassBuilder::Section WalkMassBuilder::ReadSection(const u32 section, const f32 rootBreadth) const
{
    const usize offset = WALK_MASS_SECTIONS_BYTE + section * WALK_MASS_SECTION_BYTES;
    const u8 shape = ReadByte(offset + WALK_MASS_SECTION_SHAPE_BYTE);
    const u8 finish = ReadByte(offset + WALK_MASS_SECTION_FINISH_BYTE);

    const bool isRoot = section == 0;
    const f32 breadthFraction = f32(shape & WALK_MASS_BREADTH_MASK) / f32(WALK_MASS_NIBBLE_MAX);
    const f32 thicknessFraction = f32(shape >> WALK_MASS_THICKNESS_SHIFT) / f32(WALK_MASS_NIBBLE_MAX);
    const f32 breadthShare =
        isRoot ? TKit::Math::LinearLerp(m_Params.PlinthBreadthMin, m_Params.PlinthBreadthMax, breadthFraction)
               : TKit::Math::LinearLerp(m_Params.BreadthMin, m_Params.BreadthMax, breadthFraction);
    const f32 thicknessShare =
        isRoot ? TKit::Math::LinearLerp(m_Params.PlinthThicknessMin, m_Params.PlinthThicknessMax, thicknessFraction)
               : TKit::Math::LinearLerp(m_Params.ThicknessMin, m_Params.ThicknessMax, thicknessFraction);
    const f32 breadth = TKit::Math::Max(m_Size * breadthShare, m_Params.MinBreadthShare * rootBreadth);
    const f32 thickness = m_Size * thicknessShare;

    const u32 chamfer = finish & WALK_MASS_CHAMFER_MASK;
    const u32 roll = (finish >> WALK_MASS_ROLL_SHIFT) & WALK_MASS_ROLL_MASK;
    const u32 taper = finish >> WALK_MASS_TAPER_SHIFT;

    Section result{.Corners = {},
                   .Breadth = breadth,
                   .Roll = ComputeUnitTurn(f32(roll) * m_Params.RollStepDegrees),
                   .Taper = TKit::Math::LinearLerp(1.f, m_Params.TaperMin, f32(taper) / f32(WALK_MASS_NIBBLE_MAX))};

    const f32v2 half{0.5f * breadth, 0.5f * thickness};
    const f32 cut =
        f32(chamfer) / f32(WALK_MASS_CHAMFER_LEVELS) * m_Params.ChamferMax * TKit::Math::Min(half[0], half[1]);
    for (const f32v2 &sign : CORNER_SIGNS)
    {
        if (chamfer == 0)
        {
            result.Corners.Append(sign * half);
            continue;
        }
        result.Corners.Append(sign * f32v2{half[0] - cut, half[1]});
        result.Corners.Append(sign * f32v2{half[0], half[1] - cut});
    }
    return result;
}

JointMotion WalkMassBuilder::ReadMotion(const u32 joint, const f32 length, const f32v2 &shearReach) const
{
    const usize offset = WALK_MASS_JOINTS_BYTE + joint * WALK_MASS_JOINT_BYTES;
    const u8 drive = ReadByte(offset + WALK_MASS_JOINT_DRIVE_BYTE);
    const u8 cadence = ReadByte(offset + WALK_MASS_JOINT_CADENCE_BYTE);
    const u8 detail = ReadByte(offset + WALK_MASS_JOINT_DETAIL_BYTE);

    const u8 amplitude = u8(drive >> WALK_MASS_AMPLITUDE_SHIFT);
    const MotionKind kind = amplitude == 0 ? MotionKind::Still : MotionKind(drive & WALK_MASS_MOTION_MASK);
    const f32 periodFraction = f32(cadence & WALK_MASS_PERIOD_MASK) / f32(WALK_MASS_NIBBLE_MAX);
    const f32 holdFraction = f32((drive >> WALK_MASS_HOLD_SHIFT) & WALK_MASS_HOLD_MASK) / f32(WALK_MASS_HOLD_LEVELS);
    const MotionTiming timing{.Period = TKit::Math::LinearLerp(m_Params.PeriodMin, m_Params.PeriodMax, periodFraction),
                              .Phase = f32(cadence >> WALK_MASS_PHASE_SHIFT) / f32(WALK_MASS_PHASE_STEPS),
                              .Hold = TKit::Math::LinearLerp(m_Params.HoldMin, m_Params.HoldMax, holdFraction)};

    const MotionSeed seed{.Timing = timing,
                          .Amplitude = amplitude,
                          .StepNibble = u8(detail & WALK_MASS_STEP_MASK),
                          .AxisCode = u8((detail >> WALK_MASS_AXIS_SHIFT) & WALK_MASS_AXIS_MASK),
                          .Length = length,
                          .Size = m_Size,
                          .ShearReach = shearReach};
    return MotionFactory::Get(kind).Bake(seed, m_Params);
}

/// Tries the decoded turn, then plainer ones for a kept member, longest first; a kept member that blocks is retried.
bool WalkMassBuilder::Grow(WalkMass &mass, const Spine &spine, const u32 section, u32 &budget) const
{
    if (section == WALK_MASS_SECTION_COUNT)
        return true;
    const u8 lead = ReadByte(WALK_MASS_SECTIONS_BYTE + section * WALK_MASS_SECTION_BYTES + WALK_MASS_SECTION_LEAD_BYTE);
    const bool isForced = section <= m_Params.MinMembers;
    if ((lead & WALK_MASS_LENGTH_MASK) == 0 && !isForced)
        return true;

    TKit::StaticArray<Candidate, MAX_CANDIDATES> candidates{};
    candidates.Append(Candidate{.Turn = u32((lead >> WALK_MASS_TURN_SHIFT) & WALK_MASS_TURN_MASK),
                                .IsOffset = (lead & WALK_MASS_OFFSET_BIT) != 0});
    if (isForced)
    {
        candidates.Append(Candidate{.Turn = candidates[0].Turn, .IsOffset = false});
        for (const u32 turn : FALLBACK_TURNS)
            candidates.Append(Candidate{.Turn = turn, .IsOffset = false});
    }

    const Section current = ReadSection(section, spine.RootBreadth);
    const u32 steps = CountLengthSteps(lead & WALK_MASS_LENGTH_MASK, spine.Previous, current);
    ConvexHull hull{};
    for (const Candidate &candidate : candidates)
    {
        const f32m4 rest = ComputeRest(candidate.Turn, candidate.IsOffset, spine.Previous, spine.PreviousLength);
        const f32m4 frame = spine.Frames[section - 1] * rest;
        const u32 clearance = CountClearanceSteps(mass, section, rest);
        for (u32 step = TKit::Math::Max(steps, clearance); step >= clearance && budget > 0; --step)
        {
            --budget;
            const f32 length = f32(step) * m_LatticeStep;
            hull.Build(CollectMemberCorners(spine.Previous, current, length));
            if (HasRestContact(mass, spine, section, hull, frame))
                continue;

            const usize pieceCount = mass.m_Pieces.GetSize();
            AddMember(mass, spine, current, rest, length, section);
            Spine next = spine;
            next.Frames[section] = frame;
            next.Previous = current;
            next.PreviousLength = length;
            if (Grow(mass, next, section + 1, budget))
                return true;
            mass.m_Joints.Pop();
            while (mass.m_Pieces.GetSize() > pieceCount)
                mass.m_Pieces.Pop();
        }
    }
    return !isForced;
}

/// Steps a member needs to end beyond its parent plate along its own heading, so the next member clears that plate.
u32 WalkMassBuilder::CountClearanceSteps(const WalkMass &mass, const u32 section, const f32m4 &rest) const
{
    if (section == 1)
        return 1;
    u32 parent = 0;
    while (mass.m_Pieces[parent].Joint != section - 1)
        ++parent;

    const f32v3 origin = f32v3(rest[3]);
    const f32v3 heading = f32v3(rest[2]);
    f32 reach = 0.f;
    for (const f32v3 &vertex : mass.m_Pieces[parent].Hull.GetVertices())
        reach = TKit::Math::Max(reach, TKit::Math::Dot(heading, vertex - origin));
    return TKit::Math::Max(u32(reach / m_LatticeStep) + 1, 1U);
}

/// Snaps to the nearest lattice step, then caps at LengthToBreadthMax of the narrower face, floored, at least one.
u32 WalkMassBuilder::CountLengthSteps(const u8 lengthNibble, const Section &previous, const Section &current) const
{
    if (lengthNibble == 0)
        return 1;
    const f32 fraction = f32(lengthNibble - 1) / f32(WALK_MASS_LENGTH_STEPS);
    const f32 length = m_Size * TKit::Math::LinearLerp(m_Params.LengthMin, m_Params.LengthMax, fraction);
    const u32 snapped = u32(length / m_LatticeStep + 0.5f);
    const f32 narrowest = TKit::Math::Min(previous.Breadth * current.Taper, current.Breadth);
    const u32 capped = u32(m_Params.LengthToBreadthMax * narrowest / m_LatticeStep);
    return TKit::Math::Max(TKit::Math::Min(snapped, capped), 1U);
}

/// The joint sits at the previous member's end, shifted along its right axis by a share of the rolled end face's span.
f32m4 WalkMassBuilder::ComputeRest(const u32 turn, const bool isOffset, const Section &previous,
                                   const f32 previousLength) const
{
    const TurnBasis &basis = TURNS[turn];
    f32 shift = 0.f;
    if (isOffset)
    {
        TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> face{};
        AppendFace(face, previous.Corners, 1.f, previous.Roll, 0.f);
        f32 span = 0.f;
        for (const f32v3 &corner : face)
            span = TKit::Math::Max(span, 2.f * corner[0]);
        shift = m_Params.OffsetShare * span;
    }
    return f32m4{f32v4{basis.Right, 0.f}, f32v4{basis.Up, 0.f}, f32v4{basis.Heading, 0.f},
                 f32v4{shift, 0.f, previousLength, 1.f}};
}

TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> WalkMassBuilder::CollectMemberCorners(const Section &previous,
                                                                                       const Section &current,
                                                                                       const f32 length)
{
    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> corners{};
    AppendFace(corners, previous.Corners, current.Taper, current.Roll, 0.f);
    AppendFace(corners, current.Corners, 1.f, current.Roll, length);
    return corners;
}

/// Pieces two or more members back are the only ones a new member is not joined to.
bool WalkMassBuilder::HasRestContact(const WalkMass &mass, const Spine &spine, const u32 section,
                                     const ConvexHull &hull, const f32m4 &frame) const
{
    const f32v3 center = f32v3(frame * f32v4{hull.GetCenter(), 1.f});
    for (const WalkMass::Piece &piece : mass.m_Pieces)
    {
        if (piece.Joint + 2 > section)
            continue;
        const f32m4 &pieceFrame = spine.Frames[piece.Joint];
        const f32v3 pieceCenter = f32v3(pieceFrame * f32v4{piece.Hull.GetCenter(), 1.f});
        const f32 reach = hull.GetRadius() + piece.Hull.GetRadius();
        if (TKit::Math::NormSquared(center - pieceCenter) > reach * reach)
            continue;
        if (hull.IsOverlapping(frame, piece.Hull, pieceFrame))
            return true;
    }
    return false;
}

/// Shears only as far as MinContactShare of the start face's extent stays on the parent's slice by the start plane.
f32v2 WalkMassBuilder::ComputeShearReach(const WalkMass &mass, const Section &previous, const Section &current,
                                         const f32m4 &rest, const u32 section) const
{
    if (section == 1)
        return f32v2{TKit::Limits<f32>::Max()};

    u32 parent = 0;
    while (mass.m_Pieces[parent].Joint != section - 1)
        ++parent;

    const f32v3 origin = f32v3(rest[3]);
    const auto toChild = [&](const f32v3 &point) {
        const f32v3 offset = point - origin;
        return f32v3{TKit::Math::Dot(f32v3(rest[0]), offset), TKit::Math::Dot(f32v3(rest[1]), offset),
                     TKit::Math::Dot(f32v3(rest[2]), offset)};
    };
    const f32 tolerance = SLICE_TOLERANCE * m_Size;
    Polygon slice{};
    mass.m_Pieces[parent].Hull.ForEachTriangle(
        [&](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &) {
            const TKit::FixedArray<f32v3, 3> corners{toChild(first), toChild(second), toChild(third)};
            for (u32 i = 0; i < 3; ++i)
            {
                const f32v3 &from = corners[i];
                const f32v3 &to = corners[(i + 1) % 3];
                if (TKit::Math::Absolute(from[2]) <= tolerance)
                    slice.Append(f32v2{from[0], from[1]});
                else if (from[2] * to[2] < 0.f)
                {
                    const f32v3 crossing = from + (to - from) * (from[2] / (from[2] - to[2]));
                    slice.Append(f32v2{crossing[0], crossing[1]});
                }
            }
        });
    slice = WrapPolygon(slice);

    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> corners{};
    AppendFace(corners, previous.Corners, current.Taper, current.Roll, 0.f);
    Polygon face{};
    for (const f32v3 &corner : corners)
        face.Append(f32v2{corner[0], corner[1]});
    face = WrapPolygon(face);

    const f32 longest = m_Params.SlideReachMax * m_Size;
    f32v2 reach{0.f};
    for (u32 axis = 0; axis < 2; ++axis)
    {
        const f32 kept = m_Params.MinContactShare * MeasureExtent(face, axis);
        for (u32 sample = 0; sample <= SHEAR_SAMPLES; ++sample)
        {
            const f32 shift = longest * f32(sample) / f32(SHEAR_SAMPLES);
            if (MeasureContact(face, slice, shift, axis) < kept)
                break;
            reach[axis] = shift;
        }
    }
    return reach;
}

/// Appends the member's joint, whose shear is capped by its contact with the parent, then its sleeve and any rod.
void WalkMassBuilder::AddMember(WalkMass &mass, const Spine &spine, const Section &current, const f32m4 &rest,
                                const f32 length, const u32 section) const
{
    const f32v2 shearReach = ComputeShearReach(mass, spine.Previous, current, rest, section);
    const JointMotion &motion =
        mass.m_Joints.Append(WalkMass::Joint{.Rest = rest, .Motion = ReadMotion(section, length, shearReach)}).Motion;
    const bool splits = MotionFactory::Get(motion.Kind).SplitsMember();

    const ConvexHull &sleeve = AddPiece(mass, CollectMemberCorners(spine.Previous, current, length), !splits);
    if (splits)
        AddRod(mass, sleeve, current, length, motion.Reach);
}

const ConvexHull &WalkMassBuilder::AddPiece(WalkMass &mass, const TKit::Span<const f32v3> points,
                                            const bool followsMotion)
{
    WalkMass::Piece &piece = mass.m_Pieces.Append();
    piece.Joint = u8(mass.m_Joints.GetSize() - 1);
    piece.FollowsMotion = followsMotion;
    piece.Hull.Build(points);
    return piece.Hull;
}

/// The rod ends at the member's end and shrinks its cross-section until the sleeve holds every corner at rest.
void WalkMassBuilder::AddRod(WalkMass &mass, const ConvexHull &sleeve, const Section &current, const f32 length,
                             const f32 reach) const
{
    const f32 rodLength = TKit::Math::Min(reach + m_Params.RodOverlap * length, length);
    const f32 tolerance = ROD_FIT_TOLERANCE * m_Size;
    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> points{};
    f32 scale = m_Params.RodScale;
    for (u32 attempt = 0; attempt < ROD_FIT_ATTEMPTS; ++attempt)
    {
        points.Clear();
        AppendFace(points, current.Corners, scale, current.Roll, length - rodLength);
        AppendFace(points, current.Corners, scale, current.Roll, length);
        bool isHidden = true;
        for (const f32v3 &point : points)
            isHidden = isHidden && sleeve.Distance(point) <= tolerance;
        if (isHidden)
            break;
        scale *= ROD_SHRINK;
    }
    AddPiece(mass, points, true);
}

/// Bakes what no pose changes; the radius walks each vertex out: a turning joint swings it, a sliding one adds reach.
void WalkMassBuilder::Seal(WalkMass &mass) const
{
    TKit::FixedArray<f32m4, WalkMass::MAX_JOINTS> frames{};
    frames[0] = mass.m_Joints[0].Rest;
    for (u32 i = 1; i < mass.m_Joints.GetSize(); ++i)
        frames[i] = frames[i - 1] * mass.m_Joints[i].Rest;

    for (u32 i = 0; i < mass.m_Pieces.GetSize(); ++i)
    {
        const WalkMass::Piece &piece = mass.m_Pieces[i];
        const bool isRod = i > 0 && mass.m_Pieces[i - 1].Joint == piece.Joint;
        if (!isRod)
            mass.m_Volume += piece.Hull.GetVolume();
        for (const f32v3 &vertex : piece.Hull.GetVertices())
        {
            f32v3 anchor = f32v3(frames[piece.Joint] * f32v4{vertex, 1.f});
            f32 reach = 0.f;
            for (u32 joint = piece.Joint + 1; joint > 0; --joint)
            {
                const u32 index = joint - 1;
                if (index == piece.Joint && !piece.FollowsMotion)
                    continue;
                const JointMotion &motion = mass.m_Joints[index].Motion;
                if (motion.Step != 0.f)
                {
                    const f32v3 origin = f32v3(frames[index][3]);
                    reach += TKit::Math::Norm(anchor - origin);
                    anchor = origin;
                }
                reach += motion.Reach;
            }
            mass.m_BoundingRadius = TKit::Math::Max(mass.m_BoundingRadius, reach + TKit::Math::Norm(anchor));
        }
        piece.Hull.ForEachTriangle(
            [&mass](const f32v3 &, const f32v3 &, const f32v3 &, const f32v3 &) { mass.m_VertexCount += 3; });
    }
    GatherSamples(mass);
    mass.Reset();
    mass.Evolve(0.f);
}

/// Hull vertices first, then triangle centres in the slots left, each thinned evenly when it overflows.
void WalkMassBuilder::GatherSamples(WalkMass &mass)
{
    TKit::StaticArray<WalkMass::SampleSource, WalkMass::MAX_PIECES * ConvexHull::MAX_POINTS> corners{};
    TKit::StaticArray<WalkMass::SampleSource, WalkMass::MAX_PIECES * ConvexHull::MAX_TRIANGLES> centres{};
    for (u32 i = 0; i < mass.m_Pieces.GetSize(); ++i)
    {
        const WalkMass::Piece &piece = mass.m_Pieces[i];
        for (const f32v3 &vertex : piece.Hull.GetVertices())
            corners.Append(WalkMass::SampleSource{.Local = vertex, .Piece = u8(i)});
        piece.Hull.ForEachTriangle([&](const f32v3 &first, const f32v3 &second, const f32v3 &third, const f32v3 &) {
            centres.Append(WalkMass::SampleSource{.Local = (first + second + third) / 3.f, .Piece = u8(i)});
        });
    }

    const auto take = [&mass](const TKit::Span<const WalkMass::SampleSource> sources) {
        const usize count =
            TKit::Math::Min(sources.GetSize(), Shape::MAX_SAMPLE_POINTS - mass.m_SampleSources.GetSize());
        for (usize i = 0; i < count; ++i)
            mass.m_SampleSources.Append(sources[i * sources.GetSize() / count]);
    };
    take(corners);
    take(centres);
    mass.m_Samples.Resize(mass.m_SampleSources.GetSize());
}

u8 WalkMassBuilder::ReadByte(const usize index) const
{
    return std::to_integer<u8>(m_Record[index]);
}
} // namespace Lim
