#include "lim/shape_grammar.hpp"
#include "lim/capacity.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/preprocessor/system.hpp"
#include "tkit/utils/limits.hpp"
#include "tkit/utils/logging.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
namespace
{
constexpr f32 MIN_FRACTION = 0.05f;
constexpr f32 MAX_FRACTION = 2.f;
constexpr f32 MAX_WALL_FRACTION = 0.5f;
constexpr f32 MAX_NOTCH_FRACTION = 0.45f;
constexpr f32 MAX_GROOVE_DEPTH_FRACTION = 0.5f;
constexpr f32 MIN_OPENING_MARGIN = 1.f;
constexpr f32 MAX_OPENING_MARGIN = 4.f;
constexpr f32 MIN_SMOOTH_EXTENT = 0.05f;
constexpr f32 MIN_WALL = 0.01f;
constexpr f32 OPENING_SLACK = 0.02f;
constexpr f32 MARCH_STEP = 0.02f;
constexpr f32 REACH_PER_EXTENT = 2.f;
constexpr f32 SQRT_HALF = 0.70710678f;
constexpr f32 QUARTER_TURN = 0.5f * TKit::Math::Pi<f32>();
constexpr f32 EIGHTH_TURN = 0.25f * TKit::Math::Pi<f32>();
constexpr u32 SMOOTH_SHAPE_COUNT = 4;
constexpr u32 CORE_COUNT = 5;
constexpr u32 GATE_PRIMITIVES = 3;
constexpr u32 ATTACHMENT_KINDS = 3;
constexpr u32 CARVE_KINDS = 3;
constexpr u32 QUADRANTS = 4;
constexpr u32 AXIS_X = 0;
constexpr u32 AXIS_Y = 1;
constexpr u32 AXIS_Z = 2;
constexpr u32 MAX_OPENINGS = MAX_PRIMITIVES;

using FaceFlags = u8;
enum FaceFlagBit : FaceFlags
{
    FaceFlag_X = 1U << AXIS_X,
    FaceFlag_Z = 1U << AXIS_Z
};

enum class Core : u8
{
    Monolith,
    Portal,
    Ring,
    Cage,
    Gate
};

enum class Role : u8
{
    Structure,
    Ornament
};

struct Opening
{
    f32v3 Center;
    f32v3 Axis;
};

/// The core's bounding half sizes, where attachments embed, how far they may slide and how deep notches may bite.
struct Massing
{
    Core Kind;
    f32v3 Extent;
    f32v3 FaceOffset;
    f32v3 Spread;
    f32v3 NotchLimit;
    FaceFlags AttachFaces;
    bool Trimmable;
    bool Carvable;
};

/// Decoding state of one carved form; Wall is the module every length snaps to, Clearance the opening radius to build.
struct Grammar
{
    ByteReader &Reader;
    const ShapeParams &Params;
    f32 Size;
    f32 Wall;
    f32 Clearance;
    ShapeRecipe Recipe;
    TKit::StaticArray<Role, MAX_PRIMITIVES> Roles{};
    TKit::StaticArray<Opening, MAX_OPENINGS> Openings{};
};

void OrderFractions(f32 &min, f32 &max)
{
    const f32 low = TKit::Math::Clamp(TKit::Math::Min(min, max), MIN_FRACTION, MAX_FRACTION);
    const f32 high = TKit::Math::Clamp(TKit::Math::Max(min, max), MIN_FRACTION, MAX_FRACTION);
    min = low;
    max = high;
}

[[nodiscard]] f32v3 SmoothExtent(const ShapeParams &params, const BlobPrimitive::Shape shape)
{
    switch (shape)
    {
    case BlobPrimitive::Shape::Box:
        return f32v3{params.SmoothBoxHalfWidth, params.SmoothBoxHalfHeight, params.SmoothBoxHalfDepth};
    case BlobPrimitive::Shape::Capsule:
        return f32v3{params.SmoothCapsuleRadius, params.SmoothCapsuleHalfLength, 0.f};
    case BlobPrimitive::Shape::Torus:
        return f32v3{params.SmoothTorusMajorRadius, params.SmoothTorusMinorRadius, 0.f};
    default:
        return f32v3{params.SmoothSphereRadius, 0.f, 0.f};
    }
}

[[nodiscard]] ShapeRecipe DecodeSmooth(ByteReader &reader, const ShapeRequest &request, const ShapeParams &params)
{
    ShapeRecipe recipe{.Primitives = {}, .BlendRadius = request.BlendRadius};
    const u32 count =
        TKit::Math::Clamp(reader.NextCount(request.MinPrimitives, request.MaxPrimitives), 1U, MAX_PRIMITIVES);
    for (u32 i = 0; i < count; ++i)
    {
        const f32 size = reader.NextRange(request.MinSize, request.MaxSize);
        const BlobPrimitive::Shape shape = BlobPrimitive::Shape(reader.NextIndex(SMOOTH_SHAPE_COUNT));
        const f32 spread = reader.NextUnit() * params.SmoothSpread * size;
        const f32v3 offset = reader.NextDirection() * spread;
        const f32q rotation = reader.NextRotation();
        recipe.Primitives.Append(BlobPrimitive{
            .Kind = shape, .Center = offset, .Extent = SmoothExtent(params, shape) * size, .Rotation = rotation});
    }
    return recipe;
}

[[nodiscard]] f32 Quantize(const f32 length, const f32 step)
{
    return TKit::Math::Max(TKit::Math::Round(length, step), step);
}

[[nodiscard]] f32 Measure(Grammar &grammar, const f32 minFraction, const f32 maxFraction)
{
    return Quantize(grammar.Reader.NextRange(minFraction, maxFraction) * grammar.Size, grammar.Wall);
}

[[nodiscard]] f32 Place(Grammar &grammar, const f32 limit)
{
    const f32 offset = TKit::Math::Round(grammar.Reader.NextRange(-limit, limit), grammar.Wall);
    return TKit::Math::Clamp(offset, -limit, limit);
}

[[nodiscard]] f32 PickSign(ByteReader &reader)
{
    return reader.NextIndex(2) == 0 ? 1.f : -1.f;
}

[[nodiscard]] f32v3 AxisVector(const u32 axis)
{
    f32v3 vector{0.f};
    vector[axis] = 1.f;
    return vector;
}

/// Turns a primitive whose own axis is local Y so that it runs along the given canonical axis.
[[nodiscard]] f32q AxisRotation(const u32 axis)
{
    if (axis == AXIS_X)
        return f32q::FromAngleAxis(-QUARTER_TURN, AxisVector(AXIS_Z));
    if (axis == AXIS_Z)
        return f32q::FromAngleAxis(QUARTER_TURN, AxisVector(AXIS_X));
    return Onyx::RotType<Onyx::D3>::Identity;
}

void Add(Grammar &grammar, const BlobPrimitive &primitive, const Role role)
{
    if (grammar.Recipe.Primitives.IsFull())
        return;
    grammar.Recipe.Primitives.Append(primitive);
    grammar.Roles.Append(role);
}

[[nodiscard]] Core PickCore(Grammar &grammar, const u32 budget)
{
    const ShapeParams &params = grammar.Params;
    const TKit::FixedArray<f32, CORE_COUNT> weights{params.MonolithWeight, params.PortalWeight, params.RingWeight,
                                                    params.CageWeight,
                                                    budget >= GATE_PRIMITIVES ? params.GateWeight : 0.f};
    f32 total = 0.f;
    for (const f32 weight : weights)
        total += weight;

    f32 pick = grammar.Reader.NextUnit() * total;
    for (u32 i = 0; i < CORE_COUNT; ++i)
    {
        if (pick < weights[i])
            return Core(i);
        pick -= weights[i];
    }
    return Core::Monolith;
}

[[nodiscard]] Massing BuildMonolith(Grammar &grammar)
{
    const ShapeParams &params = grammar.Params;
    const bool tower = grammar.Reader.NextIndex(2) == 0;
    const f32 width = tower ? Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction)
                            : Measure(grammar, params.MinSpanFraction, params.MaxSpanFraction);
    const f32 depth = Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction);
    f32 height = Measure(grammar, params.MinSpanFraction, params.MaxSpanFraction);
    if (tower)
        height += Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction);

    const f32v3 extent{width, depth, height};
    Add(grammar, BlobPrimitive{.Kind = BlobPrimitive::Shape::Box, .Center = f32v3{0.f}, .Extent = extent},
        Role::Structure);
    return Massing{.Kind = Core::Monolith,
                   .Extent = extent,
                   .FaceOffset = extent,
                   .Spread = f32v3{1.f},
                   .NotchLimit = extent * (2.f * params.NotchFraction),
                   .AttachFaces = FaceFlag_X | FaceFlag_Z,
                   .Trimmable = true,
                   .Carvable = true};
}

