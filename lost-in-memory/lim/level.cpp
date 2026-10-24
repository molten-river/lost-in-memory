#include "lim/level.hpp"

#include "shapes/shape_factory.hpp"
#include "shapes/shape_instance.hpp"
#include "shapes/shape_kind.hpp"
#include "shapes/walk_mass_layout.hpp"

#include "onyx/resources.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/optional.hpp"
#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr f32 SHELL_CONTACT_EPSILON = 0.05f;
constexpr f32 BYTE_MAX = f32(WALK_MASS_BYTE_MAX);
constexpr f32 NIBBLE_MAX = f32(WALK_MASS_NIBBLE_MAX);
constexpr f32 TAG_MAX = BYTE_MAX - 1.f;

static_assert(RECORD_BYTES == WALK_MASS_RECORD_BYTES, "A record must hold exactly one walk mass");
static_assert(GENOME_BYTES + RECORD_COUNT * RECORD_BYTES == REGION_BYTES, "The genome and records fill the region");
static_assert(RECORD_COUNT <= ShapeRenderer::MAX_SHAPES, "The renderer must hold every record");

struct RecordDrift
{
    f32 MinSpeed;
    f32 MaxSpeed;
    f32 TumbleSpeed;
};

[[nodiscard]] u8 ReadByte(const TKit::Span<const std::byte> bytes, const usize index)
{
    return std::to_integer<u8>(bytes[index]);
}

[[nodiscard]] f32 ReadFraction(const TKit::Span<const std::byte> bytes, const usize index)
{
    return f32(ReadByte(bytes, index)) / BYTE_MAX;
}

/// A unit direction from an azimuth in turns and a height fraction, uniform over the sphere for uniform inputs.
[[nodiscard]] f32v3 ToDirection(const f32 turns, const f32 height)
{
    const f32 z = 2.f * height - 1.f;
    const f32 ring = TKit::Math::SquareRoot(TKit::Math::Max(1.f - z * z, 0.f));
    const f32 azimuth = 2.f * TKit::Math::Pi() * turns;
    return f32v3{ring * TKit::Math::Cosine(azimuth), ring * TKit::Math::Sine(azimuth), z};
}

[[nodiscard]] Onyx::Color ComputeTint(const Level::FormKind kind, const u32 tag, const u32 lethalTagMax,
                                      const Palette &colors)
{
    if (kind == Level::FormKind::Goal)
        return colors.GoalGreen;
    if (kind == Level::FormKind::Enemy)
        return Mix(colors.EnemyPurple, colors.EnemyRed, f32(tag - 1) / TKit::Math::Max(f32(lethalTagMax) - 1.f, 1.f));
    return Mix(colors.SolidLow, colors.SolidHigh,
               f32(tag - lethalTagMax - 1) / TKit::Math::Max(TAG_MAX - f32(lethalTagMax), 1.f));
}

[[nodiscard]] RecordDrift SelectDrift(const Level::FormKind kind, const MechanicsParams &mechanics)
{
    if (kind == Level::FormKind::Goal)
        return RecordDrift{.MinSpeed = mechanics.Goal.DriftSpeed,
                           .MaxSpeed = mechanics.Goal.DriftSpeed,
                           .TumbleSpeed = mechanics.Enemies.TumbleSpeed};
    if (kind == Level::FormKind::Enemy)
        return RecordDrift{.MinSpeed = mechanics.Enemies.MinDriftSpeed,
                           .MaxSpeed = mechanics.Enemies.MaxDriftSpeed,
                           .TumbleSpeed = mechanics.Enemies.TumbleSpeed};
    return RecordDrift{.MinSpeed = mechanics.Solids.MinDriftSpeed,
                       .MaxSpeed = mechanics.Solids.MaxDriftSpeed,
                       .TumbleSpeed = mechanics.Solids.TumbleSpeed};
}
} // namespace

Level::Level() : m_Material(Onyx::Resources::RegisterMaterial<D3>())
{
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);
}

