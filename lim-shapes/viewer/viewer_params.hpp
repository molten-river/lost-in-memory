#pragma once

#include "alias/alias.hpp"
#include "look/ui_theme.hpp"
#include "tuning/param_schema.hpp"

#include "onyx/color.hpp"

#include "tkit/reflection/reflect.hpp"

namespace Lim
{
struct ViewerPalette
{
    TKIT_REFLECT_DECLARE(ViewerPalette)
    Onyx::Color Background = Onyx::Color::FromHexadecimal(0x08090B, false);
    Onyx::Color ShapeLow = Onyx::Color::FromHexadecimal(0x3C4148, false);
    Onyx::Color ShapeHigh = Onyx::Color::FromHexadecimal(0xA4AAB2, false);
    Onyx::Color Selected = Onyx::Color::FromHexadecimal(0x5CC8FF, false);
    Onyx::Color Light = Onyx::Color::FromHexadecimal(0xE4ECF5, false);
    Onyx::Color Ambient = Onyx::Color::FromHexadecimal(0x7C8794, false);
    Onyx::Color UiBackground = Onyx::Color::FromHexadecimal(0x0B0D10, false);
    Onyx::Color UiSurface = Onyx::Color::FromHexadecimal(0x171A1F, false);
    Onyx::Color UiBorder = Onyx::Color::FromHexadecimal(0x343A42, false);
    Onyx::Color UiText = Onyx::Color::FromHexadecimal(0xD6DCE3, false);
    Onyx::Color UiTextDim = Onyx::Color::FromHexadecimal(0x6F7882, false);
};

template <> struct ParamSchema<ViewerPalette>
{
    static constexpr TKit::FixedArray<ParamSpec<ViewerPalette>, 11> FIELDS{{
        {.Member = &ViewerPalette::Background, .Hint = "Clear colour of the void around the shapes"},
        {.Member = &ViewerPalette::ShapeLow, .Hint = "Shape tint at tag byte 0"},
        {.Member = &ViewerPalette::ShapeHigh, .Hint = "Shape tint at tag byte 255"},
        {.Member = &ViewerPalette::Selected, .Hint = "Tint of the selected record in the grid, and the HUD accent"},
        {.Member = &ViewerPalette::Light, .Hint = "Colour of the directional light"},
        {.Member = &ViewerPalette::Ambient, .Hint = "Colour of the ambient light"},
        {.Member = &ViewerPalette::UiBackground, .Hint = "Panel and HUD background"},
        {.Member = &ViewerPalette::UiSurface, .Hint = "Widget fill"},
        {.Member = &ViewerPalette::UiBorder, .Hint = "Borders, separators and buttons"},
        {.Member = &ViewerPalette::UiText, .Hint = "Regular panel and HUD text"},
        {.Member = &ViewerPalette::UiTextDim, .Hint = "Section titles, hints and bytes the kit never reads"},
    }};
};

struct ViewerLook
{
    TKIT_REFLECT_DECLARE(ViewerLook)
    ViewerPalette Colors;
    f32 AmbientIntensity = 0.35f;
    f32 LightIntensity = 3.f;
    f32 TextSize = 16.f;
};

template <> struct ParamSchema<ViewerLook>
{
    static constexpr DragStyle LIGHT_STRENGTH{0.02f, TWO_DECIMALS_FORMAT};

    static constexpr TKit::FixedArray<ParamSpec<ViewerLook>, 3> FIELDS{{
        {.Member = &ViewerLook::AmbientIntensity,
         .Hint = "Strength of the ambient light; Onyx clamps it at 1",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = UNIT_SHARE},
        {.Member = &ViewerLook::LightIntensity,
         .Hint = "Strength of the directional light",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 20.f,
         .Drag = LIGHT_STRENGTH},
        {.Member = &ViewerLook::TextSize,
         .Hint = "Panel and HUD text height at 1080 lines, scaled with the window",
         .Flags = DRAGGED_BOUNDS,
         .Min = 10.f,
         .Max = 28.f,
         .Drag = TEXT_POINTS},
    }};
};

struct CameraParams
{
    TKIT_REFLECT_DECLARE(CameraParams)
    f32 FlyThrust = 60.f;
    f32 FlyMaxSpeed = 40.f;
    f32 FlyVelocityKeptPerSecond = 0.02f;
    f32 FlyRollSpeed = 1.5f;
    f32 MouseRadiansPerPixel = 0.0025f;
    f32 OrbitDistance = 2.5f;
    f32 OrbitSpeed = 1.f;
};

template <> struct ParamSchema<CameraParams>
{
    static constexpr DragStyle FLY_RATE{0.1f, ONE_DECIMAL_FORMAT};
    static constexpr DragStyle LOOK_RADIANS{0.00002f, "{:.5f}"};

    static constexpr TKit::FixedArray<ParamSpec<CameraParams>, 7> FIELDS{{
        {.Member = &CameraParams::FlyThrust,
         .Hint = "Fly acceleration while a move key is held",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 500.f,
         .Drag = FLY_RATE},
        {.Member = &CameraParams::FlyMaxSpeed,
         .Hint = "Fly speed cap",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.1f,
         .Max = 500.f,
         .Drag = FLY_RATE},
        {.Member = &CameraParams::FlyVelocityKeptPerSecond,
         .Hint = "Damping: share of fly velocity left after one second without thrust",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 1.f,
         .Drag = PINCH_SHARE},
        {.Member = &CameraParams::FlyRollSpeed,
         .Hint = "Fly roll rate on Q and E, in radians per second",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 10.f,
         .Drag = SCALE_FACTOR},
        {.Member = &CameraParams::MouseRadiansPerPixel,
         .Hint = "Mouse look and orbit drag sensitivity",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.0001f,
         .Max = 0.02f,
         .Drag = LOOK_RADIANS},
        {.Member = &CameraParams::OrbitDistance,
         .Hint = "Orbit distance when orbiting starts, as a multiple of the selected shape's bounding radius",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.5f,
         .Max = 20.f,
         .Drag = SCALE_FACTOR},
        {.Member = &CameraParams::OrbitSpeed,
         .Hint = "Orbit distance doublings per second while W or S is held",
         .Flags = DRAGGED_BOUNDS,
         .Min = 0.f,
         .Max = 8.f,
         .Drag = SCALE_FACTOR},
    }};
};

struct GalleryParams
{
    TKIT_REFLECT_DECLARE(GalleryParams)
    u32 Columns = 9;
    u32 Rows = 7;
    f32 Spacing = 36.f;
};

template <> struct ParamSchema<GalleryParams>
{
    static constexpr TKit::FixedArray<ParamSpec<GalleryParams>, 3> FIELDS{{
        {.Member = &GalleryParams::Columns,
         .Hint = "Cells per grid row",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 63.f,
         .Drag = GRID_CELLS},
        {.Member = &GalleryParams::Rows,
         .Hint = "Grid rows; when the cells hold fewer than 63 records, the grid shows the selection's page",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 63.f,
         .Drag = GRID_CELLS},
        {.Member = &GalleryParams::Spacing,
         .Hint = "Distance between neighbouring grid cells",
         .Flags = DRAGGED_BOUNDS,
         .Min = 1.f,
         .Max = 100.f,
         .Drag = GRID_SPACING},
    }};
};

struct ViewerParams
{
    TKIT_REFLECT_DECLARE(ViewerParams)
    ViewerLook Look;
    CameraParams Camera;
    GalleryParams Gallery;
};

[[nodiscard]] inline UiTheme ToUiTheme(const ViewerPalette &palette)
{
    return UiTheme{.Background = palette.UiBackground,
                   .Surface = palette.UiSurface,
                   .Border = palette.UiBorder,
                   .Text = palette.UiText,
                   .TextDim = palette.UiTextDim};
}
} // namespace Lim