[[nodiscard]] Massing BuildPortal(Grammar &grammar)
{
    const ShapeParams &params = grammar.Params;
    const f32 width =
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance);
    const f32 height =
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance);
    const f32 depth = Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction);

    const f32v3 extent{width + grammar.Wall, depth, height + grammar.Wall};
    Add(grammar,
        BlobPrimitive{
            .Kind = BlobPrimitive::Shape::Frame, .Center = f32v3{0.f}, .Extent = extent, .Wall = grammar.Wall},
        Role::Structure);
    grammar.Openings.Append(Opening{.Center = f32v3{0.f}, .Axis = AxisVector(AXIS_Y)});
    return Massing{.Kind = Core::Portal,
                   .Extent = extent,
                   .FaceOffset = extent,
                   .Spread = f32v3{1.f},
                   .NotchLimit = f32v3{0.5f * grammar.Wall, 2.f * params.NotchFraction * depth, 0.5f * grammar.Wall},
                   .AttachFaces = FaceFlag_X | FaceFlag_Z,
                   .Trimmable = true,
                   .Carvable = true};
}

[[nodiscard]] Massing BuildRing(Grammar &grammar)
{
    const ShapeParams &params = grammar.Params;
    const f32 inner =
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance);
    const f32 depth = Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction);
    const f32 outer = inner + grammar.Wall;

    Add(grammar,
        BlobPrimitive{.Kind = BlobPrimitive::Shape::Ring,
                      .Center = f32v3{0.f},
                      .Extent = f32v3{outer, depth, 0.f},
                      .Wall = grammar.Wall},
        Role::Structure);
    grammar.Openings.Append(Opening{.Center = f32v3{0.f}, .Axis = AxisVector(AXIS_Y)});
    const f32v3 extent{outer, depth, outer};
    return Massing{.Kind = Core::Ring,
                   .Extent = extent,
                   .FaceOffset = extent,
                   .Spread = f32v3{0.f, 1.f, 0.f},
                   .NotchLimit = f32v3{0.5f * grammar.Wall, 2.f * params.NotchFraction * depth, 0.5f * grammar.Wall},
                   .AttachFaces = FaceFlag_X | FaceFlag_Z,
                   .Trimmable = false,
                   .Carvable = true};
}