void Level::Generate(const Region &region, const LevelParams &params, const ShapeKitParams &kit, const PlayMode mode)
{
    const Palette &colors = params.LookFeel.Colors;
    const ShellParams &shell = params.Mechanics.Shell;

    m_Forms.Clear();
    m_Mode = mode;
    m_Time = 0.f;
    m_Outcome = Outcome::Playing;
    m_Physics = params.Mechanics.Physics;
    m_TestBench = params.Mechanics.TestBench;
    m_Light = params.LookFeel.Light;
    m_AmbientTint = colors.Ambient;
    m_CoreTint = colors.CoreLight;
    m_LightRadius = shell.Radius * m_Light.CoreRadiusScale;

    const bool hasShell = mode != PlayMode::TestShapes;
    ByteReader reader(region.GetBytes());
    if (hasShell)
    {
        m_Shell.Build(shell, params.LookFeel.ShellMotion, colors, params.Flags.ShellVertexNoise);
        DecodeLethalPanels(reader, shell.LethalDensity, colors);
    }

    if ((mode == PlayMode::TestSphere || !SpawnRecords(region, params, kit)) && hasShell)
        PlaceShellGoal(reader, colors.GoalGreen);
    RewriteShapes();

    if (hasShell)
        m_Shell.Update(m_Time);
}

void Level::Update(const f32 deltaTime, Player &player)
{
    if (m_Outcome != Outcome::Playing)
        return;

    const bool hasShell = m_Mode != PlayMode::TestShapes;
    m_Time += deltaTime;
    if (hasShell)
        m_Shell.Update(m_Time);
    for (Form &form : m_Forms)
        form.RigidBody.Evolve(m_Time);
    Simulate(deltaTime);
    RewriteShapes();

    const f32 radius = player.GetRadius();
    for (const Form &form : m_Forms)
    {
        if (hasShell && form.Kind != FormKind::Solid)
            continue;
        const TKit::Optional<Body::Penetration> penetration = form.RigidBody.Penetrate(player.GetPosition(), radius);
        if (penetration)
            player.PushOut(penetration->Normal, penetration->Depth);
    }
    if (!hasShell)
        return;

    const f32v3 position = player.GetPosition();
    const f32 distance = TKit::Math::Norm(position);
    if (distance > 0.f)
        player.ConfineToRadius(m_Shell.GetRadius(position / distance));

    m_Outcome = DetectOutcome(player.GetPosition(), radius);
}

/// The kit shapes carry their tints in their vertices, so they draw before the shell sets its fill colour.
void Level::Draw(Onyx::RenderContext<D3> &context, const f32 brightness) const
{
    const f32 scale = TKit::Math::Clamp(brightness, 0.f, 1.f);

    context.PushClear();
    context.NoClip();
    context.Material(m_Material);
    context.RenderFlags(Onyx::RenderModeFlag_Shaded);
    context.SetAmbientLight(m_AmbientTint);
    context.SetAmbientIntensity(m_Light.AmbientIntensity * scale);
    context.PointLight(Onyx::PointLightParameters<D3>{.Position = f32v3{0.f},
                                                      .Tint = m_CoreTint,
                                                      .LightRadius = m_LightRadius,
                                                      .Intensity = m_Light.CoreIntensity * scale});

    m_Renderer.Draw(context);
    if (m_Mode != PlayMode::TestShapes)
        m_Shell.Draw(context);
    context.Pop();
}

Level::Outcome Level::GetOutcome() const
{
    return m_Outcome;
}

void Level::DecodeLethalPanels(ByteReader &reader, const f32 density, const Palette &colors)
{
    for (u32 triangle = 0; triangle < m_Shell.GetTriangleCount(); ++triangle)
    {
        if (!reader.NextChance(density))
            continue;
        m_Shell.SetPanel(triangle, Shell::Panel::Lethal, Mix(colors.EnemyPurple, colors.EnemyRed, reader.NextUnit()));
    }
}

void Level::PlaceShellGoal(ByteReader &reader, const Onyx::Color &tint)
{
    const u32 count = m_Shell.GetTriangleCount();
    u32 triangle = reader.NextIndex(count);
    for (u32 step = 0; step < count && m_Shell.GetPanel(triangle) == Shell::Panel::Lethal; ++step)
        triangle = (triangle + 1) % count;
    m_Shell.SetPanel(triangle, Shell::Panel::Goal, tint);
}

