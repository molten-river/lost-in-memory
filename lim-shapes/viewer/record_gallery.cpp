#include "viewer/record_gallery.hpp"

#include "shapes/shape_factory.hpp"
#include "shapes/shape_instance.hpp"
#include "shapes/shape_kind.hpp"

#include "onyx/transform.hpp"
#include "tkit/math/math.hpp"

namespace Lim
{
namespace
{
constexpr u32 MODE_COUNT = u32(RecordGallery::Mode::Single) + 1;
constexpr TKit::FixedArray<TKit::StringView, MODE_COUNT> MODE_NAMES{"GRID", "SINGLE"};

static_assert(RecordGallery::GENOME_BYTES + RecordGallery::RECORD_COUNT * RecordGallery::RECORD_BYTES == REGION_BYTES,
              "The genome and the records must fill the region exactly");
static_assert(RecordGallery::RECORD_COUNT <= ShapeRenderer::MAX_SHAPES, "The renderer must hold every record");

/// Rows x Columns cells showing the page that holds the selection; cells past the last record stay empty.
struct PageGrid
{
    u32 Columns;
    u32 Rows;
    u32 First;
    u32 Visible;
    u32 Index;
    u32 Count;
};

[[nodiscard]] PageGrid MakePageGrid(const u32 selection, const GalleryParams &layout)
{
    const u32 columns = TKit::Math::Max(layout.Columns, 1U);
    const u32 rows = TKit::Math::Max(layout.Rows, 1U);
    const u32 cells = columns * rows;
    const u32 index = selection / cells;
    const u32 first = index * cells;
    return PageGrid{.Columns = columns,
                    .Rows = rows,
                    .First = first,
                    .Visible = TKit::Math::Min(cells, RecordGallery::RECORD_COUNT - first),
                    .Index = index,
                    .Count = (RecordGallery::RECORD_COUNT + cells - 1) / cells};
}

/// Cells read top to bottom and left to right, centred on the origin in the XY plane.
[[nodiscard]] f32v3 CellPosition(const u32 cell, const PageGrid &grid, const f32 spacing)
{
    const f32 column = f32(cell % grid.Columns) - 0.5f * f32(grid.Columns - 1);
    const f32 row = 0.5f * f32(grid.Rows - 1) - f32(cell / grid.Columns);
    return f32v3{column * spacing, row * spacing, 0.f};
}
} // namespace

void RecordGallery::Bake(const Region &region, const ShapeKitParams &params)
{
    m_Region = region;
    Rebake(params);
}

void RecordGallery::Rebake(const ShapeKitParams &params)
{
    for (u32 i = 0; i < RECORD_COUNT; ++i)
        m_Shapes[i] = ShapeFactory::Create(ShapeKind::WalkMass, GetRecord(i), params);
}

void RecordGallery::ToggleMode()
{
    m_Mode = Mode((u32(m_Mode) + 1) % MODE_COUNT);
}

bool RecordGallery::MoveSelection(const i32 offset, const GalleryParams &layout)
{
    const u32 previousPage = MakePageGrid(m_Selection, layout).Index;
    const i32 count = i32(RECORD_COUNT);
    m_Selection = u32(((i32(m_Selection) + offset) % count + count) % count);
    return m_Mode == Mode::Grid && MakePageGrid(m_Selection, layout).Index != previousPage;
}

void RecordGallery::Rewrite(ShapeRenderer &renderer, const f32 seconds, const ViewerPalette &colors,
                            const GalleryParams &layout)
{
    const bool single = m_Mode == Mode::Single;
    const PageGrid grid = MakePageGrid(m_Selection, layout);
    const u32 first = single ? m_Selection : grid.First;
    const u32 count = single ? 1 : grid.Visible;

    TKit::FixedArray<ShapeInstance, RECORD_COUNT> instances;
    for (u32 slot = 0; slot < count; ++slot)
    {
        const u32 index = first + slot;
        Shape &shape = *m_Shapes[index];
        shape.Evolve(seconds);

        const f32 tag = f32(std::to_integer<u32>(GetRecord(index)[TAG_BYTE])) / f32(WALK_MASS_BYTE_MAX);
        const Onyx::Color tint =
            !single && index == m_Selection ? colors.Selected : Mix(colors.ShapeLow, colors.ShapeHigh, tag);
        const f32v3 position = single ? f32v3{0.f} : CellPosition(slot, grid, layout.Spacing);
        instances[slot] = ShapeInstance{.Shape = &shape,
                                        .Transform = Onyx::Transform<Onyx::D3>::ComputeTransform(
                                            position, f32v3{1.f}, Onyx::RotType<Onyx::D3>::Identity),
                                        .PackedColor = tint.ToLinear().Pack()};
    }
    renderer.Rewrite(TKit::Span<const ShapeInstance>(instances.GetData(), count));
}

CameraFocus RecordGallery::GetFocus(const GalleryParams &layout) const
{
    const PageGrid grid = MakePageGrid(m_Selection, layout);
    const f32v3 target =
        m_Mode == Mode::Single ? f32v3{0.f} : CellPosition(m_Selection - grid.First, grid, layout.Spacing);
    return CameraFocus{.Target = target, .Radius = m_Shapes[m_Selection]->GetBoundingRadius()};
}

CameraFocus RecordGallery::GetSceneFocus(const GalleryParams &layout) const
{
    if (m_Mode == Mode::Single)
        return GetFocus(layout);

    const PageGrid grid = MakePageGrid(m_Selection, layout);
    const f32 halfWidth = 0.5f * f32(grid.Columns - 1) * layout.Spacing;
    const f32 halfHeight = 0.5f * f32(grid.Rows - 1) * layout.Spacing;
    const f32 reach = TKit::Math::SquareRoot(halfWidth * halfWidth + halfHeight * halfHeight);
    return CameraFocus{.Target = f32v3{0.f}, .Radius = reach + 0.5f * layout.Spacing};
}

RecordGallery::Page RecordGallery::GetPage(const GalleryParams &layout) const
{
    const PageGrid grid = MakePageGrid(m_Selection, layout);
    return Page{.Index = grid.Index, .Count = grid.Count};
}

TKit::Span<const std::byte> RecordGallery::GetRecord(const u32 index) const
{
    return TKit::Span<const std::byte>(m_Region.GetBytes().GetData() + GENOME_BYTES + RECORD_BYTES * index,
                                       RECORD_BYTES);
}

u32 RecordGallery::GetSelection() const
{
    return m_Selection;
}

u64 RecordGallery::GetSource() const
{
    return m_Region.GetAddress();
}

TKit::StringView RecordGallery::GetModeName() const
{
    return MODE_NAMES[u32(m_Mode)];
}
} // namespace Lim
