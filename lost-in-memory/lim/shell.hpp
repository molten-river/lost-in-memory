#pragma once

#include "lim/alias.hpp"
#include "lim/palette.hpp"
#include "lim/shell_params.hpp"

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

    void Build(const ShellParams &shell, const ShellMotionParams &motion, const Palette &colors, bool vertexNoise);
    [[nodiscard]] u32 GetTriangleCount() const;
    void SetPanel(u32 triangle, Panel panel, const Onyx::Color &color);
    [[nodiscard]] Panel GetPanel(u32 triangle) const;

    void Update(f32 time);
    void Draw(Onyx::RenderContext<Onyx::D3> &context) const;

    /// Distance from the centre to the current displaced surface along a unit direction.
    [[nodiscard]] f32 GetRadius(const f32v3 &direction) const;
    /// Index of the current displaced triangle a unit direction passes through.
    [[nodiscard]] u32 GetTriangleAt(const f32v3 &direction) const;
    /// Lower bound of the surface radius over all time.
    [[nodiscard]] f32 GetMinRadius() const;
    /// Rest-sphere unit direction of a triangle's centroid.
    [[nodiscard]] f32v3 GetTriangleCenter(u32 triangle) const;

  private:
    void Subdivide();
    void LinkNeighbours();
    void DisplaceByWaves(f32 time);
    void DisplaceByNoise(f32 time);
    /// Descends the rest sphere's subdivision tree, whose child order matches Subdivide.
    [[nodiscard]] u32 FindRestTriangle(const f32v3 &direction) const;

    Onyx::Resource m_Mesh;
    ShellParams m_Shell{};
    ShellMotionParams m_Motion{};
    Palette m_Colors{};
    bool m_VertexNoise = false;
    f32 m_RadialAmplitude = 0.f;
    f32 m_TangentialAmplitude = 0.f;
    f32 m_MinRadius = 0.f;

    TKit::TierArray<f32v3> m_RestDirections{};
    TKit::TierArray<u32v3> m_Triangles{};
    TKit::TierArray<u32v3> m_Neighbours{};
    TKit::TierArray<f32v3> m_Centers{};
    TKit::TierArray<Panel> m_Panels{};
    TKit::TierArray<u32> m_PanelColors{};

    TKit::TierArray<f32v3> m_Positions{};
    TKit::TierArray<f32> m_Heights{};
};
} // namespace Lim
