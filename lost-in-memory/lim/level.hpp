#pragma once

#include "lim/alias.hpp"
#include "lim/body.hpp"
#include "lim/byte_reader.hpp"
#include "lim/capacity.hpp"
#include "lim/form_params.hpp"
#include "lim/form_recipe.hpp"
#include "lim/formula_params.hpp"
#include "lim/level_params.hpp"
#include "lim/palette.hpp"
#include "lim/play_mode.hpp"
#include "lim/player.hpp"
#include "lim/region.hpp"
#include "lim/section_graph.hpp"
#include "lim/shape_grammar.hpp"
#include "lim/shell.hpp"
#include "lim/world_layout.hpp"

#include "onyx/color.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/dynamic_array.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
/// A level decoded from a memory region: a moving shell and rigid forms (enemies, solids, a goal) with one-hit death.
class Level
{
  public:
    static constexpr u32 MAX_FORMS = MAX_ENEMIES + MAX_SOLIDS + 1;
    static constexpr u32 MAX_BODIES = u32(RECORD_COUNT) * SectionGraph::MAX_SECTIONS;

    enum class Outcome : u8
    {
        Playing,
        Won,
        Died
    };

    /// Registers the material and the graph-body mesh; the static-mesh pool is created by Generate.
    Level();
    ~Level();

    Level(const Level &) = delete;
    Level &operator=(const Level &) = delete;

    /// Replaces the level from sanitised params; its static meshes live in a pool recreated on every call.
    void Generate(const Region &region, const LevelParams &params, PlayMode mode);
    void Update(f32 deltaTime, Player &player);
    void Draw(Onyx::RenderContext<Onyx::D3> &context, f32 brightness) const;

    [[nodiscard]] Outcome GetOutcome() const;

  private:
    struct Form
    {
        Body RigidBody;
        Onyx::Color Tint;
        FormKind Kind;
    };

    struct FormSpec
    {
        FormKind Kind;
        Onyx::Color Tint;
        ShapeRequest Shape;
        f32 MinDistance;
        f32 MinDriftSpeed;
        f32 MaxDriftSpeed;
        f32 TumbleSpeed;
    };

    void ReplaceMeshPool();
    void DecodeLethalPanels(ByteReader &reader, f32 density, const Palette &colors);
    void DecodeGoal(ByteReader &reader, const LevelParams &params);
    void PlaceShellGoal(ByteReader &reader, const Onyx::Color &tint);
    template <typename CrowdParams>
    void DecodeCrowd(ByteReader &reader, FormKind kind, const CrowdParams &crowd, const Onyx::Color &low,
                     const Onyx::Color &high, const LevelParams &params);
    void SpawnForm(ByteReader &reader, const FormSpec &spec, const LevelParams &params);
    [[nodiscard]] bool IsCrowded(const f32v3 &position, f32 radius) const;

    /// Spawns one section-graph body per record with sections; returns whether one of them is the goal.
    [[nodiscard]] bool SpawnRecords(const Region &region, const LevelParams &params);
    void SpawnRecord(const FormRecipe &recipe, u32 record, const LevelParams &params);
    /// A still motion at a test bench cell, laid out row-major and centred on the view axis.
    [[nodiscard]] Body::Motion ComputeGridMotion(u32 cell) const;

    /// Replaces disconnected graph bodies by their pieces; the largest piece keeps the slot.
    void SplitBodies();
    void Simulate(f32 deltaTime);
    void RewriteGraphMesh();
    [[nodiscard]] Outcome DetectOutcome(const f32v3 &position, f32 radius) const;

    Shell m_Shell;
    Onyx::Resource m_Material;
    TKit::Optional<Onyx::ResourcePool> m_Pool{};
    Onyx::Resource m_GraphMesh;
    TKit::DynamicArray<Form> m_Forms{};
    TKit::Scope<Body::Pieces> m_Pieces;
    TKit::Scope<Body::PieceOffsets> m_PieceOffsets;
    TKit::Scope<WorldRecipe> m_World;

    PhysicsParams m_Physics{};
    GraphParams m_Graph{};
    EvolutionParams m_Evolution{};
    TestBenchParams m_TestBench{};
    LightParams m_Light{};
    Onyx::Color m_AmbientTint = Palette{}.Ambient;
    Onyx::Color m_CoreTint = Palette{}.CoreLight;
    Onyx::Color m_GraphTint = Palette{}.Untinted;
    f32 m_LightRadius = ShellParams{}.Radius;
    f32 m_Time = 0.f;
    u32 m_GraphVertexCapacity = 0;
    u32 m_GraphVertexCount = 0;
    PlayMode m_Mode = PlayMode::Main;
    LinkRule m_LinkRule = LinkRule::Proximity;
    Outcome m_Outcome = Outcome::Playing;
    bool m_GraphMeshOverflowed = false;
};
} // namespace Lim
