#pragma once

#include "lim/alias.hpp"
#include "lim/level_params.hpp"

#include "onyx/context.hpp"
#include "tkit/container/tier_array.hpp"

namespace Lim
{
class Shell
{
  public:
    enum class Panel : u8
    {
        Plain,
        Lethal,
        Goal
    };

    Shell();
    ~Shell();

    Shell(const Shell &) = delete;
    Shell &operator=(const Shell &) = delete;

    void Build(const ShellParams &params);
    [[nodiscard]] u32 GetTriangleCount() const;
    void SetPanel(u32 triangle, Panel panel, const Onyx::Color &color);
    [[nodiscard]] Panel GetPanel(u32 triangle) const;

    void Update(f32 time);
    void Draw(Onyx::RenderContext<Onyx::D3> &context) const;

    /// Displaced radius along a unit direction, matching the rendered surface at the vertices.
    [[nodiscard]] f32 GetRadius(const f32v3 &direction, f32 time) const;
    /// Index of the rest triangle a unit direction passes through.
    [[nodiscard]] u32 GetTriangleAt(const f32v3 &direction) const;
    /// Rest-sphere unit direction of a triangle's centroid.
    [[nodiscard]] f32v3 GetTriangleCenter(u32 triangle) const;

  private:
    Onyx::Resource m_Mesh;
    ShellParams m_Params{};

    TKit::TierArray<f32v3> m_RestDirections{};
    TKit::TierArray<u32v3> m_Triangles{};
    TKit::TierArray<f32v3> m_Centers{};
    TKit::TierArray<Panel> m_Panels{};
    TKit::TierArray<u32> m_PanelColors{};

    TKit::TierArray<f32v3> m_Positions{};
    TKit::TierArray<u32> m_Tints{};
};
} // namespace Lim