[[nodiscard]] Massing BuildCage(Grammar &grammar)
{
    const ShapeParams &params = grammar.Params;
    const f32 wall = grammar.Wall;
    const f32v3 extent{
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance) + wall,
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance) + wall,
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance) +
            wall};

    Add(grammar,
        BlobPrimitive{
            .Kind = BlobPrimitive::Shape::Lattice, .Center = f32v3{0.f}, .Extent = extent, .Wall = grammar.Wall},
        Role::Structure);
    grammar.Openings.Append(Opening{.Center = f32v3{0.f}, .Axis = AxisVector(AXIS_Y)});
    grammar.Openings.Append(Opening{.Center = f32v3{0.f}, .Axis = AxisVector(AXIS_X)});
    return Massing{.Kind = Core::Cage,
                   .Extent = extent,
                   .FaceOffset = extent,
                   .Spread = f32v3{0.f},
                   .NotchLimit = f32v3{0.f},
                   .AttachFaces = FaceFlag_Z,
                   .Trimmable = false,
                   .Carvable = false};
}

[[nodiscard]] Massing BuildGate(Grammar &grammar, const u32 budget)
{
    const ShapeParams &params = grammar.Params;
    const f32 gap =
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance);
    const f32 passage =
        TKit::Math::Max(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction), grammar.Clearance);
    const f32 pillar = Measure(grammar, params.WallFraction, params.MaxDepthFraction);
    const f32 lintel = Measure(grammar, params.WallFraction, params.MaxDepthFraction);
    const f32 depth = Measure(grammar, params.MinDepthFraction, params.MaxDepthFraction);
    const f32 overhang = grammar.Wall * f32(grammar.Reader.NextIndex(2));
    const bool prism = grammar.Reader.NextIndex(2) == 0;
    const f32 rise = passage + 2.f * lintel;
    const f32 span = gap + 2.f * pillar;

    for (const f32 side : TKit::FixedArray<f32, 2>{-1.f, 1.f})
    {
        const f32v3 center{side * (gap + pillar), 0.f, 0.f};
        Add(grammar,
            prism ? BlobPrimitive{.Kind = BlobPrimitive::Shape::Prism,
                                  .Center = center,
                                  .Extent = f32v3{pillar, rise, 0.f},
                                  .Rotation = AxisRotation(AXIS_Z)}
                  : BlobPrimitive{.Kind = BlobPrimitive::Shape::Box,
                                  .Center = center,
                                  .Extent = f32v3{pillar, depth, rise}},
            Role::Structure);
    }

    const f32v3 beam{span + overhang, depth, lintel};
    Add(grammar,
        BlobPrimitive{.Kind = BlobPrimitive::Shape::Box, .Center = f32v3{0.f, 0.f, passage + lintel}, .Extent = beam},
        Role::Structure);
    const bool based = budget > GATE_PRIMITIVES && grammar.Reader.NextIndex(2) == 0;
    if (based)
        Add(grammar,
            BlobPrimitive{
                .Kind = BlobPrimitive::Shape::Box, .Center = f32v3{0.f, 0.f, -passage - lintel}, .Extent = beam},
            Role::Structure);

    grammar.Openings.Append(Opening{.Center = f32v3{0.f}, .Axis = AxisVector(AXIS_Y)});
    return Massing{.Kind = Core::Gate,
                   .Extent = f32v3{span + overhang, depth, rise},
                   .FaceOffset = f32v3{span, depth, rise},
                   .Spread = f32v3{1.f},
                   .NotchLimit = f32v3{0.5f * grammar.Wall, 2.f * params.NotchFraction * depth, 0.5f * grammar.Wall},
                   .AttachFaces = based ? FaceFlags(FaceFlag_X | FaceFlag_Z) : FaceFlags(FaceFlag_X),
                   .Trimmable = true,
                   .Carvable = true};
}

