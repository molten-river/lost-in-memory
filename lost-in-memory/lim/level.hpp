#pragma once

#include "lim/alias.hpp"
#include "lim/blob.hpp"
#include "lim/byte_reader.hpp"
#include "lim/level_params.hpp"
#include "lim/player.hpp"
#include "lim/region.hpp"
#include "lim/shell.hpp"

#include "onyx/color.hpp"
#include "onyx/context.hpp"
#include "onyx/handle.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/quaternion.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// A level decoded from a memory region: a pulsing shell, drifting enemy blobs and a goal, with one-hit death rules.
class Level
{
  public:
    static constexpr u32 MAX_ENEMIES = 30;
    static constexpr u32 MAX_FORMS = MAX_ENEMIES + 1;

    enum class Outcome : u8
    {
        Playing,
        Won,
        Died
    };

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
        Goal
    };

    enum class DriftMode : u8
    {
        Bounce,
        Orbit
    };

    struct Form
    {
        [[nodiscard]] bool IsTouching(const f32v3 &point) const;

        Blob Body;
        Onyx::Color Tint;
        FormKind Kind;
        DriftMode Mode;
        f32v3 Position;
        f32v3 Velocity{0.f};
        f32v3 OrbitAxis{0.f, 1.f, 0.f};
        f32 OrbitRate = 0.f;
        f32q Orientation;
        f32v3 TumbleAxis;
        f32 TumbleRate;
    };

    struct FormSpec
    {
        FormKind Kind;
        Onyx::Color Tint;
        f32 BlendRadius;
        f32 MinDistance;
        f32 MinDriftSpeed;
        f32 MaxDriftSpeed;
        f32 TumbleSpeed;
    };

    void DecodeLethalPanels(ByteReader &reader, f32 density);
    void DecodeGoal(ByteReader &reader, const LevelParams &params);
    void DecodeEnemies(ByteReader &reader, const LevelParams &params);
    void SpawnForm(ByteReader &reader, const FormSpec &spec, TKit::Span<const BlobPrimitive> primitives);

    void Advance(Form &form, f32 deltaTime) const;
    [[nodiscard]] Outcome DetectOutcome(const f32v3 &position) const;

    Shell m_Shell;
    Onyx::Resource m_Material;
    Onyx::ResourcePool m_Pool;
    TKit::StaticArray<Form, MAX_FORMS> m_Forms{};

    f32 m_Time = 0.f;
    f32 m_LightRadius = ShellParams{}.Radius;
    f32 m_LowestShellRadius = ShellParams{}.Radius;
    Outcome m_Outcome = Outcome::Playing;
};
} // namespace Lim
