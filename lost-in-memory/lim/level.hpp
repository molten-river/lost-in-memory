#pragma once

#include "alias/alias.hpp"
#include "lim/body.hpp"
#include "lim/level_params.hpp"
#include "lim/palette.hpp"
#include "lim/play_mode.hpp"
#include "lim/player.hpp"
#include "lim/shell.hpp"
#include "lim/world_layout.hpp"
#include "memory/byte_reader.hpp"
#include "memory/region.hpp"
#include "shapes/shape_kit_params.hpp"
#include "shapes/shape_renderer.hpp"

#include "onyx/color.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"

#include <cstddef>

namespace Lim
{
/// A level decoded from a memory region: a moving shell and one kit body per record, with one-hit death.
class Level
{
  public:
    enum class Outcome : u8
    {
        Playing,
        Won,
        Died
    };

    enum class FormKind : u8
    {
        Enemy,
        Solid,
        Goal
    };

    Level();

    Level(const Level &) = delete;
    Level &operator=(const Level &) = delete;

    /// Replaces the level from sanitised params, baking every non-empty record into a kit body.
    void Generate(const Region &region, const LevelParams &params, const ShapeKitParams &kit, PlayMode mode);
    void Update(f32 deltaTime, Player &player);
    void Draw(Onyx::RenderContext<Onyx::D3> &context, f32 brightness) const;

    [[nodiscard]] Outcome GetOutcome() const;

  private:
    struct Form
    {
        Body RigidBody;
        FormKind Kind;
        Onyx::Color Tint;
    };

    /// A record's index in the region, and its slot among the count non-empty records in spawn order.
    struct Placement
    {
        u32 Index;
        u32 Slot;
        u32 Count;
    };

    void DecodeLethalPanels(ByteReader &reader, f32 density, const Palette &colors);
    void PlaceShellGoal(ByteReader &reader, const Onyx::Color &tint);
    /// Spawns one body per non-empty record; returns whether one of them is the goal.
    [[nodiscard]] bool SpawnRecords(const Region &region, const LevelParams &params, const ShapeKitParams &kit);
    void SpawnRecord(TKit::Span<const std::byte> record, const Placement &placement, FormKind kind,
                     const LevelParams &params, const ShapeKitParams &kit);
    /// A still motion at a test bench cell, laid out row-major and centred on the view axis.
    [[nodiscard]] Body::Motion ComputeGridMotion(u32 cell) const;

    void Simulate(f32 deltaTime);
    [[nodiscard]] Outcome DetectOutcome(const f32v3 &position, f32 radius) const;

    Shell m_Shell;
    ShapeRenderer m_Renderer;
    Onyx::Resource m_Material;
    TKit::StaticArray<Form, RECORD_COUNT> m_Forms{};

    PhysicsParams m_Physics{};
    TestBenchParams m_TestBench{};
    LightParams m_Light{};
    Onyx::Color m_AmbientTint = Palette{}.Ambient;
    Onyx::Color m_CoreTint = Palette{}.CoreLight;
    f32 m_LightRadius = ShellParams{}.Radius;
    f32 m_Time = 0.f;
    PlayMode m_Mode = PlayMode::Main;
    Outcome m_Outcome = Outcome::Playing;
};
} // namespace Lim