[[nodiscard]] Massing BuildCore(Grammar &grammar, const u32 budget)
{
    switch (PickCore(grammar, budget))
    {
    case Core::Monolith:
        return BuildMonolith(grammar);
    case Core::Portal:
        return BuildPortal(grammar);
    case Core::Ring:
        return BuildRing(grammar);
    case Core::Cage:
        return BuildCage(grammar);
    case Core::Gate:
        return BuildGate(grammar, budget);
    }
    TKIT_UNREACHABLE();
}

/// Intersects the core with a 45 degree box (chamfer) or a drum about the passage axis, cutting one module off corners.
[[nodiscard]] BlobPrimitive DecodeTrim(Grammar &grammar, const Massing &massing)
{
    const f32v3 &extent = massing.Extent;
    const f32 length = extent[AXIS_Y] + grammar.Wall;
    if (grammar.Reader.NextIndex(2) == 0)
    {
        const f32 half = (extent[AXIS_X] + extent[AXIS_Z] - grammar.Wall) * SQRT_HALF;
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box,
                             .Operation = BlobPrimitive::Op::Intersect,
                             .Center = f32v3{0.f},
                             .Extent = f32v3{half, length, half},
                             .Rotation = f32q::FromAngleAxis(EIGHTH_TURN, AxisVector(AXIS_Y))};
    }
    const f32 radius =
        TKit::Math::Norm(f32v2{extent[AXIS_X] - grammar.Wall, extent[AXIS_Z] - grammar.Wall}) + grammar.Wall;
    return BlobPrimitive{.Kind = BlobPrimitive::Shape::Cylinder,
                         .Operation = BlobPrimitive::Op::Intersect,
                         .Center = f32v3{0.f},
                         .Extent = f32v3{radius, length, 0.f}};
}

