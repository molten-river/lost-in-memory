#include "lim/sandbox_screen.hpp"

#include "lim/palette.hpp"

#include "tkit/container/fixed_array.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/math.hpp"
#include "tkit/reflection/lim/form_params.hpp"
#include "tkit/reflection/lim/formula_params.hpp"
#include "tkit/reflection/lim/level_params.hpp"
#include "tkit/reflection/lim/palette.hpp"
#include "tkit/reflection/lim/player_params.hpp"
#include "tkit/reflection/lim/shape_params.hpp"
#include "tkit/reflection/lim/shell_params.hpp"

namespace Lim
{
namespace
{
constexpr TKit::StringView WINDOW_LABEL = "lim-sandbox";
constexpr TKit::StringView TAB_BAR_ID = "lim-tuning-tabs";
constexpr TKit::StringView SCROLL_LABEL = "lim-tuning-scroll";
constexpr Onyx::OverlayWindowFlags WINDOW_FLAGS =
    Onyx::OverlayWindowFlag_NoMove | Onyx::OverlayWindowFlag_NoResize | Onyx::OverlayWindowFlag_AutoResize |
    Onyx::OverlayWindowFlag_NoHeaderBar | Onyx::OverlayWindowFlag_NoCollapse | Onyx::OverlayWindowFlag_NoScrollBar;
constexpr Onyx::OverlayTabFlags TAB_FLAGS = Onyx::OverlayTabFlag_StartOpen;
constexpr Onyx::OverlayColorFlags COLOR_FLAGS =
    Onyx::OverlayColorFlag_NoAlpha | Onyx::OverlayColorFlag_NoColorMarkers | Onyx::OverlayColorFlag_NoTooltip;
constexpr Onyx::OverlayFocusFlags HINT_FLAGS = Onyx::OverlayFocusFlag_ShortDelay;

constexpr f32 TITLE_SCALE = 1.5f;

constexpr f32 REFERENCE_SCREEN_HEIGHT = 1080.f;
constexpr f32 SCROLL_HEIGHT_SHARE = 0.55f;
constexpr f32 BORDER_WIDTH = 1.f;
constexpr f32 WINDOW_PADDING = 16.f;
constexpr f32 WIDGET_WIDTH = 280.f;
constexpr f32 HOVER_TINT = 0.25f;
constexpr f32 PRESS_TINT = 0.5f;

constexpr TKit::FixedArray<Onyx::OverlayStyleVariable, 26> SCALED_VARIABLES{Onyx::OverlayStyle_UnicodeSize,
                                                                            Onyx::OverlayStyle_IndentWidth,
                                                                            Onyx::OverlayStyle_ChildGap,
                                                                            Onyx::OverlayStyle_TabPadding,
                                                                            Onyx::OverlayStyle_TabGap,
                                                                            Onyx::OverlayStyle_TabBarWidth,
                                                                            Onyx::OverlayStyle_LineWidth,
                                                                            Onyx::OverlayStyle_SeparatorTextOffset,
                                                                            Onyx::OverlayStyle_WindowPadding,
                                                                            Onyx::OverlayStyle_WindowBorderWidth,
                                                                            Onyx::OverlayStyle_HeaderPadding,
                                                                            Onyx::OverlayStyle_IconWidth,
                                                                            Onyx::OverlayStyle_ContentAreaPadding,
                                                                            Onyx::OverlayStyle_ScrollBarWidth,
                                                                            Onyx::OverlayStyle_ScrollBarGap,
                                                                            Onyx::OverlayStyle_ScrollSensitivity,
                                                                            Onyx::OverlayStyle_WidgetSize,
                                                                            Onyx::OverlayStyle_WidgetPadding,
                                                                            Onyx::OverlayStyle_WidgetMinimumWidth,
                                                                            Onyx::OverlayStyle_MenuPadding,
                                                                            Onyx::OverlayStyle_DropDownHeightRegular,
                                                                            Onyx::OverlayStyle_DropDownHeightSmall,
                                                                            Onyx::OverlayStyle_TooltipPadding,
                                                                            Onyx::OverlayStyle_ColorTooltipSize,
                                                                            Onyx::OverlayStyle_ColorPickerSize,
                                                                            Onyx::OverlayStyle_ColorPickerPreviewSize};

constexpr usize LABEL_CAPACITY = 48;
using Label = TKit::StaticString<LABEL_CAPACITY>;

constexpr const char *INTEGER = "{}";
constexpr const char *WHOLE = "{:.0f}";
constexpr const char *ONE_DECIMAL = "{:.1f}";
constexpr const char *TWO_DECIMALS = "{:.2f}";
constexpr const char *THREE_DECIMALS = "{:.3f}";
constexpr TKit::FixedArray<const char *, 5> REAL_FORMATS{ONE_DECIMAL, TWO_DECIMALS, THREE_DECIMALS, "{:.4f}", "{:.5f}"};
constexpr f32 DECADE = 10.f;

constexpr f32 REFERENCE_SHARE_PER_PIXEL = 0.005f;
constexpr f32 RANGE_SCALE = 4.f;
constexpr f32 INTEGER_DRAG_SPEED = 0.05f;
constexpr f32 COARSE_INTEGER_DRAG_SPEED = 0.25f;

struct DragRange
{
    f32 Min;
    f32 Max;
    f32 Speed;
    const char *Format;
};

constexpr DragRange UNIT_SHARE{0.f, 1.f, REFERENCE_SHARE_PER_PIXEL, TWO_DECIMALS};
constexpr DragRange FORM_SIZE{0.1f, 10.f, 0.02f, TWO_DECIMALS};
constexpr DragRange SIZE_FRACTION{0.01f, 1.f, 0.002f, TWO_DECIMALS};
constexpr DragRange SCALE_FACTOR{0.05f, 3.f, 0.005f, TWO_DECIMALS};
constexpr DragRange PERIOD_SECONDS{0.1f, 60.f, 0.05f, ONE_DECIMAL};
constexpr DragRange GRID_CELLS{1.f, 63.f, COARSE_INTEGER_DRAG_SPEED, INTEGER};

/// Keyed by the name of the member holding the struct plus the field name, because reflection carries no type names.
struct DragOverride
{
    TKit::StringView Section;
    TKit::StringView Field;
    DragRange Range;
};

constexpr TKit::FixedArray<DragOverride, 35> DRAG_OVERRIDES{{
    {"LookFeel", "TextSize", {10.f, 28.f, INTEGER_DRAG_SPEED, WHOLE}},
    {"ShellMotion", "TangentialShare", UNIT_SHARE},
    {"Shapes", "MeshResolution", {8.f, 64.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Shell", "Subdivisions", {1.f, 5.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Shapes", "CarveChance", UNIT_SHARE},
    {"Physics", "Restitution", UNIT_SHARE},
    {"Physics", "Substeps", {1.f, 8.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Records", "LethalTagMax", {1.f, 254.f, COARSE_INTEGER_DRAG_SPEED, INTEGER}},
    {"Records", "EnemySizeMin", FORM_SIZE},
    {"Records", "EnemySizeMax", FORM_SIZE},
    {"Records", "SolidSizeMin", FORM_SIZE},
    {"Records", "SolidSizeMax", FORM_SIZE},
    {"Records", "GoalSize", FORM_SIZE},
    {"Records", "LatticeStepMin", SIZE_FRACTION},
    {"Records", "LatticeStepMax", SIZE_FRACTION},
    {"Records", "ProfileRadiusMin", SIZE_FRACTION},
    {"Records", "ProfileRadiusMax", SIZE_FRACTION},
    {"Records", "SectionScaleMin", SCALE_FACTOR},
    {"Records", "SectionScaleMax", SCALE_FACTOR},
    {"Records", "ReachBiasMin", SCALE_FACTOR},
    {"Records", "ReachBiasMax", SCALE_FACTOR},
    {"Graph", "JoinReach", SCALE_FACTOR},
    {"Graph", "BreakReach", SCALE_FACTOR},
    {"Graph", "MaxLinks", {1.f, 7.f, INTEGER_DRAG_SPEED, INTEGER}},
    {"Graph", "SlabDepth", SIZE_FRACTION},
    {"Graph", "PinchScale", {0.f, 0.5f, 0.001f, THREE_DECIMALS}},
    {"Evolution", "PeriodMin", PERIOD_SECONDS},
    {"Evolution", "PeriodMax", PERIOD_SECONDS},
    {"Evolution", "SwayMax", UNIT_SHARE},
    {"Evolution", "PulseMax", UNIT_SHARE},
    {"Evolution", "Speed", {0.f, 5.f, 0.01f, TWO_DECIMALS}},
    {"TestBench", "GridCount", GRID_CELLS},
    {"TestBench", "GridColumns", GRID_CELLS},
    {"TestBench", "GridSpacing", {1.f, 50.f, 0.05f, ONE_DECIMAL}},
    {"TestBench", "GridDistance", {1.f, 200.f, 0.1f, ONE_DECIMAL}},
}};

using ScopeFlags = u8;
enum ScopeFlagBit : ScopeFlags
{
    ScopeFlag_Main = 1U << u8(PlayMode::Main),
    ScopeFlag_Shapes = 1U << u8(PlayMode::TestShapes),
    ScopeFlag_Sphere = 1U << u8(PlayMode::TestSphere)
};

constexpr ScopeFlags MAIN_ONLY = ScopeFlag_Main;
constexpr ScopeFlags SHAPES_ONLY = ScopeFlag_Shapes;
constexpr ScopeFlags WITH_SHAPES = ScopeFlag_Main | ScopeFlag_Shapes;
constexpr ScopeFlags WITH_SPHERE = ScopeFlag_Main | ScopeFlag_Sphere;
constexpr ScopeFlags EVERY_MODE = ScopeFlag_Main | ScopeFlag_Shapes | ScopeFlag_Sphere;

/// A feature flag, looked up by name in the reflected FeatureFlags, that must hold the given value.
struct FlagCondition
{
    TKit::StringView Flag;
    bool Enabled;
};

constexpr TKit::StringView FLAGS_SECTION = "Flags";
constexpr TKit::Span<const FlagCondition> UNCONDITIONAL{};
constexpr TKit::FixedArray<FlagCondition, 1> DECODED{FlagCondition{"StructuralDecode", true}};
constexpr TKit::FixedArray<FlagCondition, 1> GRAMMAR{FlagCondition{"StructuralDecode", false}};
constexpr TKit::FixedArray<FlagCondition, 2> CARVED_GRAMMAR{FlagCondition{"StructuralDecode", false},
                                                            FlagCondition{"CarvedShapes", true}};
constexpr TKit::FixedArray<FlagCondition, 2> SMOOTH_GRAMMAR{FlagCondition{"StructuralDecode", false},
                                                            FlagCondition{"CarvedShapes", false}};
constexpr TKit::FixedArray<FlagCondition, 1> NOISE_SHELL{FlagCondition{"ShellVertexNoise", true}};
constexpr TKit::FixedArray<FlagCondition, 1> WAVE_SHELL{FlagCondition{"ShellVertexNoise", false}};

struct Visibility
{
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

/// The default for every field of a section; sections not listed show everywhere, unconditionally.
struct SectionRule
{
    TKit::StringView Section;
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

/// Replaces the section default for one field.
struct FieldRule
{
    TKit::StringView Section;
    TKit::StringView Field;
    ScopeFlags Scope;
    TKit::Span<const FlagCondition> Conditions;
};

constexpr TKit::FixedArray<SectionRule, 11> SECTION_RULES{{
    {"ShellMotion", WITH_SPHERE, NOISE_SHELL},
    {"Shapes", WITH_SHAPES, CARVED_GRAMMAR},
    {"Shell", WITH_SPHERE, UNCONDITIONAL},
    {"Enemies", MAIN_ONLY, UNCONDITIONAL},
    {"Solids", MAIN_ONLY, UNCONDITIONAL},
    {"Goal", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", WITH_SHAPES, UNCONDITIONAL},
    {"Records", WITH_SHAPES, DECODED},
    {"Graph", WITH_SHAPES, DECODED},
    {"Evolution", WITH_SHAPES, DECODED},
    {"TestBench", SHAPES_ONLY, UNCONDITIONAL},
}};

constexpr TKit::FixedArray<FieldRule, 44> FIELD_RULES{{
    {"Colors", "ShellTrough", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "ShellCrest", WITH_SPHERE, UNCONDITIONAL},
    {"Colors", "SolidLow", WITH_SHAPES, UNCONDITIONAL},
    {"Colors", "SolidHigh", WITH_SHAPES, UNCONDITIONAL},
    {"ShellMotion", "WaveAmplitude", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveSpeed", WITH_SPHERE, WAVE_SHELL},
    {"ShellMotion", "WaveFrequency", WITH_SPHERE, WAVE_SHELL},
    {"Shapes", "MeshResolution", WITH_SHAPES, GRAMMAR},
    {"Shapes", "SmoothSpread", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothSphereRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfWidth", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfHeight", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothBoxHalfDepth", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothCapsuleRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothCapsuleHalfLength", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothTorusMajorRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Shapes", "SmoothTorusMinorRadius", WITH_SHAPES, SMOOTH_GRAMMAR},
    {"Enemies", "MinCount", MAIN_ONLY, GRAMMAR},
    {"Enemies", "MaxCount", MAIN_ONLY, GRAMMAR},
    {"Enemies", "MinSize", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MaxSize", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Enemies", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Enemies", "BlendRadius", WITH_SHAPES, GRAMMAR},
    {"Solids", "MinCount", MAIN_ONLY, GRAMMAR},
    {"Solids", "MaxCount", MAIN_ONLY, GRAMMAR},
    {"Solids", "MinSize", WITH_SHAPES, GRAMMAR},
    {"Solids", "MaxSize", WITH_SHAPES, GRAMMAR},
    {"Solids", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Solids", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Solids", "BlendRadius", WITH_SHAPES, GRAMMAR},
    {"Goal", "ShellChance", MAIN_ONLY, GRAMMAR},
    {"Goal", "Size", WITH_SHAPES, GRAMMAR},
    {"Goal", "MinPrimitives", WITH_SHAPES, GRAMMAR},
    {"Goal", "MaxPrimitives", WITH_SHAPES, GRAMMAR},
    {"Physics", "WallMargin", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MaxTumbleScale", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "MinSafeSpawnRadius", MAIN_ONLY, UNCONDITIONAL},
    {"Physics", "SafeSpawnFraction", MAIN_ONLY, UNCONDITIONAL},
    {"Flags", "ShellVertexNoise", WITH_SPHERE, UNCONDITIONAL},
    {"Flags", "CarvedShapes", WITH_SHAPES, GRAMMAR},
    {"Flags", "StructuralDecode", WITH_SHAPES, UNCONDITIONAL},
    {"Flags", "ChainLinks", WITH_SHAPES, DECODED},
}};

struct FieldHint
{
    TKit::StringView Section;
    TKit::StringView Field;
    TKit::StringView Hint;
};

constexpr TKit::FixedArray<FieldHint, 134> HINTS{{
    {"Colors", "VoidBlack", "Clear colour of the void around everything"},
    {"Colors", "Untinted", "Neutral fill the shell and its plain panels start from"},
    {"Colors", "ShellTrough", "Shell tint at the lowest displacement"},
    {"Colors", "ShellCrest", "Shell tint at the highest displacement"},
    {"Colors", "EnemyPurple", "Lethal tint at tint fraction 0, for enemies and lethal panels"},
    {"Colors", "EnemyRed", "Lethal tint at tint fraction 1, for enemies and lethal panels"},
    {"Colors", "GoalGreen", "Tint of the goal form or goal panel"},
    {"Colors", "SolidLow", "Solid tint at tint fraction 0"},
    {"Colors", "SolidHigh", "Solid tint at tint fraction 1"},
    {"Colors", "CoreLight", "Colour of the point light at the centre"},
    {"Colors", "Ambient", "Colour of the ambient light"},
    {"Colors", "UiBackground", "Panel background"},
    {"Colors", "UiSurface", "Widget fill"},
    {"Colors", "UiBorder", "Borders, separators and buttons"},
    {"Colors", "UiText", "Regular panel text"},
    {"Colors", "UiTextDim", "Section titles, hints and hover tints"},
    {"ShellMotion", "WaveAmplitude", "Radial height of the travelling waves, in world units"},
    {"ShellMotion", "WaveSpeed", "Wave cycles per second"},
    {"ShellMotion", "WaveFrequency", "Wave crests across the sphere"},
    {"ShellMotion", "NoiseAmplitude", "Total vertex displacement by noise, in world units"},
    {"ShellMotion", "NoiseSpeed", "How fast the noise field drifts"},
    {"ShellMotion", "NoiseScale", "Noise features per sphere; higher means smaller bumps"},
    {"ShellMotion", "TangentialShare", "Share of the noise displacement that slides along the surface"},
    {"ShellMotion", "FacetTintStrength", "How strongly facet height drives the trough-to-crest tint"},
    {"Shapes", "MeshResolution", "Grid cells per axis when meshing a blob form"},
    {"Player", "Radius", "Collision radius of the player"},
    {"Player", "Thrust", "Acceleration while a move key is held"},
    {"Player", "MaxSpeed", "Speed cap of the player"},
    {"Player", "VelocityKeptPerSecond", "Share of velocity left after one second without thrust"},
    {"Player", "MouseRadiansPerPixel", "Look sensitivity"},
    {"Player", "RollSpeed", "Roll rate on Q and E, in radians per second"},
    {"Light", "AmbientIntensity", "Strength of the ambient light"},
    {"Light", "CoreIntensity", "Strength of the point light at the centre"},
    {"Light", "CoreRadiusScale", "Point light radius as a multiple of the shell radius"},
    {"LookFeel", "FadeSeconds", "Duration of each fade in and out"},
    {"LookFeel", "TextSize", "Panel text height at 1080 lines, scaled with the screen"},
    {"Shell", "Radius", "Rest radius of the sphere"},
    {"Shell", "Subdivisions", "Icosphere subdivisions; each level quadruples the panels"},
    {"Shell", "LethalDensity", "Chance that a shell panel is lethal"},
    {"Shapes", "CarveChance", "Chance a spare primitive cuts into the core instead of attaching"},
    {"Shapes", "OpeningMargin", "Smallest opening as a multiple of the player diameter"},
    {"Shapes", "HardBlendScale", "Blend radius multiplier for machined forms"},
    {"Shapes", "TrimChance", "Chance a trimmable core gets a trim piece"},
    {"Shapes", "MonolithWeight", "Pick weight of a solid monolith core"},
    {"Shapes", "PortalWeight", "Pick weight of a framed portal core"},
    {"Shapes", "RingWeight", "Pick weight of a ring core"},
    {"Shapes", "CageWeight", "Pick weight of a lattice cage core"},
    {"Shapes", "GateWeight", "Pick weight of a pillar and lintel gate core"},
    {"Shapes", "WallFraction", "Wall thickness and snapping step, as a fraction of size"},
    {"Shapes", "MinSpanFraction", "Shortest core span, as a fraction of size"},
    {"Shapes", "MaxSpanFraction", "Longest core span, as a fraction of size"},
    {"Shapes", "MinDepthFraction", "Thinnest core depth, as a fraction of size"},
    {"Shapes", "MaxDepthFraction", "Thickest core depth, as a fraction of size"},
    {"Shapes", "MinWindowFraction", "Narrowest opening, as a fraction of size"},
    {"Shapes", "MaxWindowFraction", "Widest opening, as a fraction of size"},
    {"Shapes", "AttachmentReach", "Farthest an attachment sits from the core, as a fraction of size"},
    {"Shapes", "NotchFraction", "Deepest notch in a core edge, as a share of its extent"},
    {"Shapes", "GrooveDepthFraction", "Groove depth as a share of the core depth"},
    {"Shapes", "SmoothSpread", "How far blob primitives scatter from the centre, as a fraction of size"},
    {"Shapes", "SmoothSphereRadius", "Sphere radius, as a fraction of size"},
    {"Shapes", "SmoothBoxHalfWidth", "Box half width, as a fraction of size"},
    {"Shapes", "SmoothBoxHalfHeight", "Box half height, as a fraction of size"},
    {"Shapes", "SmoothBoxHalfDepth", "Box half depth, as a fraction of size"},
    {"Shapes", "SmoothCapsuleRadius", "Capsule radius, as a fraction of size"},
    {"Shapes", "SmoothCapsuleHalfLength", "Capsule half length, as a fraction of size"},
    {"Shapes", "SmoothTorusMajorRadius", "Torus ring radius, as a fraction of size"},
    {"Shapes", "SmoothTorusMinorRadius", "Torus tube radius, as a fraction of size"},
    {"Enemies", "MinCount", "Fewest enemies the byte stream can spawn"},
    {"Enemies", "MaxCount", "Most enemies the byte stream can spawn"},
    {"Enemies", "MinSize", "Smallest enemy size"},
    {"Enemies", "MaxSize", "Largest enemy size"},
    {"Enemies", "MinPrimitives", "Fewest primitives in one enemy"},
    {"Enemies", "MaxPrimitives", "Most primitives in one enemy"},
    {"Enemies", "BlendRadius", "Smooth union radius of enemies and the goal form"},
    {"Enemies", "MinDriftSpeed", "Enemy drift speed at speed fraction 0"},
    {"Enemies", "MaxDriftSpeed", "Enemy drift speed at speed fraction 1"},
    {"Enemies", "TumbleSpeed", "Enemy spin rate before the tumble scale"},
    {"Solids", "MinCount", "Fewest solids the byte stream can spawn"},
    {"Solids", "MaxCount", "Most solids the byte stream can spawn"},
    {"Solids", "MinSize", "Smallest solid size"},
    {"Solids", "MaxSize", "Largest solid size"},
    {"Solids", "MinPrimitives", "Fewest primitives in one solid"},
    {"Solids", "MaxPrimitives", "Most primitives in one solid"},
    {"Solids", "BlendRadius", "Smooth union radius of solids"},
    {"Solids", "MinDriftSpeed", "Solid drift speed at speed fraction 0"},
    {"Solids", "MaxDriftSpeed", "Solid drift speed at speed fraction 1"},
    {"Solids", "TumbleSpeed", "Solid spin rate before the tumble scale"},
    {"Goal", "ShellChance", "Chance the goal is a shell panel instead of a form"},
    {"Goal", "Size", "Size of the goal form"},
    {"Goal", "DriftSpeed", "Drift speed of the goal form"},
    {"Goal", "MinSpawnDistance", "Nearest the goal form spawns to the start"},
    {"Goal", "MinPrimitives", "Fewest primitives in the goal form"},
    {"Goal", "MaxPrimitives", "Most primitives in the goal form"},
    {"Physics", "Restitution", "Bounciness of every collision; 1 keeps all energy"},
    {"Physics", "Density", "Mass per unit volume of every form"},
    {"Physics", "AngularDamping", "Share of spin lost per second"},
    {"Physics", "Substeps", "Physics steps per frame"},
    {"Physics", "WallMargin", "Gap kept between spawned forms and the shell"},
    {"Physics", "MinTumbleScale", "Tumble multiplier at tumble fraction 0"},
    {"Physics", "MaxTumbleScale", "Tumble multiplier at tumble fraction 1"},
    {"Physics", "MinSafeSpawnRadius", "Smallest clear radius around the start"},
    {"Physics", "SafeSpawnFraction", "Clear radius around the start, as a share of the shell radius"},
    {"Records", "LethalTagMax", "Tags 1 to this are enemies; higher tags are solids"},
    {"Records", "EnemySizeMin", "Enemy size at size byte 0"},
    {"Records", "EnemySizeMax", "Enemy size at size byte 255"},
    {"Records", "SolidSizeMin", "Solid size at size byte 0"},
    {"Records", "SolidSizeMax", "Solid size at size byte 255"},
    {"Records", "GoalSize", "Size of the goal record's form"},
    {"Records", "LatticeStepMin", "Section snapping step at step bits 0, as a fraction of size"},
    {"Records", "LatticeStepMax", "Section snapping step at step bits 31, as a fraction of size"},
    {"Records", "ProfileRadiusMin", "Profile vertex radius at nibble 0, as a fraction of size"},
    {"Records", "ProfileRadiusMax", "Profile vertex radius at nibble 15, as a fraction of size"},
    {"Records", "SectionScaleMin", "Section scale at scale nibble 1"},
    {"Records", "SectionScaleMax", "Section scale at scale nibble 15"},
    {"Records", "ReachBiasMin", "Join and break reach multiplier at reach byte 0"},
    {"Records", "ReachBiasMax", "Join and break reach multiplier at reach byte 255"},
    {"Graph", "JoinReach", "Sections closer than this times size link up"},
    {"Graph", "BreakReach", "Linked sections farther than this times size break apart"},
    {"Graph", "MaxLinks", "Most links one section can hold"},
    {"Graph", "SlabDepth", "Thickness of an unlinked section's slab, as a fraction of size"},
    {"Graph", "PinchScale", "Below this share of its rest scale a section drops its links"},
    {"Evolution", "PeriodMin", "Motion period at period nibble 0, in seconds"},
    {"Evolution", "PeriodMax", "Motion period at period nibble 15, in seconds"},
    {"Evolution", "SwayMax", "Sway distance at sway nibble 15, as a fraction of size"},
    {"Evolution", "PulseMax", "Scale pinch at pulse nibble 15; 1 pinches to nothing"},
    {"Evolution", "Speed", "Time multiplier for every section's motion"},
    {"TestBench", "GridCount", "Records laid out on the test grid"},
    {"TestBench", "GridColumns", "Columns of the test grid"},
    {"TestBench", "GridSpacing", "Distance between neighbouring grid cells"},
    {"TestBench", "GridDistance", "Distance from the start to the grid plane"},
    {"Flags", "ShellVertexNoise", "Shell moves by drifting noise instead of travelling waves"},
    {"Flags", "CarvedShapes", "Byte stream builds machined forms instead of smooth blobs"},
    {"Flags", "StructuralDecode", "Forms come from the record layout instead of the byte stream"},
    {"Flags", "ChainLinks", "Sections link in slot order instead of by proximity"},
}};

struct TabName
{
    TKit::StringView Field;
    TKit::StringView Title;
};

constexpr TKit::FixedArray<TabName, 3> TAB_NAMES{{
    {"LookFeel", "Look & Feel"},
    {"Mechanics", "Core Mechanics"},
    {"Flags", "Feature flags"},
}};

struct Heading
{
    SandboxScreen::Title Title;
    TKit::StringView Text;
};

constexpr TKit::FixedArray<Heading, 3> HEADINGS{{
    {SandboxScreen::Title::Cleared, "REGION CLEARED"},
    {SandboxScreen::Title::Terminated, "PROCESS TERMINATED"},
    {SandboxScreen::Title::Interrupted, "PROCESS INTERRUPTED"},
}};

struct ModeName
{
    PlayMode Mode;
    TKit::StringView Text;
};

constexpr TKit::FixedArray<ModeName, 2> MODE_NAMES{{
    {PlayMode::TestShapes, "TEST SHAPES"},
    {PlayMode::TestSphere, "TEST SPHERE"},
}};

[[nodiscard]] constexpr bool HasHint(const TKit::StringView section, const TKit::StringView field)
{
    for (const FieldHint &entry : HINTS)
        if (entry.Section == section && entry.Field == field)
            return true;
    return false;
}

template <typename Params> constexpr void CountHintedFields(const TKit::StringView section, u32 &fields, u32 &hinted)
{
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            CountHintedFields<Type>(field.Name, fields, hinted);
        else
        {
            ++fields;
            hinted += HasHint(section, field.Name) ? 1U : 0U;
        }
    });
}

[[nodiscard]] constexpr bool IsEveryFieldHinted()
{
    u32 fields = 0;
    u32 hinted = 0;
    CountHintedFields<LevelParams>(TKit::StringView{}, fields, hinted);
    return hinted == fields && fields == HINTS.GetSize();
}

template <typename Entry, usize Size>
[[nodiscard]] constexpr bool AreFieldsKnown(const TKit::FixedArray<Entry, Size> &table)
{
    for (const Entry &entry : table)
        if (!HasHint(entry.Section, entry.Field))
            return false;
    return true;
}

[[nodiscard]] constexpr bool AreRulesKnown()
{
    const auto areFlagsKnown = [](const TKit::Span<const FlagCondition> conditions) {
        for (const FlagCondition &condition : conditions)
            if (!HasHint(FLAGS_SECTION, condition.Flag))
                return false;
        return true;
    };
    const auto isSectionKnown = [](const TKit::StringView section) {
        for (const FieldHint &entry : HINTS)
            if (entry.Section == section)
                return true;
        return false;
    };

    for (const SectionRule &rule : SECTION_RULES)
        if (!isSectionKnown(rule.Section) || !areFlagsKnown(rule.Conditions))
            return false;
    for (const FieldRule &rule : FIELD_RULES)
        if (!areFlagsKnown(rule.Conditions))
            return false;
    return true;
}

static_assert(IsEveryFieldHinted(), "Every tuning field needs exactly one HINTS entry");
static_assert(AreFieldsKnown(FIELD_RULES), "Every FIELD_RULES entry must name a tuning field");
static_assert(AreFieldsKnown(DRAG_OVERRIDES), "Every DRAG_OVERRIDES entry must name a tuning field");
static_assert(AreRulesKnown(), "Every rule must name a tuning section and existing feature flags");

struct SectionWalk
{
    Onyx::Overlay &Ui;
    Onyx::Color DimText;
    ScopeFlags Scope;
    const FeatureFlags &Flags;
    u32 Count;
};

[[nodiscard]] Onyx::OverlayColors CreateColors(const Palette &palette)
{
    const Onyx::Color hovered = Mix(palette.UiSurface, palette.UiTextDim, HOVER_TINT);
    const Onyx::Color pressed = Mix(palette.UiSurface, palette.UiTextDim, PRESS_TINT);

    Onyx::OverlayPalette overlay{};
    overlay[Onyx::OverlayPalette_Idle0] = palette.UiSurface;
    overlay[Onyx::OverlayPalette_Idle1] = palette.UiBorder;
    overlay[Onyx::OverlayPalette_Idle2] = palette.UiBackground;

    overlay[Onyx::OverlayPalette_Hovered0] = hovered;
    overlay[Onyx::OverlayPalette_Hovered1] = Mix(palette.UiBorder, palette.UiTextDim, HOVER_TINT);
    overlay[Onyx::OverlayPalette_Hovered2] = hovered;
    overlay[Onyx::OverlayPalette_Hovered3] = hovered;

    overlay[Onyx::OverlayPalette_Pressed0] = pressed;
    overlay[Onyx::OverlayPalette_Pressed1] = pressed;
    overlay[Onyx::OverlayPalette_Pressed2] = pressed;

    overlay[Onyx::OverlayPalette_Text0] = palette.UiText;

    overlay[Onyx::OverlayPalette_Inner0] = palette.UiTextDim;
    overlay[Onyx::OverlayPalette_Inner1] = palette.UiText;

    overlay[Onyx::OverlayPalette_Background0] = palette.UiBackground;
    overlay[Onyx::OverlayPalette_Background1] = palette.UiBorder;
    overlay[Onyx::OverlayPalette_Background2] = palette.UiBackground;

    Onyx::OverlayColors colors = Onyx::CreateOverlayColorsFromPalette(overlay);
    colors[Onyx::OverlayColor_WindowBorderIdle] = palette.UiBorder;
    colors[Onyx::OverlayColor_WindowBorderHovered] = palette.UiBorder;
    colors[Onyx::OverlayColor_WindowBorderPressed] = palette.UiBorder;
    colors[Onyx::OverlayColor_ButtonIdle] = palette.UiBorder;
    colors[Onyx::OverlayColor_ButtonHovered] = Mix(palette.UiBorder, palette.UiTextDim, HOVER_TINT);
    colors[Onyx::OverlayColor_ButtonPressed] = Mix(palette.UiBorder, palette.UiTextDim, PRESS_TINT);
    return colors;
}

[[nodiscard]] Onyx::OverlayStyleVariables CreateVariables()
{
    Onyx::OverlayStyleVariables variables = Onyx::CreateDefaultOverlayVariables();
    variables[Onyx::OverlayStyle_WindowBorderWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_LineWidth] = BORDER_WIDTH;
    variables[Onyx::OverlayStyle_WindowPadding] = WINDOW_PADDING;
    variables[Onyx::OverlayStyle_WidgetMinimumWidth] = WIDGET_WIDTH;
    return variables;
}

/// Writes the text colour directly, because PushStyleColor backs up the colour already scaled by the style alpha.
template <typename Widget> void Dimmed(Onyx::Overlay &ui, const Onyx::Color &dim, const Widget &widget)
{
    Onyx::OverlayColors &colors = ui.GetStyle().Colors;
    const Onyx::Color regular = colors[Onyx::OverlayColor_Text];
    colors[Onyx::OverlayColor_Text] = dim;
    widget();
    colors[Onyx::OverlayColor_Text] = regular;
}

void DrawHeader(Onyx::Overlay &ui, const Onyx::Color &dim, const PlayMode mode, const SandboxScreen::Title title,
                const u64 address)
{
    ui.PushStyleVar(Onyx::OverlayStyle_FontSize, TITLE_SCALE * ui.GetStyle()[Onyx::OverlayStyle_FontSize]);
    for (const Heading &heading : HEADINGS)
        if (heading.Title == title)
            ui.TextRaw(heading.Text);
    ui.PopStyleVar();

    for (const ModeName &name : MODE_NAMES)
        if (name.Mode == mode)
            ui.TextRaw(name.Text);

    const TKit::StaticString32 source = TKit::StaticString32::Format("SOURCE 0x{:016X}", address);
    Dimmed(ui, dim, [&] { ui.TextRaw(source); });
}

/// Upper-cases a PascalCase name and splits it into words: "WaveAmplitude" becomes "WAVE AMPLITUDE".
[[nodiscard]] Label ToLabel(const TKit::StringView name, const TKit::StringView prefix = {})
{
    const auto isUpper = [](const char letter) { return letter >= 'A' && letter <= 'Z'; };
    const auto isLower = [](const char letter) { return letter >= 'a' && letter <= 'z'; };

    Label label{};
    for (const char letter : prefix)
        label.Append(letter);
    for (usize i = 0; i < name.GetSize(); ++i)
    {
        const char letter = name[i];
        const bool afterLower = i > 0 && isLower(name[i - 1]);
        const bool endsAcronym = i > 0 && isUpper(name[i - 1]) && i + 1 < name.GetSize() && isLower(name[i + 1]);
        if (isUpper(letter) && (afterLower || endsAcronym))
            label.Append(' ');
        label.Append(isLower(letter) ? char(letter - 'a' + 'A') : letter);
    }
    return label;
}

/// The field's own rule when one matches, else its section's, else visible everywhere.
[[nodiscard]] Visibility FindVisibility(const TKit::StringView section, const TKit::StringView field)
{
    for (const FieldRule &rule : FIELD_RULES)
        if (rule.Section == section && rule.Field == field)
            return Visibility{.Scope = rule.Scope, .Conditions = rule.Conditions};
    for (const SectionRule &rule : SECTION_RULES)
        if (rule.Section == section)
            return Visibility{.Scope = rule.Scope, .Conditions = rule.Conditions};
    return Visibility{.Scope = EVERY_MODE, .Conditions = UNCONDITIONAL};
}

[[nodiscard]] bool IsVisible(const SectionWalk &walk, const TKit::StringView section, const TKit::StringView field)
{
    const Visibility visibility = FindVisibility(section, field);
    if ((visibility.Scope & walk.Scope) == 0)
        return false;
    for (const FlagCondition &condition : visibility.Conditions)
    {
        const auto flag = TKit::Reflect<FeatureFlags>::GetMemberField<bool>(condition.Flag);
        if (!flag || flag.Get(walk.Flags) != condition.Enabled)
            return false;
    }
    return true;
}

template <typename Params> [[nodiscard]] bool HasVisibleField(const SectionWalk &walk, const TKit::StringView section)
{
    bool visible = false;
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
            visible = visible || HasVisibleField<Type>(walk, field.Name);
        else
            visible = visible || IsVisible(walk, section, field.Name);
    });
    return visible;
}

[[nodiscard]] TKit::StringView FindHint(const TKit::StringView section, const TKit::StringView field)
{
    for (const FieldHint &entry : HINTS)
        if (entry.Section == section && entry.Field == field)
            return entry.Hint;
    return TKit::StringView{};
}

/// An override when one matches, else a range scaled from the struct default so new fields need no table entry.
template <TKit::Numeric T>
[[nodiscard]] DragRange FindRange(const TKit::StringView section, const TKit::StringView field, const T defaultValue)
{
    for (const DragOverride &entry : DRAG_OVERRIDES)
        if (entry.Section == section && entry.Field == field)
            return entry.Range;

    const f32 magnitude = TKit::Math::Absolute(f32(defaultValue));
    const f32 reference = magnitude > 0.f ? magnitude : 1.f;
    const f32 limit = RANGE_SCALE * reference;
    const f32 low = f32(defaultValue) < 0.f ? -limit : 0.f;
    if constexpr (TKit::Integer<T>)
        return DragRange{low, limit, INTEGER_DRAG_SPEED, INTEGER};
    else
    {
        usize precision = 0;
        for (f32 bound = DECADE; reference < bound && precision + 1 < REAL_FORMATS.GetSize(); bound /= DECADE)
            ++precision;
        return DragRange{low, limit, REFERENCE_SHARE_PER_PIXEL * reference, REAL_FORMATS[precision]};
    }
}

void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView, const TKit::StringView field, bool &enabled, const bool)
{
    ui.CheckBox(ToLabel(field), &enabled);
}

void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView, const TKit::StringView field, Onyx::Color &color,
              const Onyx::Color &)
{
    ui.ColorEditor(ToLabel(field), &color, COLOR_FLAGS);
}

template <TKit::Numeric T>
void DrawLeaf(Onyx::Overlay &ui, const TKit::StringView section, const TKit::StringView field, T &value,
              const T defaultValue)
{
    const DragRange range = FindRange(section, field, defaultValue);
    ui.HorizontalDrag(ToLabel(field), &value, range.Speed, T(range.Min), T(range.Max), range.Format);
}

/// Draws the visible plain fields first, then each nested struct holding a visible field as a numbered section.
template <typename Params>
void DrawFields(SectionWalk &walk, Params &params, const Params &defaults, const TKit::StringView section)
{
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (!TKit::Reflect<Type>::Implemented)
        {
            if (!IsVisible(walk, section, field.Name))
                return;
            DrawLeaf(walk.Ui, section, field.Name, field.Get(params), field.Get(defaults));
            walk.Ui.SetItemTooltipRaw(FindHint(section, field.Name), HINT_FLAGS);
        }
    });
    TKit::Reflect<Params>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        if constexpr (TKit::Reflect<Type>::Implemented)
        {
            if (!HasVisibleField<Type>(walk, field.Name))
                return;
            const Label title = ToLabel(field.Name, Label::Format("[{:02}] ", ++walk.Count));
            Dimmed(walk.Ui, walk.DimText, [&] { walk.Ui.HorizontalSeparator(title); });
            walk.Ui.PushId(field.Name);
            DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
            walk.Ui.PopId();
        }
    });
}

void DrawTabs(SectionWalk &walk, LevelParams &params, const LevelParams &defaults, const f32 scrollHeight)
{
    Onyx::Overlay &ui = walk.Ui;
    ui.BeginTabBar(TAB_BAR_ID);
    TKit::Reflect<LevelParams>::ForEachMemberField([&](const auto &field) {
        using Type = TKIT_REFLECT_FIELD_TYPE(field);
        static_assert(TKit::Reflect<Type>::Implemented, "Every LevelParams member is a tab, so it must be a struct");
        if (!HasVisibleField<Type>(walk, field.Name))
            return;

        TKit::StringView name = field.Name;
        for (const TabName &tab : TAB_NAMES)
            if (tab.Field == name)
                name = tab.Title;
        if (!ui.BeginTab(ToLabel(name), TAB_FLAGS))
            return;

        ui.BeginScroll(SCROLL_LABEL, scrollHeight);
        walk.Count = 0;
        DrawFields(walk, field.Get(params), field.Get(defaults), field.Name);
        ui.EndScroll();
        ui.EndTab();
    });
    ui.EndTabBar();
}

[[nodiscard]] SandboxScreen::Action DrawButtons(Onyx::Overlay &ui)
{
    ui.BeginPanel(Onyx::Overlay::LyPnPar{.Direction = Onyx::LayoutDirection_LeftToRight,
                                         .Sizing = {Onyx::LayoutSizing::Grow(), Onyx::LayoutSizing::Fit()},
                                         .ChildGap = ui.GetStyle()[Onyx::OverlayStyle_ChildGap]});
    SandboxScreen::Action action = SandboxScreen::Action::None;
    if (ui.Button("SAME BYTES", Onyx::OverlayButtonFlag_SpanFullWidth))
        action = SandboxScreen::Action::SameBytes;
    if (ui.Button("NEW REGION", Onyx::OverlayButtonFlag_SpanFullWidth))
        action = SandboxScreen::Action::NewRegion;
    ui.EndPanel();
    return action;
}
} // namespace

SandboxScreen::SandboxScreen(Onyx::Window &window, FontLibrary &fonts)
    : m_Font(fonts.Get()),
      m_Overlay(window.CreateOverlay(Onyx::OverlaySpecs{
          .Layout = {.RootAlignment = {Onyx::Alignment_Left, Onyx::Alignment_Top}, .Font = m_Font}})),
      m_Variables(CreateVariables())
{
}

SandboxScreen::Action SandboxScreen::Draw(LevelParams &params, const PlayMode mode, const Title title,
                                          const u64 address, const f32 opacity)
{
    const Action action = opacity > 0.f ? DrawPanel(params, mode, title, address, opacity) : Action::None;
    m_Overlay->Draw();
    return action;
}

SandboxScreen::Action SandboxScreen::DrawPanel(LevelParams &params, const PlayMode mode, const Title title,
                                               const u64 address, const f32 opacity)
{
    Onyx::Overlay &ui = *m_Overlay;
    const Onyx::NativeWindow &native = *ui.GetMainNativeWindow();
    const f32v2 screen{native.Window->GetScreenDimensions()};
    if (screen[1] <= 0.f)
        return Action::None;

    ApplyTheme(params.LookFeel, screen[1], m_Size ? opacity : 0.f);
    const f32v2 size = m_Size ? *m_Size : f32v2{0.f};
    ui.SetNextWindowPosition(
        f32v2{TKit::Math::Round(0.5f * (screen[0] - size[0])), TKit::Math::Round(0.5f * (screen[1] - size[1]))});

    const Onyx::OverlayLabel label{WINDOW_LABEL};
    if (!ui.BeginWindow(label, WINDOW_FLAGS))
        return Action::None;
    if (const Onyx::LayoutElementQueryInfo *element = ui.GetActiveLayout()->QueryElement(label.Id))
        m_Size = element->Size;

    const Onyx::Color dim = params.LookFeel.Colors.UiTextDim;
    DrawHeader(ui, dim, mode, title, address);
    ui.HorizontalLine();
    SectionWalk walk{.Ui = ui, .DimText = dim, .Scope = ScopeFlags(1U << u8(mode)), .Flags = params.Flags, .Count = 0};
    DrawTabs(walk, params, m_Defaults, SCROLL_HEIGHT_SHARE * screen[1]);
    const Action action = DrawButtons(ui);
    ui.EndWindow();
    return action;
}

void SandboxScreen::ApplyTheme(const LookFeelParams &lookFeel, const f32 screenHeight, const f32 alpha)
{
    const f32 fontPixels = lookFeel.TextSize * screenHeight / REFERENCE_SCREEN_HEIGHT;
    const f32 scale = fontPixels / m_Variables[Onyx::OverlayStyle_FontSize];

    Onyx::OverlayStyle &style = m_Overlay->GetStyle();
    style.Colors = CreateColors(lookFeel.Colors);
    style.Variables = m_Variables;
    style.Variables[Onyx::OverlayStyle_FontSize] = fontPixels;
    for (const Onyx::OverlayStyleVariable variable : SCALED_VARIABLES)
        style.Variables[variable] *= scale;
    style.Variables[Onyx::OverlayStyle_Alpha] = TKit::Math::Clamp(alpha, 0.f, 1.f);

    m_Overlay->GetMainNativeWindow()->Context->Font(m_Font);
}
} // namespace Lim
