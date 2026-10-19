#pragma once

#include "lim/alias.hpp"
#include "lim/body.hpp"
#include "lim/byte_reader.hpp"
#include "lim/form_params.hpp"
#include "lim/level_params.hpp"
#include "lim/palette.hpp"
#include "lim/player.hpp"
#include "lim/region.hpp"
#include "lim/shape_grammar.hpp"
#include "lim/shell.hpp"

#include "onyx/color.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// A level decoded from a memory region: a moving shell and rigid forms (enemies, solids, a goal) with one-hit death.
class Level
{
  public:
    static constexpr u32 MAX_ENEMIES = 30;
    static constexpr u32 MAX_SOLIDS = 12;
    static constexpr u32 MAX_FORMS = MAX_ENEMIES + MAX_SOLIDS + 1;

    enum class Outcome : u8
    {
        Playing,
        Won,
        Died
    };

    /// Creates the one static-mesh pool, never recreated because Onyx reuses pool ids.
    Level();

    Level(const Level &) = delete;
    Level &operator=(const Level &) = delete;

    void Generate(const Region &region, const LevelParams &params);
    void Update(f32 deltaTime, Player &player);
    void Draw(Onyx::RenderContext<Onyx::D3> &context, f32 brightness) const;

    [[nodiscard]] Outcome GetOutcome() const;

  private:
    enum class FormKind : u8
    {
        Enemy,
        Solid,
        Goal
    };

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

    void DecodeLethalPanels(ByteReader &reader, f32 density, const Palette &colors);
    void DecodeGoal(ByteReader &reader, const LevelParams &params);
    template <typename CrowdParams>
    void DecodeCrowd(ByteReader &reader, FormKind kind, const CrowdParams &crowd, const Onyx::Color &low,
                     const Onyx::Color &high, const LevelParams &params);
    void SpawnForm(ByteReader &reader, const FormSpec &spec, const LevelParams &params);
    [[nodiscard]] bool IsCrowded(const f32v3 &position, f32 radius) const;

    void Simulate(f32 deltaTime);
    [[nodiscard]] Outcome DetectOutcome(const f32v3 &position, f32 radius) const;

    Shell m_Shell;
    Onyx::Resource m_Material;
    const Onyx::ResourcePool m_Pool;
    TKit::StaticArray<Form, MAX_FORMS> m_Forms{};

    PhysicsParams m_Physics{};
    LightParams m_Light{};
    Onyx::Color m_AmbientTint = Palette{}.Ambient;
    Onyx::Color m_CoreTint = Palette{}.CoreLight;
    f32 m_LightRadius = ShellParams{}.Radius;
    f32 m_Time = 0.f;
    Outcome m_Outcome = Outcome::Playing;
};
} // namespace Lim