/// A bar, block or buttress embedded half a module into a side face, never reaching into a passage.
[[nodiscard]] BlobPrimitive DecodeAttachment(Grammar &grammar, const Massing &massing)
{
    ByteReader &reader = grammar.Reader;
    const f32 wall = grammar.Wall;
    u32 axis = massing.AttachFaces & FaceFlag_X ? AXIS_X : AXIS_Z;
    if (massing.AttachFaces == (FaceFlag_X | FaceFlag_Z) && reader.NextIndex(2) == 0)
        axis = AXIS_Z;
    const f32 side = PickSign(reader);
    const u32 run = axis == AXIS_X ? AXIS_Z : AXIS_X;
    const f32 protrusion =
        Quantize(reader.NextRange(wall, TKit::Math::Max(grammar.Params.AttachmentReach * grammar.Size, wall)), wall);
    const f32 halfDepth = 0.5f * (protrusion + 0.5f * wall);

    f32v3 center{0.f};
    center[axis] = side * (massing.FaceOffset[axis] - 0.5f * wall + halfDepth);
    f32v3 extent{0.f};
    extent[axis] = halfDepth;

    if (massing.Kind == Core::Cage)
    {
        extent[AXIS_X] = massing.Extent[AXIS_X] + TKit::Math::Round(reader.NextRange(0.f, protrusion), wall);
        extent[AXIS_Y] = massing.Extent[AXIS_Y] + TKit::Math::Round(reader.NextRange(0.f, protrusion), wall);
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box, .Center = center, .Extent = extent};
    }

    center[AXIS_Y] = Place(grammar, massing.Spread[AXIS_Y] * massing.Extent[AXIS_Y]);
    center[run] = Place(grammar, massing.Spread[run] * massing.Extent[run]);
    switch (reader.NextIndex(ATTACHMENT_KINDS))
    {
    case 0: {
        const u32 along = reader.NextIndex(2) == 0 ? AXIS_Y : run;
        const f32 length = Quantize(reader.NextRange(wall, massing.Extent[along] + protrusion), wall);
        if (reader.NextIndex(2) == 0)
            return BlobPrimitive{.Kind = BlobPrimitive::Shape::Prism,
                                 .Center = center,
                                 .Extent = f32v3{halfDepth, length, 0.f},
                                 .Rotation = AxisRotation(along)};
        extent[along] = length;
        extent[along == AXIS_Y ? run : AXIS_Y] = 0.5f * wall;
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box, .Center = center, .Extent = extent};
    }
    case 1:
        extent[AXIS_Y] = Quantize(reader.NextRange(wall, TKit::Math::Max(massing.Extent[AXIS_Y], wall)), wall);
        extent[run] = Quantize(reader.NextRange(wall, TKit::Math::Max(massing.Extent[run], wall)), wall);
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box, .Center = center, .Extent = extent};
    default:
        break;
    }

    const f32 runSign = PickSign(reader);
    extent[run] = Quantize(reader.NextRange(wall, TKit::Math::Max(massing.Extent[run], wall)), wall);
    extent[AXIS_Y] = 0.5f * wall;
    f32v3 corner{0.f};
    corner[axis] = -side;
    corner[run] = -runSign;

    f32q rotation = Onyx::RotType<Onyx::D3>::Identity;
    f32 alignment = -TKIT_F32_MAX;
    for (u32 quadrant = 0; quadrant < QUADRANTS; ++quadrant)
    {
        const f32q turn = f32q::FromAngleAxis(f32(quadrant) * QUARTER_TURN, AxisVector(AXIS_Y));
        const f32 candidate = TKit::Math::Dot(turn * f32v3{-1.f, 0.f, -1.f}, corner);
        if (candidate > alignment)
        {
            alignment = candidate;
            rotation = turn;
        }
    }
    return BlobPrimitive{.Kind = BlobPrimitive::Shape::Wedge,
                         .Center = center,
                         .Extent = TKit::Math::Absolute(TKit::Math::Conjugate(rotation) * extent),
                         .Rotation = rotation};
}

