#include "lim/level.hpp"

#include "lim/palette.hpp"

#include "onyx/resources.hpp"
#include "onyx/transform.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/math/math.hpp"
#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr u32 MAX_SUBDIVISIONS = 5;
constexpr f32 MIN_SHELL_RADIUS = 4.f;
constexpr f32 MAX_WAVE_AMPLITUDE_FRACTION = 0.4f;
constexpr f32 MIN_BLOB_SIZE = 0.1f;
constexpr f32 MAX_BLOB_SIZE_FRACTION = 0.25f;

constexpr u32 GOAL_MIN_PRIMITIVES = 1;
constexpr u32 GOAL_MAX_PRIMITIVES = 3;
static_assert(GOAL_MAX_PRIMITIVES <= Blob::MAX_PRIMITIVES);
constexpr f32 MIN_SAFE_SPAWN_RADIUS = 8.f;
constexpr f32 SAFE_SPAWN_FRACTION = 0.3f;
constexpr f32 EDGE_MARGIN = 0.5f;
constexpr f32 CENTER_SPREAD = 0.8f;

constexpr f32 ORBIT_CHANCE = 0.5f;
constexpr f32 MIN_ORBIT_RADIUS = 1.f;
constexpr f32 MIN_AXIS_LENGTH = 1e-3f;
constexpr f32 MIN_TUMBLE_SCALE = 0.5f;
constexpr f32 MAX_TUMBLE_SCALE = 1.5f;

constexpr f32 SHELL_CONTACT_EPSILON = 0.05f;
constexpr f32 AMBIENT_INTENSITY = 1.f;
constexpr f32 LIGHT_INTENSITY = 5.f;

constexpr TKit::FixedArray<f32v3, 4> SHAPE_EXTENT_PER_SIZE{f32v3{0.8f, 0.f, 0.f}, f32v3{0.7f, 0.5f, 0.35f},
                                                           f32v3{0.4f, 0.8f, 0.f}, f32v3{0.8f, 0.3f, 0.f}};

using PrimitiveArray = TKit::StaticArray<BlobPrimitive, Blob::MAX_PRIMITIVES>;

template <typename T> void OrderPair(T &first, T &second)
{
    const T low = TKit::Math::Min(first, second);
    const T high = TKit::Math::Max(first, second);
    first = low;
    second = high;
}

[[nodiscard]] LevelParams Sanitize(LevelParams params)
{
    ShellParams &shell = params.Shell;
    shell.Radius = TKit::Math::Max(shell.Radius, MIN_SHELL_RADIUS);
    shell.Subdivisions = TKit::Math::Min(shell.Subdivisions, MAX_SUBDIVISIONS);
    shell.WaveAmplitude = TKit::Math::Clamp(shell.WaveAmplitude, 0.f, MAX_WAVE_AMPLITUDE_FRACTION * shell.Radius);
    shell.LethalDensity = TKit::Math::Clamp(shell.LethalDensity, 0.f, 1.f);

    EnemyParams &enemies = params.Enemies;
    OrderPair(enemies.MinCount, enemies.MaxCount);
    enemies.MaxCount = TKit::Math::Min(enemies.MaxCount, Level::MAX_ENEMIES);
    enemies.MinCount = TKit::Math::Min(enemies.MinCount, enemies.MaxCount);
    OrderPair(enemies.MinSize, enemies.MaxSize);
    const f32 maxBlobSize = MAX_BLOB_SIZE_FRACTION * shell.Radius;
    enemies.MinSize = TKit::Math::Clamp(enemies.MinSize, MIN_BLOB_SIZE, maxBlobSize);
    enemies.MaxSize = TKit::Math::Clamp(enemies.MaxSize, MIN_BLOB_SIZE, maxBlobSize);
    OrderPair(enemies.MinPrimitives, enemies.MaxPrimitives);
    enemies.MinPrimitives = TKit::Math::Clamp(enemies.MinPrimitives, 1U, Blob::MAX_PRIMITIVES);
    enemies.MaxPrimitives = TKit::Math::Clamp(enemies.MaxPrimitives, 1U, Blob::MAX_PRIMITIVES);
    enemies.BlendRadius = TKit::Math::Max(enemies.BlendRadius, 0.f);
    OrderPair(enemies.MinDriftSpeed, enemies.MaxDriftSpeed);
    enemies.MinDriftSpeed = TKit::Math::Max(enemies.MinDriftSpeed, 0.f);
    enemies.MaxDriftSpeed = TKit::Math::Max(enemies.MaxDriftSpeed, 0.f);
    enemies.TumbleSpeed = TKit::Math::Max(enemies.TumbleSpeed, 0.f);

    GoalParams &goal = params.Goal;
    goal.Size = TKit::Math::Clamp(goal.Size, MIN_BLOB_SIZE, maxBlobSize);
    goal.DriftSpeed = TKit::Math::Max(goal.DriftSpeed, 0.f);
    goal.MinSpawnDistance = TKit::Math::Max(goal.MinSpawnDistance, 0.f);
    return params;
}

