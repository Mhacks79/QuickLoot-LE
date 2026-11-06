#pragma once

#include <cstdint>

#define KHAOS_UI_THEME_API_VERSION 1u
#define KHAOS_UI_THEME_MAX_COLORS 64u

// Stable KHA-OS color IDs. These deliberately do NOT expose Dear ImGui's enum values,
// because ImGui may add/reorder color slots between versions used by different DLLs.
enum KHAOS_UI_ThemeColor : std::uint32_t
{
    KHAOS_UI_COLOR_TEXT = 0,
    KHAOS_UI_COLOR_TEXT_DISABLED,
    KHAOS_UI_COLOR_WINDOW_BG,
    KHAOS_UI_COLOR_CHILD_BG,
    KHAOS_UI_COLOR_POPUP_BG,
    KHAOS_UI_COLOR_BORDER,
    KHAOS_UI_COLOR_BORDER_SHADOW,
    KHAOS_UI_COLOR_FRAME_BG,
    KHAOS_UI_COLOR_FRAME_BG_HOVERED,
    KHAOS_UI_COLOR_FRAME_BG_ACTIVE,
    KHAOS_UI_COLOR_TITLE_BG,
    KHAOS_UI_COLOR_TITLE_BG_ACTIVE,
    KHAOS_UI_COLOR_TITLE_BG_COLLAPSED,
    KHAOS_UI_COLOR_MENU_BAR_BG,
    KHAOS_UI_COLOR_SCROLLBAR_BG,
    KHAOS_UI_COLOR_SCROLLBAR_GRAB,
    KHAOS_UI_COLOR_SCROLLBAR_GRAB_HOVERED,
    KHAOS_UI_COLOR_SCROLLBAR_GRAB_ACTIVE,
    KHAOS_UI_COLOR_CHECK_MARK,
    KHAOS_UI_COLOR_SLIDER_GRAB,
    KHAOS_UI_COLOR_SLIDER_GRAB_ACTIVE,
    KHAOS_UI_COLOR_BUTTON,
    KHAOS_UI_COLOR_BUTTON_HOVERED,
    KHAOS_UI_COLOR_BUTTON_ACTIVE,
    KHAOS_UI_COLOR_HEADER,
    KHAOS_UI_COLOR_HEADER_HOVERED,
    KHAOS_UI_COLOR_HEADER_ACTIVE,
    KHAOS_UI_COLOR_SEPARATOR,
    KHAOS_UI_COLOR_SEPARATOR_HOVERED,
    KHAOS_UI_COLOR_SEPARATOR_ACTIVE,
    KHAOS_UI_COLOR_RESIZE_GRIP,
    KHAOS_UI_COLOR_RESIZE_GRIP_HOVERED,
    KHAOS_UI_COLOR_RESIZE_GRIP_ACTIVE,
    KHAOS_UI_COLOR_TAB,
    KHAOS_UI_COLOR_TAB_HOVERED,
    KHAOS_UI_COLOR_TAB_ACTIVE,
    KHAOS_UI_COLOR_TAB_UNFOCUSED,
    KHAOS_UI_COLOR_TAB_UNFOCUSED_ACTIVE,
    KHAOS_UI_COLOR_PLOT_LINES,
    KHAOS_UI_COLOR_PLOT_LINES_HOVERED,
    KHAOS_UI_COLOR_PLOT_HISTOGRAM,
    KHAOS_UI_COLOR_PLOT_HISTOGRAM_HOVERED,
    KHAOS_UI_COLOR_TEXT_SELECTED_BG,
    KHAOS_UI_COLOR_DRAG_DROP_TARGET,
    KHAOS_UI_COLOR_NAV_HIGHLIGHT,
    KHAOS_UI_COLOR_NAV_WINDOWING_HIGHLIGHT,
    KHAOS_UI_COLOR_NAV_WINDOWING_DIM_BG,
    KHAOS_UI_COLOR_MODAL_WINDOW_DIM_BG,
    KHAOS_UI_COLOR_TABLE_HEADER_BG,
    KHAOS_UI_COLOR_TABLE_BORDER_STRONG,
    KHAOS_UI_COLOR_TABLE_BORDER_LIGHT,
    KHAOS_UI_COLOR_TABLE_ROW_BG,
    KHAOS_UI_COLOR_TABLE_ROW_BG_ALT,
    KHAOS_UI_COLOR_COUNT
};

static_assert(KHAOS_UI_COLOR_COUNT <= KHAOS_UI_THEME_MAX_COLORS,
    "KHA-OS theme color ABI exceeds snapshot storage");

#pragma pack(push, 8)
struct KHAOS_UI_ThemeSnapshot
{
    std::uint32_t apiVersion;
    std::uint32_t structSize;
    std::uint32_t generation;
    std::uint32_t colorCount;

    float uiScale;
    float userScale;
    std::uint32_t autoScale;
    std::uint32_t reserved0;

    float alpha;
    float disabledAlpha;
    float windowPadding[2];
    float windowRounding;
    float windowBorderSize;
    float windowMinSize[2];
    float windowTitleAlign[2];
    float childRounding;
    float childBorderSize;
    float popupRounding;
    float popupBorderSize;
    float framePadding[2];
    float frameRounding;
    float frameBorderSize;
    float itemSpacing[2];
    float itemInnerSpacing[2];
    float indentSpacing;
    float scrollbarSize;
    float scrollbarRounding;
    float grabMinSize;
    float grabRounding;
    float tabRounding;
    float tabBorderSize;
    float tabBarBorderSize;

    // RGBA float colors indexed by KHAOS_UI_ThemeColor, not ImGuiCol.
    float colors[KHAOS_UI_THEME_MAX_COLORS][4];
    char themeName[64];
    char themePath[260];
};
#pragma pack(pop)

typedef bool (__cdecl *KHAOS_UI_GetThemeSnapshotFn)(KHAOS_UI_ThemeSnapshot* outTheme);
typedef float (__cdecl *KHAOS_UI_GetScaleFn)();
typedef std::uint32_t (__cdecl *KHAOS_UI_GetThemeGenerationFn)();
