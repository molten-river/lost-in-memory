#pragma once

#include "alias/alias.hpp"
#include "memory/region.hpp"
#include "shapes/shape.hpp"
#include "shapes/shape_kit_params.hpp"
#include "shapes/shape_renderer.hpp"
#include "shapes/walk_mass_layout.hpp"
#include "viewer/camera_strategy.hpp"
#include "viewer/viewer_params.hpp"

#include "onyx/context.hpp"
#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/memory/ptr.hpp"

#include <cstddef>

namespace Lim
{
/// Bakes every record of a region into a walk mass and lays them out as a grid or shows the selected one alone.
class RecordGallery
{
  public:
    static constexpr u32 GENOME_BYTES = 64;
    static constexpr u32 RECORD_BYTES = u32(WALK_MASS_RECORD_BYTES);
    static constexpr u32 RECORD_COUNT = 63;
    static constexpr usize TAG_BYTE = 0;

    enum class Mode : u8
    {
        Grid,
        Single
    };

    /// The page of the grid that holds the selection, counted from zero, out of Count pages.
    struct Page
    {
        u32 Index;
        u32 Count;
    };

    RecordGallery() = default;

    RecordGallery(const RecordGallery &) = delete;
    RecordGallery &operator=(const RecordGallery &) = delete;

    /// Keeps a copy of the region, so later rebakes reuse the same bytes.
    void Bake(const Region &region, const ShapeKitParams &params);
    void Rebake(const ShapeKitParams &params);
    void ToggleMode();
    /// Moves the selection by offset records, wrapping around; returns whether the grid now shows another page.
    [[nodiscard]] bool MoveSelection(i32 offset, const GalleryParams &layout);
    /// Poses the visible shapes at absolute time seconds.
    void Evolve(f32 seconds, const GalleryParams &layout);
    void Draw(Onyx::RenderContext<Onyx::D3> &context, const ViewerPalette &colors, const GalleryParams &layout) const;

    [[nodiscard]] CameraFocus GetFocus(const GalleryParams &layout) const;
    /// A focus whose radius bounds everything the current mode shows: the selection's page or the selection alone.
    [[nodiscard]] CameraFocus GetSceneFocus(const GalleryParams &layout) const;
    [[nodiscard]] Page GetPage(const GalleryParams &layout) const;
    [[nodiscard]] TKit::Span<const std::byte> GetRecord(u32 index) const;
    [[nodiscard]] u32 GetSelection() const;
    [[nodiscard]] u64 GetSource() const;
    [[nodiscard]] TKit::StringView GetModeName() const;

  private:
    Region m_Region{};
    TKit::FixedArray<TKit::Scope<Shape>, RECORD_COUNT> m_Shapes{};
    ShapeRenderer m_Renderer{};
    u32 m_Selection = 0;
    Mode m_Mode = Mode::Grid;
};
} // namespace Lim
