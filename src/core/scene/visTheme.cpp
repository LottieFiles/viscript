#include "vis.h"

namespace vis
{

Theme Theme::preset(ThemePreset preset) noexcept
{
    Theme theme;
    if (preset == ThemePreset::ProWhite || preset == ThemePreset::AdaptiveVscode) return theme;
    if (preset == ThemePreset::ThreeBlueOneEyes) {
        theme.background = Color::hex("#0d1117");
        theme.h1 = {"Pretendard", 38.0f, Color::hex("#f4f7fb")};
        theme.h2 = {"Pretendard", 30.0f, Color::hex("#f4f7fb")};
        theme.h3 = {"Pretendard", 23.0f, Color::hex("#dbe7f3")};
        theme.text = {"Pretendard", 17.0f, Color::hex("#dbe7f3")};
        theme.code = {"JetBrainsMono", 15.0f, Color::hex("#ffd166")};
        const char* objects[] = {
            "#f28e2b", "#ff6b6b", "#76b7b2", "#7bc96f",
            "#c29bc0", "#ff9da7", "#c49a83", "#bab0ac",
        };
        for (auto i = 0u; i < 8u; i++)
            theme.objects[i] = Color::hex(objects[i]);
        theme.objectCount = 8;
        theme.objectWidth = 2.0f;
        theme.endGradientStop = Color::hex("#f72585");
        theme.axis = {
            Color::hex("#ef5350"),
            Color::hex("#66bb6a"),
            Color::hex("#42a5f5"),
            Color::hex("#374151b4"),
            Color::hex("#aeb7c3"),
        };
        theme.colors = {
            Color::hex("#f4f7fb"),
            Color::hex("#aeb7c3"),
            Color::hex("#4cc9f0"),
            Color::hex("#9b8cff"),
            Color::hex("#7bd88f"),
            Color::hex("#ffd166"),
            Color::hex("#ff6b6b"),
            Color::hex("#42a5f5"),
            Color::hex("#161b22"),
            Color::hex("#374151"),
            Color::hex("#edc948"),
            Color::hex("#6ea8fe"),
        };
        return theme;
    }
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