/// The genome's goal byte picks a record; the scan walks past empty records to the first one with a tag.
bool Level::SpawnRecords(const Region &region, const LevelParams &params, const ShapeKitParams &kit)
{
    const TKit::Span<const std::byte> bytes = region.GetBytes();
    const auto readRecord = [&bytes](const u32 index) {
        return TKit::Span<const std::byte>(bytes.GetData() + GENOME_BYTES + index * RECORD_BYTES, RECORD_BYTES);
    };

    TKit::Optional<u32> goal{};
    const u32 seed = ReadByte(bytes, GENOME_GOAL_BYTE);
    for (u32 step = 0; step < RECORD_COUNT && !goal; ++step)
        if (ReadByte(readRecord((seed + step) % u32(RECORD_COUNT)), RECORD_TAG_BYTE) != 0)
            goal = (seed + step) % u32(RECORD_COUNT);

    const u32 lethalTagMax = params.Mechanics.Records.LethalTagMax;
    for (u32 index = 0; index < RECORD_COUNT; ++index)
    {
        if (m_Mode == PlayMode::TestShapes && index >= m_TestBench.GridCount)
            break;
        const TKit::Span<const std::byte> record = readRecord(index);
        const u8 tag = ReadByte(record, RECORD_TAG_BYTE);
        if (tag == 0)
            continue;
        const FormKind kind = goal && *goal == index ? FormKind::Goal
                              : tag <= lethalTagMax  ? FormKind::Enemy
                                                     : FormKind::Solid;
        SpawnRecord(record, index, kind, params, kit);
    }
    return goal;
}

/// Bytes 7 to 14 place the body; one that reaches too far for the shell is rebaked once at a size that fits.
void Level::SpawnRecord(const TKit::Span<const std::byte> record, const u32 index, const FormKind kind,
                        const LevelParams &params, const ShapeKitParams &kit)
{
    const MechanicsParams &mechanics = params.Mechanics;
    const PhysicsParams &physics = mechanics.Physics;
    const u32 tint =
        ComputeTint(kind, ReadByte(record, RECORD_TAG_BYTE), mechanics.Records.LethalTagMax, params.LookFeel.Colors)
            .ToLinear()
            .Pack();

    TKit::Scope<Shape> shape = ShapeFactory::Create(ShapeKind::WalkMass, record, kit);
    if (m_Mode == PlayMode::TestShapes)
    {
        Form &form = m_Forms.Append(Form{.RigidBody = Body(std::move(shape), ComputeGridMotion(index), physics.Density),
                                         .Kind = kind,
                                         .PackedTint = tint});
        form.RigidBody.Anchor();
        return;
    }

    const f32 maxReach = mechanics.Records.MaxFormRadiusShare * mechanics.Shell.Radius;
    if (shape->GetBoundingRadius() > maxReach)
    {
        const f32 size = TKit::Math::LinearLerp(kit.WalkMass.SizeMin, kit.WalkMass.SizeMax,
                                                ReadFraction(record, WALK_MASS_SIZE_BYTE));
        shape = ShapeFactory::Create(ShapeKind::WalkMass, record, kit, size * maxReach / shape->GetBoundingRadius());
    }

    const RecordDrift drift = SelectDrift(kind, mechanics);
    const f32 bounding = shape->GetBoundingRadius();
    const f32 safeDistance =
        TKit::Math::Max(physics.MinSafeSpawnRadius, physics.SafeSpawnFraction * mechanics.Shell.Radius);
    const f32 farthest = TKit::Math::Max(m_Shell.GetMinRadius() - bounding - physics.WallMargin, 0.f);
    const f32 nearest =
        TKit::Math::Min(kind == FormKind::Goal ? mechanics.Goal.MinSpawnDistance : safeDistance + bounding, farthest);
    const f32 distance = TKit::Math::LinearLerp(nearest, farthest, ReadFraction(record, RECORD_SPAWN_DISTANCE_BYTE));
    const f32 speed = TKit::Math::LinearLerp(drift.MinSpeed, drift.MaxSpeed, ReadFraction(record, RECORD_SPEED_BYTE));
    const f32 tumbleScale = TKit::Math::LinearLerp(physics.MinTumbleScale, physics.MaxTumbleScale,
                                                   ReadFraction(record, RECORD_TUMBLE_SCALE_BYTE));
    const u8 tumble = ReadByte(record, RECORD_TUMBLE_AXIS_BYTE);
    const f32v3 tumbleAxis = ToDirection(f32(tumble >> 4) / (NIBBLE_MAX + 1.f), f32(tumble & 0x0F) / NIBBLE_MAX);

    const Body::Motion motion{.Position = ToDirection(ReadFraction(record, RECORD_SPAWN_AZIMUTH_BYTE),
                                                      ReadFraction(record, RECORD_SPAWN_Z_BYTE)) *
                                          distance,
                              .Orientation = Onyx::RotType<D3>::Identity,
                              .Velocity = ToDirection(ReadFraction(record, RECORD_HEADING_AZIMUTH_BYTE),
                                                      ReadFraction(record, RECORD_HEADING_Z_BYTE)) *
                                          speed,
                              .AngularVelocity = tumbleAxis * (drift.TumbleSpeed * tumbleScale)};
    m_Forms.Append(
        Form{.RigidBody = Body(std::move(shape), motion, physics.Density), .Kind = kind, .PackedTint = tint});
}