/// A through window (monolith only, registered as an opening), a surface groove that never cuts through, or a notch.
[[nodiscard]] BlobPrimitive DecodeCarve(Grammar &grammar, const Massing &massing)
{
    ByteReader &reader = grammar.Reader;
    const ShapeParams &params = grammar.Params;
    const f32 wall = grammar.Wall;
    const f32v3 &extent = massing.Extent;
    const u32 kind = reader.NextIndex(CARVE_KINDS);

    const f32 roomX = extent[AXIS_X] - wall;
    const f32 roomZ = extent[AXIS_Z] - wall;
    if (kind == 0 && massing.Kind == Core::Monolith && roomX >= grammar.Clearance && roomZ >= grammar.Clearance)
    {
        const f32 width = TKit::Math::Clamp(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction),
                                            grammar.Clearance, roomX);
        const f32 height = TKit::Math::Clamp(Measure(grammar, params.MinWindowFraction, params.MaxWindowFraction),
                                             grammar.Clearance, roomZ);
        const f32v3 center{Place(grammar, roomX - width), 0.f, Place(grammar, roomZ - height)};
        grammar.Openings.Append(Opening{.Center = center, .Axis = AxisVector(AXIS_Y)});
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box,
                             .Operation = BlobPrimitive::Op::Subtract,
                             .Center = center,
                             .Extent = f32v3{width, extent[AXIS_Y] + 2.f * grammar.Clearance, height}};
    }

    if (kind == 2)
    {
        const f32 face = PickSign(reader);
        const u32 run = reader.NextIndex(2) == 0 ? AXIS_X : AXIS_Z;
        const u32 across = run == AXIS_X ? AXIS_Z : AXIS_X;
        f32v3 center{0.f};
        center[AXIS_Y] = face * extent[AXIS_Y];
        center[across] = Place(grammar, TKit::Math::Max(extent[across] - wall, 0.f));
        f32v3 groove{0.f};
        groove[run] = extent[run] + wall;
        groove[across] = 0.5f * wall;
        groove[AXIS_Y] = params.GrooveDepthFraction * extent[AXIS_Y];
        return BlobPrimitive{.Kind = BlobPrimitive::Shape::Box,
                             .Operation = BlobPrimitive::Op::Subtract,
                             .Center = center,
                             .Extent = groove};
    }

    f32v3 center{PickSign(reader) * extent[AXIS_X], PickSign(reader) * extent[AXIS_Y],
                 PickSign(reader) * extent[AXIS_Z]};
    if (massing.Kind == Core::Ring)
        center[reader.NextIndex(2) == 0 ? AXIS_X : AXIS_Z] = 0.f;
    f32v3 notch;
    for (u32 axis = 0; axis < 3; ++axis)
        notch[axis] = Quantize(reader.NextUnit() * massing.NotchLimit[axis], 0.5f * wall);
    return BlobPrimitive{
        .Kind = BlobPrimitive::Shape::Box, .Operation = BlobPrimitive::Op::Subtract, .Center = center, .Extent = notch};
}