[[nodiscard]] Onyx::Color DecodeHostileColor(ByteReader &reader)
{
    return Mix(ENEMY_PURPLE, ENEMY_RED, reader.NextUnit());
}

[[nodiscard]] BlobPrimitive DecodePrimitive(ByteReader &reader, const f32 minSize, const f32 maxSize)
{
    const f32 size = reader.NextRange(minSize, maxSize);
    const u32 shape = reader.NextIndex(u32(SHAPE_EXTENT_PER_SIZE.GetSize()));
    const f32 spread = reader.NextUnit() * CENTER_SPREAD * size;
    const f32v3 offset = reader.NextDirection() * spread;
    const f32q rotation = reader.NextRotation();
    return BlobPrimitive{.Kind = BlobPrimitive::Shape(shape),
                         .Center = offset,
                         .Extent = SHAPE_EXTENT_PER_SIZE[shape] * size,
                         .Rotation = rotation};
}

[[nodiscard]] PrimitiveArray DecodePrimitives(ByteReader &reader, const u32 count, const f32 minSize, const f32 maxSize)
{
    PrimitiveArray primitives{};
    for (u32 i = 0; i < count; ++i)
        primitives.Append(DecodePrimitive(reader, minSize, maxSize));
    return primitives;
}
} // namespace

Level::Level()
    : m_Material(Onyx::Resources::RegisterMaterial<D3>()),
      m_Pool(Onyx::Resources::CreateResourcePool<D3>(Onyx::Resource_StaticMesh))
{
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials);
}

void Level::Generate(const Region &region, const LevelParams &requested)
{
    const LevelParams params = Sanitize(requested);

    m_Forms.Clear();
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);
    Onyx::Resources::DestroyResourcePool<D3>(m_Pool);
    m_Pool = Onyx::Resources::CreateResourcePool<D3>(Onyx::Resource_StaticMesh);

    m_Time = 0.f;
    m_Outcome = Outcome::Playing;
    m_LightRadius = params.Shell.Radius;
    m_LowestShellRadius = params.Shell.Radius - params.Shell.WaveAmplitude;

    ByteReader reader(region.GetBytes());
    m_Shell.Build(params.Shell);
    DecodeLethalPanels(reader, params.Shell.LethalDensity);
    DecodeGoal(reader, params);
    DecodeEnemies(reader, params);

    m_Shell.Update(m_Time);
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);
}

void Level::Update(const f32 deltaTime, Player &player)
{
    if (m_Outcome != Outcome::Playing)
        return;

    m_Time += deltaTime;
    m_Shell.Update(m_Time);
    for (Form &form : m_Forms)
        Advance(form, deltaTime);

    const f32v3 position = player.GetPosition();
    const f32 distance = TKit::Math::Norm(position);
    if (distance > 0.f)
        player.ConfineToRadius(m_Shell.GetRadius(position / distance, m_Time));

    m_Outcome = DetectOutcome(player.GetPosition());
}

