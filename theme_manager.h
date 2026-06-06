#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QString>
#include <QColor>
#include <QFont>

// ============================================================================
// ThemeManager — Unified Design System for DiPDF
//
// ALL visual constants and QSS live here. Nothing in mainwindow.cpp should
// contain raw color codes or inline stylesheets.
// ============================================================================
class ThemeManager {
public:
    // ── Color Palette (exact spec) ─────────────────────────────────────────
    static constexpr const char* Background      = "#F5F7FA";
    static constexpr const char* Surface         = "#FFFFFF";
    static constexpr const char* SurfaceMuted    = "#F1F5F9";
    static constexpr const char* SurfaceHover    = "#F9FAFB";

    static constexpr const char* Border          = "#E2E8F0";
    static constexpr const char* BorderStrong    = "#CBD5E1";
    static constexpr const char* BorderLight     = "#F1F5F9";

    static constexpr const char* TextPrimary     = "#111827";
    static constexpr const char* TextSecondary   = "#6B7280";
    static constexpr const char* TextMuted       = "#9CA3AF";
    static constexpr const char* TextLabel       = "#374151";

    static constexpr const char* Primary         = "#2563EB";
    static constexpr const char* PrimaryHover    = "#1D4ED8";
    static constexpr const char* PrimaryPressed  = "#1E40AF";
    static constexpr const char* PrimarySoft     = "#EFF6FF";
    static constexpr const char* PrimaryBorder   = "#BFDBFE";
    static constexpr const char* PrimaryText     = "#1E40AF";

    static constexpr const char* Danger          = "#EF4444";
    static constexpr const char* DangerSoft      = "#FEE2E2";
    static constexpr const char* DangerBorder    = "#FECACA";

    static constexpr const char* Warning         = "#F59E0B";
    static constexpr const char* WarningSoft     = "#FEF3C7";
    static constexpr const char* WarningBorder   = "#FDE68A";

    static constexpr const char* CanvasBg        = "#EAECF0";  // PDF page scroll area

    // ── Spacing & Sizing ───────────────────────────────────────────────────
    static constexpr int RadiusSmall   = 6;
    static constexpr int RadiusMedium  = 8;    // standard buttons, inputs, cards
    static constexpr int RadiusLarge   = 12;   // panels, big cards

    static constexpr int Sp4  = 4;
    static constexpr int Sp8  = 8;
    static constexpr int Sp12 = 12;
    static constexpr int Sp16 = 16;
    static constexpr int Sp20 = 20;
    static constexpr int Sp24 = 24;

    static constexpr int ButtonHeight  = 36;
    static constexpr int InputHeight   = 38;
    static constexpr int ToolbarHeight = 48;
    static constexpr int PanelHeaderH  = 40;   // consistent for left & right panels

    static constexpr int LeftPanelMinW   = 240;
    static constexpr int LeftPanelMaxW   = 300;
    static constexpr int RightPanelMinW  = 320;
    static constexpr int RightPanelMaxW  = 400;
    static constexpr int NavSidebarW     = 220;

    // ── Font helpers ───────────────────────────────────────────────────────
    static QFont bodyFont(int size = 13);
    static QFont headingFont(int size = 15);
    static QFont monoFont(int size = 12);

    // ── Global Stylesheet ──────────────────────────────────────────────────
    static QString globalStyleSheet();

    // ── Component-level QSS ────────────────────────────────────────────────
    // Document tab bar (Chrome/VSCode style)
    static QString pdfTabBarStyle();

    // Side panel inner tabs (Thumbnail/Bookmark, Search/Notes/AI)
    static QString panelTabStyle();
    
    // Custom button-based tabs for panels to guarantee 100% full-width expansion
    static QString customTabButtonStyle();
    static QString customTabBarContainerStyle();

    // Toolbar container
    static QString toolbarStyle();

    // Toolbar control group (nav group, zoom group)
    static QString toolbarGroupStyle();
    static QString toolbarGroupButtonStyle();

    // Thumbnail list widget
    static QString thumbnailListStyle();

    // Bookmark tree view
    static QString bookmarkTreeStyle();

    // Buttons
    static QString primaryButtonStyle();
    static QString secondaryButtonStyle();
    static QString dangerButtonStyle();
    static QString ghostButtonStyle();
    static QString quickActionButtonStyle();

    // Inputs
    static QString searchInputStyle();

    // PDF Canvas
    static QString scrollAreaCanvasStyle();
    static QString pageCanvasStyle();

    // Panel header (shared by left and right panels)
    static QString panelHeaderStyle();
    static QString panelTitleStyle();
    static QString collapseButtonStyle();

    // Empty state labels
    static QString emptyStateStyle();

    // File list items (recent/favorite)
    static QString fileListStyle();
    static QString fileRowTitleStyle();
    static QString fileRowPathStyle();
    static QString fileRowOpenBtnStyle();
    static QString fileRowFavBtnStyle();
    static QString fileRowDeleteBtnStyle();

    // Settings view inputs
    static QString settingsInputStyle();
    static QString settingsLabelStyle();
    static QString settingsTitleStyle();
    static QString settingsSubtitleStyle();

    // Result list (search results, notes list)
    static QString resultListStyle();

    // Home view
    static QString homeGreetingStyle();
    static QString homeSubGreetingStyle();
    static QString homeSectionTitleStyle();
};

#endif // THEME_MANAGER_H
