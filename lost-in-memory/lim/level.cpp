#include "lim/level.hpp"

#include "onyx/resources.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/optional.hpp"
#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr f32 MAX_WAVE_AMPLITUDE_FRACTION = 0.4f;
constexpr f32 MAX_FORM_SIZE_FRACTION = 0.25f;
constexpr f32 MIN_FORM_SIZE = 0.1f;
constexpr f32 MIN_DENSITY = 1e-3f;
constexpr u32 MAX_SUBSTEPS = 8;
constexpr u32 MAX_SPAWN_ATTEMPTS = 8;
constexpr f32 SHELL_CONTACT_EPSILON = 0.05f;

[[nodiscard]] LevelParams FitToShell(LevelParams params)
{
    const f32 radius = params.Mechanics.Shell.Radius;
    const f32 maxAmplitude = MAX_WAVE_AMPLITUDE_FRACTION * radius;
    ShellMotionParams &motion = params.LookFeel.ShellMotion;
    motion.WaveAmplitude = TKit::Math::Min(motion.WaveAmplitude, maxAmplitude);
    motion.NoiseAmplitude = TKit::Math::Min(motion.NoiseAmplitude, maxAmplitude);

    const f32 maxSize = MAX_FORM_SIZE_FRACTION * radius;
    MechanicsParams &mechanics = params.Mechanics;
    mechanics.Enemies.MinSize = TKit::Math::Min(mechanics.Enemies.MinSize, maxSize);
    mechanics.Enemies.MaxSize = TKit::Math::Min(mechanics.Enemies.MaxSize, maxSize);
    mechanics.Solids.MinSize = TKit::Math::Min(mechanics.Solids.MinSize, maxSize);
    mechanics.Solids.MaxSize = TKit::Math::Min(mechanics.Solids.MaxSize, maxSize);
    mechanics.Goal.Size = TKit::Math::Min(mechanics.Goal.Size, maxSize);
    return params;
}

[[nodiscard]] f32 ComputeMinOpening(const LevelParams &params)
{
    return 2.f * params.LookFeel.Player.Radius * params.Mechanics.Shapes.OpeningMargin;
}

template <typename T> void OrderPair(T &first, T &second)
{
    const T low = TKit::Math::Min(first, second);
    const T high = TKit::Math::Max(first, second);
    first = low;
    second = high;
}

template <typename CrowdParams> [[nodiscard]] CrowdParams SanitizeCrowd(CrowdParams crowd, const u32 maxCount)
{
    OrderPair(crowd.MinCount, crowd.MaxCount);
    crowd.MaxCount = TKit::Math::Min(crowd.MaxCount, maxCount);
    crowd.MinCount = TKit::Math::Min(crowd.MinCount, crowd.MaxCount);
    OrderPair(crowd.MinSize, crowd.MaxSize);
    crowd.MinSize = TKit::Math::Max(crowd.MinSize, MIN_FORM_SIZE);
    crowd.MaxSize = TKit::Math::Max(crowd.MaxSize, MIN_FORM_SIZE);
    OrderPair(crowd.MinPrimitives, crowd.MaxPrimitives);
    crowd.MinPrimitives = TKit::Math::Clamp(crowd.MinPrimitives, 1U, Blob::MAX_PRIMITIVES);
    crowd.MaxPrimitives = TKit::Math::Clamp(crowd.MaxPrimitives, 1U, Blob::MAX_PRIMITIVES);
    crowd.BlendRadius = TKit::Math::Max(crowd.BlendRadius, 0.f);
    OrderPair(crowd.MinDriftSpeed, crowd.MaxDriftSpeed);
    crowd.MinDriftSpeed = TKit::Math::Max(crowd.MinDriftSpeed, 0.f);
    crowd.MaxDriftSpeed = TKit::Math::Max(crowd.MaxDriftSpeed, 0.f);
    crowd.TumbleSpeed = TKit::Math::Max(crowd.TumbleSpeed, 0.f);
    return crowd;
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
    const LevelParams params = FitToShell(Sanitize(requested));
    const Palette &colors = params.LookFeel.Colors;
    const ShellParams &shell = params.Mechanics.Shell;

    m_Forms.Clear();
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);

    m_Time = 0.f;
    m_Outcome = Outcome::Playing;
    m_Physics = params.Mechanics.Physics;
    m_Light = params.LookFeel.Light;
    m_AmbientTint = colors.Ambient;
    m_CoreTint = colors.CoreLight;
    m_LightRadius = shell.Radius * m_Light.CoreRadiusScale;

    ByteReader reader(region.GetBytes());
    m_Shell.Build(shell, params.LookFeel.ShellMotion, colors, params.Flags.ShellVertexNoise);
    DecodeLethalPanels(reader, shell.LethalDensity, colors);
    DecodeGoal(reader, params);
    DecodeCrowd(reader, FormKind::Enemy, params.Mechanics.Enemies, colors.EnemyPurple, colors.EnemyRed, params);
    DecodeCrowd(reader, FormKind::Solid, params.Mechanics.Solids, colors.SolidLow, colors.SolidHigh, params);

    m_Shell.Update(m_Time);
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);
}