void Level::Draw(Onyx::RenderContext<D3> &context, const f32 brightness) const
{
    const f32 scale = TKit::Math::Clamp(brightness, 0.f, 1.f);

    context.PushClear();
    context.NoClip();
    context.Material(m_Material);
    context.RenderFlags(Onyx::RenderModeFlag_Shaded);
    context.SetAmbientLight(AMBIENT);
    context.SetAmbientIntensity(AMBIENT_INTENSITY * scale);
    context.PointLight(Onyx::PointLightParameters<D3>{.Position = f32v3{0.f},
                                                      .Tint = CORE_LIGHT,
                                                      .LightRadius = m_LightRadius,
                                                      .Intensity = LIGHT_INTENSITY * scale});

    m_Shell.Draw(context);
    for (const Form &form : m_Forms)
    {
        context.FillColor(form.Tint);
        form.Body.Draw(context, Onyx::Transform<D3>::ComputeTransform(form.Position, f32v3{1.f}, form.Orientation));
    }
    context.Pop();
}

Level::Outcome Level::GetOutcome() const
{
    return m_Outcome;
}

void Level::DecodeLethalPanels(ByteReader &reader, const f32 density)
{
    for (u32 triangle = 0; triangle < m_Shell.GetTriangleCount(); ++triangle)
    {
        if (!reader.NextChance(density))
            continue;
        m_Shell.SetPanel(triangle, Shell::Panel::Lethal, DecodeHostileColor(reader));
    }
}

void Level::DecodeGoal(ByteReader &reader, const LevelParams &params)
{
    if (reader.NextChance(params.Goal.ShellChance))
    {
        const u32 count = m_Shell.GetTriangleCount();
        u32 triangle = reader.NextIndex(count);
        for (u32 step = 0; step < count && m_Shell.GetPanel(triangle) == Shell::Panel::Lethal; ++step)
            triangle = (triangle + 1) % count;
        m_Shell.SetPanel(triangle, Shell::Panel::Goal, GOAL_GREEN);
        return;
    }

    const u32 primitiveCount = reader.NextCount(GOAL_MIN_PRIMITIVES, GOAL_MAX_PRIMITIVES);
    const PrimitiveArray primitives = DecodePrimitives(reader, primitiveCount, params.Goal.Size, params.Goal.Size);
    SpawnForm(reader,
              FormSpec{.Kind = FormKind::Goal,
                       .Tint = GOAL_GREEN,
                       .BlendRadius = params.Enemies.BlendRadius,
                       .MinDistance = params.Goal.MinSpawnDistance,
                       .MinDriftSpeed = params.Goal.DriftSpeed,
                       .MaxDriftSpeed = params.Goal.DriftSpeed,
                       .TumbleSpeed = params.Enemies.TumbleSpeed},
              primitives);
}

void Level::DecodeEnemies(ByteReader &reader, const LevelParams &params)
{
    const EnemyParams &enemies = params.Enemies;
    const f32 safeRadius = TKit::Math::Max(MIN_SAFE_SPAWN_RADIUS, SAFE_SPAWN_FRACTION * params.Shell.Radius);
    const u32 count = reader.NextCount(enemies.MinCount, enemies.MaxCount);
    for (u32 i = 0; i < count; ++i)
    {
        const u32 primitiveCount = reader.NextCount(enemies.MinPrimitives, enemies.MaxPrimitives);
        const PrimitiveArray primitives = DecodePrimitives(reader, primitiveCount, enemies.MinSize, enemies.MaxSize);
        const Onyx::Color tint = DecodeHostileColor(reader);
        SpawnForm(reader,
                  FormSpec{.Kind = FormKind::Enemy,
                           .Tint = tint,
                           .BlendRadius = enemies.BlendRadius,
                           .MinDistance = safeRadius,
                           .MinDriftSpeed = enemies.MinDriftSpeed,
                           .MaxDriftSpeed = enemies.MaxDriftSpeed,
                           .TumbleSpeed = enemies.TumbleSpeed},
                  primitives);
    }
}