/// The first point on an opening's passage line where the field leaves less than the required clearance.
[[nodiscard]] TKit::Optional<f32v3> FindBlockage(const Grammar &grammar, const f32 required)
{
    const ShapeRecipe &recipe = grammar.Recipe;
    f32 reach = 0.f;
    for (const BlobPrimitive &primitive : recipe.Primitives)
        reach = TKit::Math::Max(reach, TKit::Math::Norm(primitive.Center) +
                                           REACH_PER_EXTENT * TKit::Math::Norm(primitive.Extent));

    for (const Opening &opening : grammar.Openings)
    {
        if (Blob::Evaluate(recipe.Primitives, recipe.BlendRadius, opening.Center) < required)
            return opening.Center;
        const f32 span = reach + TKit::Math::Norm(opening.Center);
        for (f32 along = -span; along <= span;)
        {
            const f32v3 point = opening.Center + opening.Axis * along;
            const f32 clearance = Blob::Evaluate(recipe.Primitives, recipe.BlendRadius, point);
            if (clearance < required)
                return point;
            along += TKit::Math::Max(clearance - required, MARCH_STEP);
        }
    }
    return TKit::Optional<f32v3>::None();
}

[[nodiscard]] bool RemoveNearestOrnament(Grammar &grammar, const f32v3 &point)
{
    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> &primitives = grammar.Recipe.Primitives;
    TKit::Optional<u32> nearest = TKit::Optional<u32>::None();
    f32 nearestDistance = TKIT_F32_MAX;
    for (u32 i = 0; i < primitives.GetSize(); ++i)
    {
        const BlobPrimitive &primitive = primitives[i];
        if (grammar.Roles[i] != Role::Ornament || primitive.Operation != BlobPrimitive::Op::Union)
            continue;
        const f32 distance = Blob::Evaluate(TKit::Span<const BlobPrimitive>{&primitive, 1}, 0.f, point);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = i;
        }
    }
    if (!nearest)
        return false;
    primitives.RemoveOrdered(primitives.begin() + *nearest);
    grammar.Roles.RemoveOrdered(grammar.Roles.begin() + *nearest);
    return true;
}

void SecurePassages(Grammar &grammar, const f32 required)
{
    for (u32 attempt = 0; attempt < MAX_PRIMITIVES; ++attempt)
    {
        const TKit::Optional<f32v3> blockage = FindBlockage(grammar, required);
        if (!blockage)
            return;
        if (!RemoveNearestOrnament(grammar, *blockage))
            break;
    }
    TKIT_LOG_WARNING("[LIM][SHAPE] An opening of a carved shape stays narrower than {}", 2.f * required);
}

[[nodiscard]] ShapeRecipe DecodeCarved(ByteReader &reader, const ShapeRequest &request, const ShapeParams &params)
{
    const u32 budget =
        TKit::Math::Clamp(reader.NextCount(request.MinPrimitives, request.MaxPrimitives), 1U, MAX_PRIMITIVES);
    const f32 size = reader.NextRange(request.MinSize, request.MaxSize);
    const f32 blendRadius = TKit::Math::Max(request.BlendRadius, 0.f) * params.HardBlendScale;
    const f32 required = 0.5f * TKit::Math::Max(request.MinOpening, 0.f);
    const f32 clearance = required + blendRadius + OPENING_SLACK;

    Grammar grammar{.Reader = reader,
                    .Params = params,
                    .Size = size,
                    .Wall = TKit::Math::Max(params.WallFraction * TKit::Math::Max(size, clearance), MIN_WALL),
                    .Clearance = clearance,
                    .Recipe = ShapeRecipe{.Primitives = {}, .BlendRadius = blendRadius}};
    const Massing massing = BuildCore(grammar, budget);

    u32 remaining = budget - TKit::Math::Min(budget, u32(grammar.Recipe.Primitives.GetSize()));
    if (remaining != 0 && massing.Trimmable && reader.NextChance(params.TrimChance))
    {
        Add(grammar, DecodeTrim(grammar, massing), Role::Structure);
        --remaining;
    }

    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> attachments{};
    TKit::StaticArray<BlobPrimitive, MAX_PRIMITIVES> carves{};
    for (u32 i = 0; i < remaining; ++i)
    {
        if (massing.Carvable && reader.NextChance(params.CarveChance))
            carves.Append(DecodeCarve(grammar, massing));
        else
            attachments.Append(DecodeAttachment(grammar, massing));
    }
    for (const BlobPrimitive &attachment : attachments)
        Add(grammar, attachment, Role::Ornament);
    for (const BlobPrimitive &carve : carves)
        Add(grammar, carve, Role::Ornament);

    SecurePassages(grammar, required);
    return grammar.Recipe;
}
} // namespace