void Level::Update(const f32 deltaTime, Player &player)
{
    if (m_Outcome != Outcome::Playing)
        return;

    m_Time += deltaTime;
    m_Shell.Update(m_Time);
    Simulate(deltaTime);

    const f32 radius = player.GetRadius();
    for (const Form &form : m_Forms)
    {
        if (form.Kind != FormKind::Solid)
            continue;
        const TKit::Optional<Body::Penetration> penetration = form.RigidBody.Penetrate(player.GetPosition(), radius);
        if (penetration)
            player.PushOut(penetration->Normal, penetration->Depth);
    }

    const f32v3 position = player.GetPosition();
    const f32 distance = TKit::Math::Norm(position);
    if (distance > 0.f)
        player.ConfineToRadius(m_Shell.GetRadius(position / distance));

    m_Outcome = DetectOutcome(player.GetPosition(), radius);
}

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

    m_Shell.Draw(context);
    for (const Form &form : m_Forms)
    {
        context.FillColor(form.Tint);
        form.RigidBody.Draw(context);
    }
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

void Level::DecodeGoal(ByteReader &reader, const LevelParams &params)
{
    const GoalParams &goal = params.Mechanics.Goal;
    const Onyx::Color &tint = params.LookFeel.Colors.GoalGreen;
    if (reader.NextChance(goal.ShellChance))
    {
        const u32 count = m_Shell.GetTriangleCount();
        u32 triangle = reader.NextIndex(count);
        for (u32 step = 0; step < count && m_Shell.GetPanel(triangle) == Shell::Panel::Lethal; ++step)
            triangle = (triangle + 1) % count;
        m_Shell.SetPanel(triangle, Shell::Panel::Goal, tint);
        return;
    }

    const EnemyParams &enemies = params.Mechanics.Enemies;
    SpawnForm(reader,
              FormSpec{.Kind = FormKind::Goal,
                       .Tint = tint,
                       .Shape = ShapeRequest{.MinSize = goal.Size,
                                             .MaxSize = goal.Size,
                                             .MinPrimitives = goal.MinPrimitives,
                                             .MaxPrimitives = goal.MaxPrimitives,
                                             .BlendRadius = enemies.BlendRadius,
                                             .MinOpening = ComputeMinOpening(params),
                                             .Carved = params.Flags.CarvedShapes},
                       .MinDistance = goal.MinSpawnDistance,
                       .MinDriftSpeed = goal.DriftSpeed,
                       .MaxDriftSpeed = goal.DriftSpeed,
                       .TumbleSpeed = enemies.TumbleSpeed},
              params);
}

template <typename CrowdParams>
void Level::DecodeCrowd(ByteReader &reader, const FormKind kind, const CrowdParams &crowd, const Onyx::Color &low,
                        const Onyx::Color &high, const LevelParams &params)
{
    const PhysicsParams &physics = params.Mechanics.Physics;
    const f32 safeDistance =
        TKit::Math::Max(physics.MinSafeSpawnRadius, physics.SafeSpawnFraction * params.Mechanics.Shell.Radius);
    const u32 count = reader.NextCount(crowd.MinCount, crowd.MaxCount);
    for (u32 i = 0; i < count; ++i)
    {
        const Onyx::Color tint = Mix(low, high, reader.NextUnit());
        SpawnForm(reader,
                  FormSpec{.Kind = kind,
                           .Tint = tint,
                           .Shape = ShapeRequest{.MinSize = crowd.MinSize,
                                                 .MaxSize = crowd.MaxSize,
                                                 .MinPrimitives = crowd.MinPrimitives,
                                                 .MaxPrimitives = crowd.MaxPrimitives,
                                                 .BlendRadius = crowd.BlendRadius,
                                                 .MinOpening = ComputeMinOpening(params),
                                                 .Carved = params.Flags.CarvedShapes},
                           .MinDistance = safeDistance,
                           .MinDriftSpeed = crowd.MinDriftSpeed,
                           .MaxDriftSpeed = crowd.MaxDriftSpeed,
                           .TumbleSpeed = crowd.TumbleSpeed},
                  params);
    }
}