void Level::SpawnForm(ByteReader &reader, const FormSpec &spec, const TKit::Span<const BlobPrimitive> primitives)
{
    Blob body(m_Pool, primitives, spec.BlendRadius);
    const f32 bounding = body.GetBoundingRadius();

    const f32 farthest = TKit::Math::Max(m_LowestShellRadius - bounding - EDGE_MARGIN, 0.f);
    const f32 requested = spec.Kind == FormKind::Enemy ? spec.MinDistance + bounding : spec.MinDistance;
    const f32 distance = reader.NextRange(TKit::Math::Min(requested, farthest), farthest);
    const f32v3 radial = reader.NextDirection();

    const bool orbits = reader.NextChance(ORBIT_CHANCE);
    const f32 speed = reader.NextRange(spec.MinDriftSpeed, spec.MaxDriftSpeed);
    const f32v3 heading = reader.NextDirection();
    const f32q orientation = reader.NextRotation();
    const f32v3 tumbleAxis = reader.NextDirection();
    const f32 tumbleScale = reader.NextRange(MIN_TUMBLE_SCALE, MAX_TUMBLE_SCALE);

    Form form{.Body = std::move(body),
              .Tint = spec.Tint,
              .Kind = spec.Kind,
              .Mode = orbits ? DriftMode::Orbit : DriftMode::Bounce,
              .Position = radial * distance,
              .Orientation = orientation,
              .TumbleAxis = tumbleAxis,
              .TumbleRate = spec.TumbleSpeed * tumbleScale};

    if (orbits)
    {
        const f32v3 tangent = heading - radial * TKit::Math::Dot(heading, radial);
        const f32 tangentLength = TKit::Math::Norm(tangent);
        form.OrbitAxis = tangentLength > MIN_AXIS_LENGTH ? tangent / tangentLength
                                                         : Onyx::Transform<D3>::CreateOrthogonalBase(radial)[0];
        form.OrbitRate = speed / TKit::Math::Max(distance, MIN_ORBIT_RADIUS);
    }
    else
        form.Velocity = heading * speed;

    m_Forms.Append(std::move(form));
}

void Level::Advance(Form &form, const f32 deltaTime) const
{
    if (form.Mode == DriftMode::Orbit)
        form.Position = f32q::FromAngleAxis(form.OrbitRate * deltaTime, form.OrbitAxis) * form.Position;
    else
    {
        form.Position += form.Velocity * deltaTime;
        const f32 distance = TKit::Math::Norm(form.Position);
        if (distance > 0.f)
        {
            const f32v3 outward = form.Position / distance;
            const f32 limit = TKit::Math::Max(m_Shell.GetRadius(outward, m_Time) - form.Body.GetBoundingRadius(), 0.f);
            const f32 outwardSpeed = TKit::Math::Dot(form.Velocity, outward);
            if (distance >= limit && outwardSpeed > 0.f)
            {
                form.Position = outward * limit;
                form.Velocity -= outward * (2.f * outwardSpeed);
            }
        }
    }

    form.Orientation =
        TKit::Math::Normalize(f32q::FromAngleAxis(form.TumbleRate * deltaTime, form.TumbleAxis) * form.Orientation);
}

Level::Outcome Level::DetectOutcome(const f32v3 &position) const
{
    Outcome outcome = Outcome::Playing;
    for (const Form &form : m_Forms)
    {
        if (!form.IsTouching(position))
            continue;
        if (form.Kind == FormKind::Enemy)
            return Outcome::Died;
        outcome = Outcome::Won;
    }

    const f32 distance = TKit::Math::Norm(position);
    if (distance <= 0.f)
        return outcome;

    const f32v3 direction = position / distance;
    if (distance < m_Shell.GetRadius(direction, m_Time) - Player::RADIUS - SHELL_CONTACT_EPSILON)
        return outcome;

    const Shell::Panel panel = m_Shell.GetPanel(m_Shell.GetTriangleAt(direction));
    if (panel == Shell::Panel::Lethal)
        return Outcome::Died;
    return panel == Shell::Panel::Goal ? Outcome::Won : outcome;
}

bool Level::Form::IsTouching(const f32v3 &point) const
{
    const f32v3 offset = point - Position;
    const f32 reach = Body.GetBoundingRadius() + Player::RADIUS;
    if (TKit::Math::NormSquared(offset) >= reach * reach)
        return false;
    return Body.Distance(TKit::Math::Conjugate(Orientation) * offset) < Player::RADIUS;
}
} // namespace Lim
