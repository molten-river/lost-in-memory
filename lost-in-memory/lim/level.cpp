#include "lim/level.hpp"

#include "lim/convex_hull.hpp"
#include "lim/world_decoder.hpp"

#include "onyx/resources.hpp"
#include "tkit/math/math.hpp"
#include "tkit/utils/logging.hpp"
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
constexpr u32 GRAPH_VERTICES_PER_RECORD = 3 * SectionGraph::MAX_CELLS * ConvexHull::MAX_TRIANGLES;

static_assert(Level::MAX_FORMS <= Level::MAX_BODIES);

struct RecordDrift
{
    f32 MinSpeed;
    f32 MaxSpeed;
    f32 TumbleSpeed;
};

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

    RecordParams &records = mechanics.Records;
    records.EnemySizeMin = TKit::Math::Min(records.EnemySizeMin, maxSize);
    records.EnemySizeMax = TKit::Math::Min(records.EnemySizeMax, maxSize);
    records.SolidSizeMin = TKit::Math::Min(records.SolidSizeMin, maxSize);
    records.SolidSizeMax = TKit::Math::Min(records.SolidSizeMax, maxSize);
    records.GoalSize = TKit::Math::Min(records.GoalSize, maxSize);
    return params;
}

[[nodiscard]] f32 ComputeMinOpening(const LevelParams &params)
{
    return 2.f * params.LookFeel.Player.Radius * params.Mechanics.Shapes.OpeningMargin;
}

[[nodiscard]] f32 ComputeSafeDistance(const MechanicsParams &mechanics)
{
    const PhysicsParams &physics = mechanics.Physics;
    return TKit::Math::Max(physics.MinSafeSpawnRadius, physics.SafeSpawnFraction * mechanics.Shell.Radius);
}

[[nodiscard]] Onyx::Color ComputeRecordTint(const FormRecipe &recipe, const Palette &colors)
{
    if (recipe.Kind == FormKind::Enemy)
        return Mix(colors.EnemyPurple, colors.EnemyRed, recipe.TintFraction);
    if (recipe.Kind == FormKind::Solid)
        return Mix(colors.SolidLow, colors.SolidHigh, recipe.TintFraction);
    return colors.GoalGreen;
}

[[nodiscard]] RecordDrift SelectDrift(const FormKind kind, const MechanicsParams &mechanics)
{
    if (kind == FormKind::Enemy)
        return RecordDrift{.MinSpeed = mechanics.Enemies.MinDriftSpeed,
                           .MaxSpeed = mechanics.Enemies.MaxDriftSpeed,
                           .TumbleSpeed = mechanics.Enemies.TumbleSpeed};
    if (kind == FormKind::Solid)
        return RecordDrift{.MinSpeed = mechanics.Solids.MinDriftSpeed,
                           .MaxSpeed = mechanics.Solids.MaxDriftSpeed,
                           .TumbleSpeed = mechanics.Solids.TumbleSpeed};
    return RecordDrift{.MinSpeed = mechanics.Goal.DriftSpeed,
                       .MaxSpeed = mechanics.Goal.DriftSpeed,
                       .TumbleSpeed = mechanics.Enemies.TumbleSpeed};
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
      m_GraphMesh(Onyx::Resources::RegisterDynamicMesh<D3>().Handle), m_Pieces(TKit::Scope<Body::Pieces>::Create()),
      m_PieceOffsets(TKit::Scope<Body::PieceOffsets>::Create()), m_World(TKit::Scope<WorldRecipe>::Create())
{
    Onyx::Resources::GetDynamicMeshData<D3>(m_GraphMesh)->Flags &= ~Onyx::MeshDataFlag_BackCulled;
    Onyx::Resources::Sync(Onyx::SyncFlag_Materials | Onyx::SyncFlag_DynamicMeshes);
}

Level::~Level()
{
    Onyx::Resources::DestroyDynamicMesh<D3>(m_GraphMesh);
}

void Level::Generate(const Region &region, const LevelParams &requested, const PlayMode mode)
{
    const LevelParams params = FitToShell(Sanitize(requested));
    const Palette &colors = params.LookFeel.Colors;
    const ShellParams &shell = params.Mechanics.Shell;

    ReplaceMeshPool();
    m_Forms.Reserve(MAX_BODIES);

    m_Mode = mode;
    m_Time = 0.f;
    m_Outcome = Outcome::Playing;
    m_Physics = params.Mechanics.Physics;
    m_Graph = params.Mechanics.Graph;
    m_Evolution = params.Mechanics.Evolution;
    m_TestBench = params.Mechanics.TestBench;
    m_LinkRule = params.Flags.ChainLinks ? LinkRule::Chain : LinkRule::Proximity;
    m_Light = params.LookFeel.Light;
    m_AmbientTint = colors.Ambient;
    m_CoreTint = colors.CoreLight;
    m_GraphTint = colors.Untinted;
    m_LightRadius = shell.Radius * m_Light.CoreRadiusScale;
    m_GraphMeshOverflowed = false;

    const bool hasShell = mode != PlayMode::TestShapes;
    const bool fromRecords = params.Flags.StructuralDecode && mode != PlayMode::TestSphere;
    ByteReader reader(region.GetBytes());
    if (hasShell)
    {
        m_Shell.Build(shell, params.LookFeel.ShellMotion, colors, params.Flags.ShellVertexNoise);
        DecodeLethalPanels(reader, shell.LethalDensity, colors);
    }

    if (mode == PlayMode::TestSphere)
        PlaceShellGoal(reader, colors.GoalGreen);
    else if (!fromRecords)
    {
        DecodeGoal(reader, params);
        DecodeCrowd(reader, FormKind::Enemy, params.Mechanics.Enemies, colors.EnemyPurple, colors.EnemyRed, params);
        DecodeCrowd(reader, FormKind::Solid, params.Mechanics.Solids, colors.SolidLow, colors.SolidHigh, params);
    }
    else if (!SpawnRecords(region, params) && hasShell)
        PlaceShellGoal(reader, colors.GoalGreen);

    m_GraphVertexCapacity = fromRecords ? u32(m_Forms.GetSize()) * GRAPH_VERTICES_PER_RECORD : 0;
    Onyx::DynamicMeshData<D3> *const mesh = Onyx::Resources::GetDynamicMeshData<D3>(m_GraphMesh);
    mesh->Vertices.Reserve(m_GraphVertexCapacity);
    mesh->Indices.Reserve(m_GraphVertexCapacity);
    RewriteGraphMesh();

    if (hasShell)
        m_Shell.Update(m_Time);
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);
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
        form.RigidBody.UpdateShape(m_Time, m_Graph, m_Evolution, m_LinkRule);
    SplitBodies();
    Simulate(deltaTime);
    RewriteGraphMesh();

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

    if (m_Mode != PlayMode::TestShapes)
        m_Shell.Draw(context);
    for (const Form &form : m_Forms)
    {
        context.FillColor(form.Tint);
        form.RigidBody.Draw(context);
    }
    if (m_GraphVertexCount > 0)
    {
        context.FillColor(m_GraphTint);
        context.DynamicMesh(m_GraphMesh);
    }
    context.Pop();
}

