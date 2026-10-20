// Theme presets and the color each role resolves to.
#include "vis.h"

namespace vis
{

Theme Theme::preset(ThemePreset preset) noexcept
{
    Theme theme;
    // Pro White is the default Theme, and adaptive uses it until a host supplies its own.
    if (preset != ThemePreset::ProBlack) return theme;

    theme.background = Color::hex("#000000");
    theme.h1 = {"Pretendard", 36.0f, Color::hex("#ffffff")};
    theme.h2 = {"Pretendard", 27.0f, Color::hex("#ffffff")};
    theme.h3 = {"Pretendard", 21.0f, Color::hex("#e8eaed")};
    theme.text = {"Pretendard", 16.0f, Color::hex("#d8dadd")};
    theme.code = {"JetBrainsMono", 14.0f, Color::hex("#f6c49f")};
    const char* objects[] = {
        "#f28e2b", "#ff6b6b", "#76b7b2", "#7bc96f",
        "#c29bc0", "#ff9da7", "#c49a83", "#bab0ac",
    };
    for (auto i = 0u; i < 8u; i++)
        theme.objects[i] = Color::hex(objects[i]);
    theme.objectCount = 8;
    theme.objectWidth = 1.25f;
    theme.endGradientStop = Color::hex("#999999");
    auto grid = Color::hex("#d8dadd");
    theme.axis = {
        grid,
        grid,
        grid,
        grid,
        Color::hex("#e8eaed"),
    };
    theme.colors = {
        Color::hex("#ffffff"),
        Color::hex("#b8bcc2"),
        Color::hex("#6cb6ff"),
        Color::hex("#c8a7ff"),
        Color::hex("#7ee787"),
        Color::hex("#e3b341"),
        Color::hex("#ff7b72"),
        Color::hex("#79c0ff"),
        Color::hex("#161b22"),
        Color::hex("#30363d"),
        Color::hex("#edc948"),
        Color::hex("#6ea8fe"),
    };
    return theme;
}

Color Theme::color(ThemeColorRole role) const noexcept
{
    switch (role) {
        case ThemeColorRole::Background: return background;
        case ThemeColorRole::Foreground: return colors.foreground;
        case ThemeColorRole::Muted: return colors.muted;
        case ThemeColorRole::Accent: return colors.accent;
        case ThemeColorRole::Secondary: return colors.secondary;
        case ThemeColorRole::Success: return colors.success;
        case ThemeColorRole::Warning: return colors.warning;
        case ThemeColorRole::Danger: return colors.danger;
        case ThemeColorRole::Info: return colors.info;
        case ThemeColorRole::Surface: return colors.surface;
        case ThemeColorRole::Border: return colors.border;
        case ThemeColorRole::Result: return colors.result;
        case ThemeColorRole::Focus: return colors.focus;
    }
    return colors.foreground;
}

}  // namespace vis