ShapeRecipe DecodeShape(ByteReader &reader, const ShapeRequest &request, const ShapeParams &params)
{
    if (!request.Carved)
        return DecodeSmooth(reader, request, params);
    return DecodeCarved(reader, request, params);
}

ShapeParams Sanitize(ShapeParams params)
{
    params.CarveChance = TKit::Math::Clamp(params.CarveChance, 0.f, 1.f);
    params.OpeningMargin = TKit::Math::Clamp(params.OpeningMargin, MIN_OPENING_MARGIN, MAX_OPENING_MARGIN);
    params.HardBlendScale = TKit::Math::Clamp(params.HardBlendScale, 0.f, 1.f);
    params.TrimChance = TKit::Math::Clamp(params.TrimChance, 0.f, 1.f);
    params.MonolithWeight = TKit::Math::Max(params.MonolithWeight, 0.f);
    params.PortalWeight = TKit::Math::Max(params.PortalWeight, 0.f);
    params.RingWeight = TKit::Math::Max(params.RingWeight, 0.f);
    params.CageWeight = TKit::Math::Max(params.CageWeight, 0.f);
    params.GateWeight = TKit::Math::Max(params.GateWeight, 0.f);
    params.WallFraction = TKit::Math::Clamp(params.WallFraction, MIN_FRACTION, MAX_WALL_FRACTION);
    OrderFractions(params.MinSpanFraction, params.MaxSpanFraction);
    OrderFractions(params.MinDepthFraction, params.MaxDepthFraction);
    OrderFractions(params.MinWindowFraction, params.MaxWindowFraction);
    params.AttachmentReach = TKit::Math::Clamp(params.AttachmentReach, 0.f, MAX_FRACTION);
    params.NotchFraction = TKit::Math::Clamp(params.NotchFraction, 0.f, MAX_NOTCH_FRACTION);
    params.GrooveDepthFraction = TKit::Math::Clamp(params.GrooveDepthFraction, 0.f, MAX_GROOVE_DEPTH_FRACTION);
    params.SmoothSpread = TKit::Math::Clamp(params.SmoothSpread, 0.f, MAX_FRACTION);
    params.SmoothSphereRadius = TKit::Math::Clamp(params.SmoothSphereRadius, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothBoxHalfWidth = TKit::Math::Clamp(params.SmoothBoxHalfWidth, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothBoxHalfHeight = TKit::Math::Clamp(params.SmoothBoxHalfHeight, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothBoxHalfDepth = TKit::Math::Clamp(params.SmoothBoxHalfDepth, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothCapsuleRadius = TKit::Math::Clamp(params.SmoothCapsuleRadius, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothCapsuleHalfLength = TKit::Math::Clamp(params.SmoothCapsuleHalfLength, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothTorusMajorRadius = TKit::Math::Clamp(params.SmoothTorusMajorRadius, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    params.SmoothTorusMinorRadius = TKit::Math::Clamp(params.SmoothTorusMinorRadius, MIN_SMOOTH_EXTENT, MAX_FRACTION);
    return params;
}

ShapeLookParams Sanitize(ShapeLookParams params)
{
    params.MeshResolution = TKit::Math::Clamp(params.MeshResolution, MIN_MESH_RESOLUTION, MAX_MESH_RESOLUTION);
    return params;
}
} // namespace Lim