Level::Outcome Level::GetOutcome() const
{
    return m_Outcome;
}

/// Drops every form first, then idles the GPU and flushes all contexts before the old meshes are destroyed.
void Level::ReplaceMeshPool()
{
    m_Forms.Clear();
    Onyx::Resources::Sync(Onyx::SyncFlag_StaticMeshes);
    if (m_Pool)
        Onyx::Resources::DestroyResourcePool<D3>(*m_Pool);
    m_Pool = Onyx::Resources::CreateResourcePool<D3>(Onyx::Resource_StaticMesh);
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
    if (m_Mode != PlayMode::TestShapes && reader.NextChance(goal.ShellChance))
    {
        PlaceShellGoal(reader, tint);
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

void Level::PlaceShellGoal(ByteReader &reader, const Onyx::Color &tint)
{
    const u32 count = m_Shell.GetTriangleCount();
    u32 triangle = reader.NextIndex(count);
    for (u32 step = 0; step < count && m_Shell.GetPanel(triangle) == Shell::Panel::Lethal; ++step)
        triangle = (triangle + 1) % count;
    m_Shell.SetPanel(triangle, Shell::Panel::Goal, tint);
}

template <typename CrowdParams>
void Level::DecodeCrowd(ByteReader &reader, const FormKind kind, const CrowdParams &crowd, const Onyx::Color &low,
                        const Onyx::Color &high, const LevelParams &params)
{
    const f32 safeDistance = ComputeSafeDistance(params.Mechanics);
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
    const bool onGrid = m_Mode == PlayMode::TestShapes;
    const u32 cell = u32(m_Forms.GetSize());
    if (onGrid && cell >= m_TestBench.GridCount)
        return;

    const PhysicsParams &physics = params.Mechanics.Physics;
    const ShapeRecipe recipe = DecodeShape(reader, spec.Shape, params.Mechanics.Shapes);
    Blob shape(*m_Pool, recipe.Primitives, recipe.BlendRadius, params.LookFeel.Shapes.MeshResolution);
    if (onGrid)
    {
        Form &form = m_Forms.Append(Form{.RigidBody = Body(std::move(shape), ComputeGridMotion(cell), physics.Density),
                                         .Tint = spec.Tint,
                                         .Kind = spec.Kind});
        form.RigidBody.SetAnchored(true);
        return;
    }

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

bool Level::SpawnRecords(const Region &region, const LevelParams &params)
{
    const MechanicsParams &mechanics = params.Mechanics;
    *m_World = DecodeWorld(region.GetBytes(), mechanics.Records, mechanics.Evolution);
    const WorldRecipe &world = *m_World;

    bool hasGoal = false;
    for (u32 i = 0; i < world.Forms.GetSize(); ++i)
    {
        const FormRecipe &recipe = world.Forms[i];
        const u32 record = world.RecordIndices[i];
        if (m_Mode == PlayMode::TestShapes && record >= m_TestBench.GridCount)
            break;
        if (recipe.Sections.IsEmpty())
            continue;
        SpawnRecord(recipe, record, params);
        hasGoal = hasGoal || recipe.Kind == FormKind::Goal;
    }
    return hasGoal;
}

void Level::SpawnRecord(const FormRecipe &recipe, const u32 record, const LevelParams &params)
{
    const MechanicsParams &mechanics = params.Mechanics;
    const PhysicsParams &physics = mechanics.Physics;
    const TKit::Scope<SectionGraph> shape = TKit::Scope<SectionGraph>::Create(recipe, mechanics.Graph, m_LinkRule);
    const Onyx::Color tint = ComputeRecordTint(recipe, params.LookFeel.Colors);
    if (m_Mode == PlayMode::TestShapes)
    {
        Form &form = m_Forms.Append(Form{
            .RigidBody = Body(*shape, ComputeGridMotion(record), physics.Density), .Tint = tint, .Kind = recipe.Kind});
        form.RigidBody.SetAnchored(true);
        return;
    }

    const FormPlacement &placement = recipe.Placement;
    const RecordDrift drift = SelectDrift(recipe.Kind, mechanics);
    const f32 bounding = shape->GetBoundingRadius();
    const f32 farthest = TKit::Math::Max(m_Shell.GetMinRadius() - bounding - physics.WallMargin, 0.f);
    const f32 requested =
        recipe.Kind == FormKind::Goal ? mechanics.Goal.MinSpawnDistance : ComputeSafeDistance(mechanics) + bounding;
    const f32 nearest = TKit::Math::Min(requested, farthest);
    const f32 distance = TKit::Math::LinearLerp(nearest, farthest, placement.DistanceFraction);
    const f32 speed = TKit::Math::LinearLerp(drift.MinSpeed, drift.MaxSpeed, placement.SpeedFraction);
    const f32 tumbleScale =
        TKit::Math::LinearLerp(physics.MinTumbleScale, physics.MaxTumbleScale, placement.TumbleScale);

    const Body::Motion motion{.Position = placement.Direction * distance,
                              .Orientation = Onyx::RotType<D3>::Identity,
                              .Velocity = placement.Heading * speed,
                              .AngularVelocity = placement.TumbleAxis * (drift.TumbleSpeed * tumbleScale)};
    m_Forms.Append(Form{.RigidBody = Body(*shape, motion, physics.Density), .Tint = tint, .Kind = recipe.Kind});
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

void Level::SplitBodies()
{
    Body::Pieces &pieces = *m_Pieces;
    Body::PieceOffsets &offsets = *m_PieceOffsets;
    const u32 count = u32(m_Forms.GetSize());
    for (u32 index = 0; index < count; ++index)
    {
        if (!m_Forms[index].RigidBody.Split(pieces, offsets))
            continue;

        u32 kept = 0;
        for (u32 piece = 1; piece < pieces.GetSize(); ++piece)
        {
            const u32 sections = pieces[piece].GetSectionCount();
            const u32 keptSections = pieces[kept].GetSectionCount();
            if (sections > keptSections ||
                (sections == keptSections && pieces[piece].GetLowestSlot() < pieces[kept].GetLowestSlot()))
                kept = piece;
        }

        for (u32 piece = 0; piece < pieces.GetSize(); ++piece)
        {
            if (piece == kept)
                continue;
            const Form &parent = m_Forms[index];
            m_Forms.Append(
                Form{.RigidBody = parent.RigidBody.MakePiece(pieces[piece], offsets[piece], m_Physics.Density),
                     .Tint = parent.Tint,
                     .Kind = parent.Kind});
        }

        Body &survivor = m_Forms[index].RigidBody;
        const bool anchored = survivor.IsAnchored();
        survivor = survivor.MakePiece(pieces[kept], offsets[kept], m_Physics.Density);
        survivor.SetAnchored(anchored);
    }
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

void Level::RewriteGraphMesh()
{
    m_GraphVertexCount = 0;
    if (m_GraphVertexCapacity == 0)
        return;

    Onyx::DynamicMeshData<D3> *const mesh = Onyx::Resources::GetDynamicMeshData<D3>(m_GraphMesh);
    mesh->Vertices.Resize(m_GraphVertexCapacity);

    u32 written = 0;
    for (const Form &form : m_Forms)
    {
        if (form.RigidBody.AppendTriangles(form.Tint.ToLinear().Pack(), mesh->Vertices, written) ||
            m_GraphMeshOverflowed)
            continue;
        m_GraphMeshOverflowed = true;
        TKIT_LOG_ERROR("[LIM][LEVEL] The graph body mesh is full at {} vertices, so some bodies are not drawn",
                       m_GraphVertexCapacity);
    }

    mesh->Vertices.Resize(written);
    mesh->Indices.Resize(written);
    for (u32 vertex = 0; vertex < written; ++vertex)
        mesh->Indices[vertex] = vertex;
    m_GraphVertexCount = written;
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