Body::Motion Level::ComputeGridMotion(const u32 cell) const
{
    const u32 columns = TKit::Math::Min(m_TestBench.GridColumns, m_TestBench.GridCount);
    const u32 rows = (m_TestBench.GridCount + columns - 1) / columns;
    const f32 column = f32(cell % columns) - 0.5f * f32(columns - 1);
    const f32 row = 0.5f * f32(rows - 1) - f32(cell / columns);
    return Body::Motion{
        .Position = f32v3{column * m_TestBench.GridSpacing, row * m_TestBench.GridSpacing, -m_TestBench.GridDistance},
        .Orientation = Onyx::RotType<D3>::Identity,
        .Velocity = f32v3{0.f},
        .AngularVelocity = f32v3{0.f}};
}

void Level::Simulate(const f32 deltaTime)
{
    const bool hasShell = m_Mode != PlayMode::TestShapes;
    const f32 step = deltaTime / f32(m_Physics.Substeps);
    for (u32 substep = 0; substep < m_Physics.Substeps; ++substep)
    {
        for (Form &form : m_Forms)
            form.RigidBody.Advance(step, m_Physics.AngularDamping);
        for (u32 first = 0; first < m_Forms.GetSize(); ++first)
            for (u32 second = first + 1; second < m_Forms.GetSize(); ++second)
                m_Forms[first].RigidBody.Collide(m_Forms[second].RigidBody, m_Physics.Restitution);
        if (!hasShell)
            continue;
        for (Form &form : m_Forms)
            form.RigidBody.CollideWithShell(m_Shell, m_Physics.Restitution);
    }
}

void Level::RewriteShapes()
{
    TKit::FixedArray<ShapeInstance, RECORD_COUNT> instances;
    for (u32 i = 0; i < m_Forms.GetSize(); ++i)
        instances[i] = m_Forms[i].RigidBody.GetInstance(m_Forms[i].PackedTint);
    m_Renderer.Rewrite(TKit::Span<const ShapeInstance>(instances.GetData(), m_Forms.GetSize()));
}

Level::Outcome Level::DetectOutcome(const f32v3 &position, const f32 radius) const
{
    Outcome outcome = Outcome::Playing;
    for (const Form &form : m_Forms)
    {
        if (form.Kind == FormKind::Solid || !form.RigidBody.Penetrate(position, radius))
            continue;
        if (form.Kind == FormKind::Enemy)
            return Outcome::Died;
        outcome = Outcome::Won;
    }

    const f32 distance = TKit::Math::Norm(position);
    if (distance <= 0.f)
        return outcome;

    const f32v3 direction = position / distance;
    if (distance < m_Shell.GetRadius(direction) - radius - SHELL_CONTACT_EPSILON)
        return outcome;

    const Shell::Panel panel = m_Shell.GetPanel(m_Shell.GetTriangleAt(direction));
    if (panel == Shell::Panel::Lethal)
        return Outcome::Died;
    return panel == Shell::Panel::Goal ? Outcome::Won : outcome;
}
} // namespace Lim
