#include "shapes/walk_mass_builder.hpp"
#include "shapes/motion_factory.hpp"
#include "shapes/walk_mass_layout.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/debug.hpp"

namespace Lim
{
namespace
{
constexpr f32 MIN_MASS_SIZE = 1e-3f;
constexpr f32 ROD_FIT_TOLERANCE = 1e-4f;
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

/// Section 0 is the root profile; each later section adds a member until its length nibble is 0.
TKit::Scope<WalkMass> WalkMassBuilder::Build() const
{
    TKit::Scope<WalkMass> mass = TKit::Scope<WalkMass>::Create();
    Section previous = ReadSection(0);
    mass->m_Joints.Append(WalkMass::Joint{.Rest = f32m4::Identity(), .Motion = ReadMotion(0, m_Size)});

    f32 previousLength = 0.f;
    for (u32 section = 1; section < WALK_MASS_SECTION_COUNT; ++section)
    {
        const u8 lead =
            ReadByte(WALK_MASS_SECTIONS_BYTE + section * WALK_MASS_SECTION_BYTES + WALK_MASS_SECTION_LEAD_BYTE);
        const u8 lengthNibble = lead & WALK_MASS_LENGTH_MASK;
        if (lengthNibble == 0)
            break;

        const Section current = ReadSection(section);
        const f32 length = ReadLength(lengthNibble);
        mass->m_Joints.Append(WalkMass::Joint{.Rest = ComputeRest(lead, previous.Width, previousLength),
                                              .Motion = ReadMotion(section, length)});
        AddMember(*mass, previous, current, length);
        previous = current;
        previousLength = length;
    }
    if (mass->m_Joints.GetSize() == 1)
        AddSlab(*mass, previous);

    Seal(*mass);
    return mass;
}

WalkMassBuilder::Section WalkMassBuilder::ReadSection(const u32 section) const
{
    const usize offset = WALK_MASS_SECTIONS_BYTE + section * WALK_MASS_SECTION_BYTES;
    const u8 shape = ReadByte(offset + WALK_MASS_SECTION_SHAPE_BYTE);
    const u8 finish = ReadByte(offset + WALK_MASS_SECTION_FINISH_BYTE);

    const f32 widthFraction = f32(shape & WALK_MASS_WIDTH_MASK) / f32(WALK_MASS_NIBBLE_MAX);
    const f32 heightFraction = f32(shape >> WALK_MASS_HEIGHT_SHIFT) / f32(WALK_MASS_NIBBLE_MAX);
    const f32 width = m_Size * TKit::Math::LinearLerp(m_Params.WidthMin, m_Params.WidthMax, widthFraction);
    const f32 height = m_Size * TKit::Math::LinearLerp(m_Params.HeightMin, m_Params.HeightMax, heightFraction);

    const u32 chamfer = finish & WALK_MASS_CHAMFER_MASK;
    const u32 roll = (finish >> WALK_MASS_ROLL_SHIFT) & WALK_MASS_ROLL_MASK;
    const u32 taper = finish >> WALK_MASS_TAPER_SHIFT;

    Section result{.Corners = {},
                   .Width = width,
                   .Roll = ComputeUnitTurn(f32(roll) * m_Params.RollStepDegrees),
                   .Taper = TKit::Math::LinearLerp(1.f, m_Params.TaperMin, f32(taper) / f32(WALK_MASS_NIBBLE_MAX))};

    const f32v2 half{0.5f * width, 0.5f * height};
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

JointMotion WalkMassBuilder::ReadMotion(const u32 joint, const f32 length) const
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
                          .Size = m_Size};
    return MotionFactory::Get(kind).Bake(seed, m_Params);
}

/// Rounds to the nearest whole lattice step, at least one, without calling into libm.
f32 WalkMassBuilder::ReadLength(const u8 lengthNibble) const
{
    const f32 fraction = f32(lengthNibble - 1) / f32(WALK_MASS_LENGTH_STEPS);
    const f32 length = m_Size * TKit::Math::LinearLerp(m_Params.LengthMin, m_Params.LengthMax, fraction);
    const f32 steps = TKit::Math::Max(f32(u32(length / m_LatticeStep + 0.5f)), 1.f);
    return steps * m_LatticeStep;
}

/// The joint sits at the previous member's end, shifted along that member's right axis when the offset bit is set.
f32m4 WalkMassBuilder::ComputeRest(const u8 lead, const f32 previousWidth, const f32 previousLength) const
{
    const TurnBasis &turn = TURNS[(lead >> WALK_MASS_TURN_SHIFT) & WALK_MASS_TURN_MASK];
    const f32 shift = (lead & WALK_MASS_OFFSET_BIT) != 0 ? m_Params.OffsetShare * previousWidth : 0.f;
    return f32m4{f32v4{turn.Right, 0.f}, f32v4{turn.Up, 0.f}, f32v4{turn.Heading, 0.f},
                 f32v4{shift, 0.f, previousLength, 1.f}};
}

/// The hull from the previous profile, tapered, at the joint to this section's profile at the member's end.
void WalkMassBuilder::AddMember(WalkMass &mass, const Section &previous, const Section &current, const f32 length) const
{
    const JointMotion &motion = mass.m_Joints.GetBack().Motion;
    const bool splits = MotionFactory::Get(motion.Kind).SplitsMember();

    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> points{};
    AppendFace(points, previous.Corners, current.Taper, current.Roll, 0.f);
    AppendFace(points, current.Corners, 1.f, current.Roll, length);
    const ConvexHull &sleeve = AddPiece(mass, points, !splits);
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

void WalkMassBuilder::AddSlab(WalkMass &mass, const Section &root) const
{
    const f32 halfDepth = 0.5f * m_Params.SlabDepth * m_Size;
    TKit::StaticArray<f32v3, ConvexHull::MAX_POINTS> points{};
    AppendFace(points, root.Corners, 1.f, root.Roll, -halfDepth);
    AppendFace(points, root.Corners, 1.f, root.Roll, halfDepth);
    AddPiece(mass, points, true);
}

/// Every motion is a rotation about a joint or a bounded translation, so chaining rest offsets and reaches bounds it.
void WalkMassBuilder::Seal(WalkMass &mass) const
{
    TKit::FixedArray<f32, WalkMass::MAX_JOINTS> jointReach{};
    f32 chain = 0.f;
    for (u32 i = 0; i < mass.m_Joints.GetSize(); ++i)
    {
        const WalkMass::Joint &joint = mass.m_Joints[i];
        chain += TKit::Math::Norm(f32v3(joint.Rest[3]));
        jointReach[i] = chain;
        chain += joint.Motion.Reach;
    }

    for (const WalkMass::Piece &piece : mass.m_Pieces)
    {
        f32 extent = 0.f;
        for (const f32v3 &vertex : piece.Hull.GetVertices())
            extent = TKit::Math::Max(extent, TKit::Math::Norm(vertex));
        const f32 carried = piece.FollowsMotion ? mass.m_Joints[piece.Joint].Motion.Reach : 0.f;
        mass.m_BoundingRadius = TKit::Math::Max(mass.m_BoundingRadius, jointReach[piece.Joint] + carried + extent);
        piece.Hull.ForEachTriangle(
            [&mass](const f32v3 &, const f32v3 &, const f32v3 &, const f32v3 &) { mass.m_VertexCount += 3; });
    }
    mass.Evolve(0.f);
}

u8 WalkMassBuilder::ReadByte(const usize index) const
{
    return std::to_integer<u8>(m_Record[index]);
}
} // namespace Lim