void Level::SpawnForm(ByteReader &reader, const FormSpec &spec, const LevelParams &params)
{
    const PhysicsParams &physics = params.Mechanics.Physics;
    const ShapeRecipe recipe = DecodeShape(reader, spec.Shape, params.Mechanics.Shapes);
    Blob shape(m_Pool, recipe.Primitives, recipe.BlendRadius, params.LookFeel.Shapes.MeshResolution);
    const f32 bounding = shape.GetBoundingRadius();

    const f32 farthest = TKit::Math::Max(m_Shell.GetMinRadius() - bounding - physics.WallMargin, 0.f);
    const f32 requested = spec.Kind == FormKind::Goal ? spec.MinDistance : spec.MinDistance + bounding;
    const f32 nearest = TKit::Math::Min(requested, farthest);

    f32v3 position{0.f};
    for (u32 attempt = 0; attempt < MAX_SPAWN_ATTEMPTS; ++attempt)
    {
        const f32 distance = reader.NextRange(nearest, farthest);
        position = reader.NextDirection() * distance;
        if (!IsCrowded(position, bounding))
            break;
    }

    const f32 speed = reader.NextRange(spec.MinDriftSpeed, spec.MaxDriftSpeed);
    const f32v3 heading = reader.NextDirection();
    const f32q orientation = reader.NextRotation();
    const f32v3 tumbleAxis = reader.NextDirection();
    const f32 tumbleScale = reader.NextRange(physics.MinTumbleScale, physics.MaxTumbleScale);

    const Body::Motion motion{.Position = position,
                              .Orientation = orientation,
                              .Velocity = heading * speed,
                              .AngularVelocity = tumbleAxis * (spec.TumbleSpeed * tumbleScale)};
    m_Forms.Append(
        Form{.RigidBody = Body(std::move(shape), motion, physics.Density), .Tint = spec.Tint, .Kind = spec.Kind});
}

bool Level::IsCrowded(const f32v3 &position, const f32 radius) const
{
    for (const Form &form : m_Forms)
    {
        const f32 reach = radius + form.RigidBody.GetBoundingRadius();
        if (TKit::Math::NormSquared(position - form.RigidBody.GetPosition()) < reach * reach)
            return true;
    }
    return false;
}

void Level::Simulate(const f32 deltaTime)
{
    const f32 step = deltaTime / f32(m_Physics.Substeps);
    for (u32 substep = 0; substep < m_Physics.Substeps; ++substep)
    {
        for (Form &form : m_Forms)
            form.RigidBody.Advance(step, m_Physics.AngularDamping);
        for (u32 first = 0; first < m_Forms.GetSize(); ++first)
            for (u32 second = first + 1; second < m_Forms.GetSize(); ++second)
                m_Forms[first].RigidBody.Collide(m_Forms[second].RigidBody, m_Physics.Restitution);
        for (Form &form : m_Forms)
            form.RigidBody.CollideWithShell(m_Shell, m_Physics.Restitution);
    }
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

EnemyParams Sanitize(const EnemyParams params)
{
    return SanitizeCrowd(params, Level::MAX_ENEMIES);
}

SolidParams Sanitize(const SolidParams params)
{
    return SanitizeCrowd(params, Level::MAX_SOLIDS);
}

GoalParams Sanitize(GoalParams params)
{
    params.ShellChance = TKit::Math::Clamp(params.ShellChance, 0.f, 1.f);
    params.Size = TKit::Math::Max(params.Size, MIN_FORM_SIZE);
    params.DriftSpeed = TKit::Math::Max(params.DriftSpeed, 0.f);
    params.MinSpawnDistance = TKit::Math::Max(params.MinSpawnDistance, 0.f);
    OrderPair(params.MinPrimitives, params.MaxPrimitives);
    params.MinPrimitives = TKit::Math::Clamp(params.MinPrimitives, 1U, Blob::MAX_PRIMITIVES);
    params.MaxPrimitives = TKit::Math::Clamp(params.MaxPrimitives, 1U, Blob::MAX_PRIMITIVES);
    return params;
}

PhysicsParams Sanitize(PhysicsParams params)
{
    params.Restitution = TKit::Math::Clamp(params.Restitution, 0.f, 1.f);
    params.Density = TKit::Math::Max(params.Density, MIN_DENSITY);
    params.AngularDamping = TKit::Math::Clamp(params.AngularDamping, 0.f, 1.f);
    params.Substeps = TKit::Math::Clamp(params.Substeps, 1U, MAX_SUBSTEPS);
    params.WallMargin = TKit::Math::Max(params.WallMargin, 0.f);
    OrderPair(params.MinTumbleScale, params.MaxTumbleScale);
    params.MinTumbleScale = TKit::Math::Max(params.MinTumbleScale, 0.f);
    params.MaxTumbleScale = TKit::Math::Max(params.MaxTumbleScale, 0.f);
    params.MinSafeSpawnRadius = TKit::Math::Max(params.MinSafeSpawnRadius, 0.f);
    params.SafeSpawnFraction = TKit::Math::Clamp(params.SafeSpawnFraction, 0.f, 1.f);
    return params;
}
} // namespace Lim
