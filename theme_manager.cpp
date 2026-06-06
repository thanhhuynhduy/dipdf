#include "theme_manager.h"

// ============================================================================
// ThemeManager Implementation — Complete Design System for DiPDF
// ============================================================================

// ── Font helpers ────────────────────────────────────────────────────────────

QFont ThemeManager::bodyFont(int size) {
    QFont f("Inter", size);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setFamilies({"Inter", "Segoe UI", "Noto Sans", "sans-serif"});
    return f;
}

QFont ThemeManager::headingFont(int size) {
    QFont f("Quicksand", size, QFont::Bold);
    f.setStyleStrategy(QFont::PreferAntialias);
    f.setFamilies({"Quicksand", "Inter", "Segoe UI", "sans-serif"});
    return f;
}

QFont ThemeManager::monoFont(int size) {
    QFont f("JetBrains Mono", size);
    f.setFamilies({"JetBrains Mono", "Cascadia Code", "Consolas", "monospace"});
    return f;
}

// ── Global Stylesheet ───────────────────────────────────────────────────────

QString ThemeManager::globalStyleSheet() {
    return QStringLiteral(R"(

/* ═══════════════════════════════════════════════════════════════════════════
   BASE RESET
   ═══════════════════════════════════════════════════════════════════════════ */

QWidget {
    font-family: 'Inter', 'Segoe UI', 'Noto Sans', sans-serif;
    font-size: 13px;
    color: #111827;
    outline: none;
}

QMainWindow {
    background-color: #F5F7FA;
}

/* ═══════════════════════════════════════════════════════════════════════════
   BUTTONS — default primary
   ═══════════════════════════════════════════════════════════════════════════ */

QPushButton {
    background-color: #2563EB;
    color: white;
    border: none;
    padding: 8px 16px;
    border-radius: 8px;
    font-weight: 600;
    font-size: 13px;
    min-height: 20px;
}
QPushButton:hover  { background-color: #1D4ED8; }
QPushButton:pressed { background-color: #1E40AF; }
QPushButton:disabled { background-color: #E2E8F0; color: #9CA3AF; }

/* ═══════════════════════════════════════════════════════════════════════════
   INPUTS
   ═══════════════════════════════════════════════════════════════════════════ */

QLineEdit {
    border: 1px solid #E2E8F0;
    border-radius: 8px;
    padding: 8px 12px;
    background: #FFFFFF;
    color: #111827;
    font-size: 13px;
    min-height: 20px;
    selection-background-color: #BFDBFE;
}
QLineEdit:focus       { border-color: #2563EB; }
QLineEdit:hover:!focus { border-color: #CBD5E1; }

QTextEdit {
    border: 1px solid #E2E8F0;
    border-radius: 8px;
    padding: 8px 12px;
    background: #FFFFFF;
    color: #111827;
    font-size: 13px;
    selection-background-color: #BFDBFE;
}
QTextEdit:focus { border-color: #2563EB; }

/* ═══════════════════════════════════════════════════════════════════════════
   SCROLLBARS — thin minimal
   ═══════════════════════════════════════════════════════════════════════════ */

QScrollBar:vertical {
    border: none; background: transparent;
    width: 7px; margin: 2px 1px;
}
QScrollBar::handle:vertical {
    background: #D1D5DB; border-radius: 3px; min-height: 30px;
}
QScrollBar::handle:vertical:hover   { background: #9CA3AF; }
QScrollBar::handle:vertical:pressed { background: #6B7280; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

QScrollBar:horizontal {
    border: none; background: transparent;
    height: 7px; margin: 1px 2px;
}
QScrollBar::handle:horizontal {
    background: #D1D5DB; border-radius: 3px; min-width: 30px;
}
QScrollBar::handle:horizontal:hover   { background: #9CA3AF; }
QScrollBar::handle:horizontal:pressed { background: #6B7280; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* ═══════════════════════════════════════════════════════════════════════════
   MISC GLOBAL
   ═══════════════════════════════════════════════════════════════════════════ */

QLabel { background: transparent; border: none; }

QToolTip {
    background-color: #1F2937; color: #F9FAFB;
    border: 1px solid #374151; border-radius: 6px;
    padding: 6px 10px; font-size: 12px;
}

QDockWidget { border: none; }

QMessageBox { background-color: #FFFFFF; }
QMessageBox QLabel { color: #111827; font-size: 13px; }

/* ═══════════════════════════════════════════════════════════════════════════
   NAV SIDEBAR (#NavSidebar)
   ═══════════════════════════════════════════════════════════════════════════ */

QListWidget#NavSidebar {
    background-color: #FFFFFF;
    border: none;
    border-right: 1px solid #E2E8F0;
    padding-top: 12px;
    outline: none;
}
QListWidget#NavSidebar::item {
    padding: 10px 16px;
    margin: 2px 10px;
    border-radius: 8px;
    color: #6B7280;
    font-weight: 600;
    font-size: 13px;
}
QListWidget#NavSidebar::item:hover {
    background-color: #F1F5F9;
    color: #374151;
}
QListWidget#NavSidebar::item:selected {
    background-color: #EFF6FF;
    color: #2563EB;
    font-weight: 700;
}

/* ═══════════════════════════════════════════════════════════════════════════
   FILE LISTS (#ContentList) — recent / favorite
   ═══════════════════════════════════════════════════════════════════════════ */

QListWidget#ContentList {
    background-color: #FFFFFF;
    border: none;
    outline: none;
}
QListWidget#ContentList::item {
    border-bottom: 1px solid #F1F5F9;
    padding: 2px 0;
}
QListWidget#ContentList::item:hover {
    background-color: #F9FAFB;
}
QListWidget#ContentList::item:selected {
    background-color: transparent;
    color: #111827;
}

    )");
}

// ── Document Tab Bar (Chrome/VSCode style) ──────────────────────────────────

QString ThemeManager::pdfTabBarStyle() {
    return QStringLiteral(R"(
QTabWidget::pane {
    border: none;
    border-top: 1px solid #E2E8F0;
}
QTabBar::tab {
    background: #F1F5F9;
    color: #6B7280;
    padding: 8px 16px 8px 14px;
    border: none;
    border-bottom: 2px solid transparent;
    margin-right: 1px;
    font-weight: 500;
    font-size: 12px;
    min-width: 80px;
    max-width: 200px;
}
QTabBar::tab:selected {
    background: #FFFFFF;
    color: #2563EB;
    border-bottom: 2px solid #2563EB;
    font-weight: 600;
}
QTabBar::tab:hover:!selected {
    background: #E2E8F0;
    color: #374151;
}
QTabBar::close-button {
    image: url(:/assets/close.svg);
    subcontrol-position: right;
    margin-right: 4px;
    padding: 3px;
}
QTabBar::close-button:hover {
    background: #FEE2E2;
    border-radius: 6px;
}
QTabBar::tab:last, QTabBar::tab:only-one {
    min-width: 36px;
    max-width: 36px;
    padding: 0;
    margin-left: 2px;
    font-size: 16px;
    font-weight: 500;
    background: transparent;
    border-bottom: 2px solid transparent;
}
QTabBar::tab:last:hover, QTabBar::tab:only-one:hover {
    background: #E2E8F0;
    color: #111827;
}
QTabBar::tab:last:selected, QTabBar::tab:only-one:selected {
    background: transparent;
    border-bottom: 2px solid transparent;
    color: #6B7280;
}
    )");
}

// ── Panel Tabs (shared: left sidebar + right panel inner tabs) ───────────────

QString ThemeManager::panelTabStyle() {
    return QStringLiteral(R"(
QTabWidget::pane {
    border: none;
    background: #FFFFFF;
}
QTabBar {
    background: #FFFFFF;
}
QTabBar::tab {
    background: transparent;
    padding: 9px 16px;
    color: #6B7280;
    font-weight: 600;
    font-size: 12px;
    border-bottom: 2px solid transparent;
}
QTabBar::tab:selected {
    color: #2563EB;
    border-bottom: 2px solid #2563EB;
}
QTabBar::tab:hover:!selected {
    color: #374151;
    background: #F9FAFB;
}
    )");
}

QString ThemeManager::customTabButtonStyle() {
    return QStringLiteral(
        "QPushButton {"
        "    background: transparent;"
        "    padding: 9px 0px;"
        "    color: #6B7280;"
        "    font-weight: 600;"
        "    font-size: 12px;"
        "    border: none;"
        "    border-bottom: 2px solid transparent;"
        "    border-radius: 0px;"
        "}"
        "QPushButton:hover:!checked {"
        "    color: #374151;"
        "    background: #F9FAFB;"
        "}"
        "QPushButton:checked {"
        "    color: #2563EB;"
        "    border-bottom: 2px solid #2563EB;"
        "}"
    );
}

QString ThemeManager::customTabBarContainerStyle() {
    return QStringLiteral(
        "QWidget { background: #FFFFFF; border-bottom: 1px solid #E2E8F0; }"
    );
}

// ── Toolbar ─────────────────────────────────────────────────────────────────

QString ThemeManager::toolbarStyle() {
    return QStringLiteral(R"(
QToolBar {
    background-color: #FFFFFF;
    border-bottom: 1px solid #E2E8F0;
    padding: 4px 8px;
    spacing: 2px;
    min-height: 44px;
    max-height: 48px;
}
    )");
}

QString ThemeManager::toolbarGroupStyle() {
    return QStringLiteral(
        "QWidget { background: #F1F5F9; border-radius: 8px; }"
    );
}

QString ThemeManager::toolbarGroupButtonStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #374151; border: none; "
        "border-radius: 6px; font-size: 12px; padding: 0px; min-width: 32px; min-height: 32px; }"
        "QPushButton:hover { background: #E2E8F0; }"
        "QPushButton:pressed { background: #CBD5E1; }"
    );
}

// ── Thumbnails ──────────────────────────────────────────────────────────────

QString ThemeManager::thumbnailListStyle() {
    return QStringLiteral(R"(
QListWidget {
    border: none;
    background: #FFFFFF;
    outline: none;
    padding: 6px;
}
QListWidget::item {
    background: #F9FAFB;
    border: 1px solid #E2E8F0;
    border-radius: 6px;
    padding: 4px;
    margin: 4px 4px;
    color: #6B7280;
    font-size: 11px;
}
QListWidget::item:hover {
    border-color: #BFDBFE;
    background: #EFF6FF;
}
QListWidget::item:selected {
    border: 2px solid #2563EB;
    background: #EFF6FF;
    color: #2563EB;
    font-weight: 600;
}
    )");
}

// ── Bookmarks ───────────────────────────────────────────────────────────────

QString ThemeManager::bookmarkTreeStyle() {
    return QStringLiteral(R"(
QTreeView {
    border: none;
    background: #FFFFFF;
    outline: none;
    padding: 4px;
    show-decoration-selected: 0;
}
QTreeView::item {
    padding: 6px 10px;
    border-radius: 6px;
    margin: 1px 4px;
    color: #374151;
    border: none;
    outline: none;
}
QTreeView::item:hover {
    background: #F1F5F9;
}
QTreeView::item:selected {
    background: #EFF6FF;
    color: #2563EB;
    font-weight: 600;
    border: none;
    outline: none;
}
QTreeView::branch {
    background: transparent;
    border: none;
}
QTreeView::branch:selected {
    background: transparent;
    border: none;
}
QTreeView::branch:hover {
    background: transparent;
    border: none;
}
QTreeView::branch:has-children:!has-siblings:closed,
QTreeView::branch:closed:has-children:has-siblings {
    border-image: none;
    image: url(:/assets/chevron-right.svg);
    background: transparent;
}
QTreeView::branch:open:has-children:!has-siblings,
QTreeView::branch:open:has-children:has-siblings  {
    border-image: none;
    image: url(:/assets/chevron-down.svg);
    background: transparent;
}
QTreeView::branch:!has-children {
    border-image: none;
    image: none;
    background: transparent;
    border: none;
}
    )");
}

// ── Buttons ─────────────────────────────────────────────────────────────────

QString ThemeManager::primaryButtonStyle() {
    return QStringLiteral(
        "QPushButton { background-color: #2563EB; color: white; border: none; "
        "padding: 8px 18px; border-radius: 8px; font-weight: 600; font-size: 13px; min-height: 20px; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
        "QPushButton:pressed { background-color: #1E40AF; }"
    );
}

QString ThemeManager::secondaryButtonStyle() {
    return QStringLiteral(
        "QPushButton { background-color: #FFFFFF; color: #374151; "
        "border: 1px solid #E2E8F0; padding: 8px 18px; border-radius: 8px; "
        "font-weight: 600; font-size: 13px; min-height: 20px; }"
        "QPushButton:hover { background-color: #F1F5F9; border-color: #CBD5E1; }"
        "QPushButton:pressed { background-color: #E2E8F0; }"
    );
}

QString ThemeManager::dangerButtonStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: 1px solid #FECACA; padding: 6px 14px; border-radius: 6px; "
        "font-weight: 600; font-size: 12px; min-height: 18px; }"
        "QPushButton:hover { background: #FEE2E2; border-color: #EF4444; }"
        "QPushButton:pressed { background: #FECACA; }"
    );
}

QString ThemeManager::ghostButtonStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #6B7280; border: none; "
        "padding: 6px 10px; border-radius: 6px; font-weight: 500; font-size: 12px; }"
        "QPushButton:hover { background: #F1F5F9; color: #374151; }"
        "QPushButton:pressed { background: #E2E8F0; }"
    );
}

QString ThemeManager::quickActionButtonStyle() {
    return QStringLiteral(
        "QPushButton { background: #EFF6FF; color: #1E40AF; "
        "border: 1px solid #BFDBFE; padding: 8px 10px; border-radius: 8px; "
        "font-weight: 500; font-size: 12px; min-height: 18px; }"
        "QPushButton:hover { background: #DBEAFE; border-color: #93C5FD; }"
        "QPushButton:pressed { background: #BFDBFE; }"
    );
}

// ── Inputs ──────────────────────────────────────────────────────────────────

QString ThemeManager::searchInputStyle() {
    return QStringLiteral(
        "QLineEdit { border: 1px solid #E2E8F0; border-radius: 8px; "
        "padding: 9px 14px; background: #FFFFFF; color: #111827; font-size: 13px; min-height: 20px; }"
        "QLineEdit:focus { border-color: #2563EB; }"
    );
}

// ── PDF Canvas ──────────────────────────────────────────────────────────────

QString ThemeManager::scrollAreaCanvasStyle() {
    return QStringLiteral("QScrollArea { background: #EAECF0; border: none; }");
}

QString ThemeManager::pageCanvasStyle() {
    return QStringLiteral("background: white; border: none; border-radius: 2px;");
}

// ── Panel Header (shared) ───────────────────────────────────────────────────

QString ThemeManager::panelHeaderStyle() {
    return QStringLiteral(
        "QWidget { background: #FFFFFF; border-bottom: 1px solid #E2E8F0; }"
    );
}

QString ThemeManager::panelTitleStyle() {
    return QStringLiteral(
        "font-weight: 600; font-size: 13px; color: #111827; background: transparent;"
    );
}

QString ThemeManager::collapseButtonStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #6B7280; "
        "border: 1px solid #E2E8F0; border-radius: 6px; "
        "font-size: 13px; font-weight: bold; padding: 0px; "
        "min-width: 28px; max-width: 28px; min-height: 28px; max-height: 28px; }"
        "QPushButton:hover { background: #F1F5F9; color: #374151; border-color: #CBD5E1; }"
        "QPushButton:pressed { background: #E2E8F0; }"
    );
}

// ── Empty State ─────────────────────────────────────────────────────────────

QString ThemeManager::emptyStateStyle() {
    return QStringLiteral(
        "QLabel { color: #9CA3AF; font-size: 13px; background: transparent; "
        "padding: 24px 16px; }"
    );
}

// ── File List Rows ──────────────────────────────────────────────────────────

QString ThemeManager::fileListStyle() {
    return QStringLiteral(
        "QListWidget { border: none; background: transparent; outline: none; }"
        "QListWidget::item { border-bottom: 1px solid #F1F5F9; }"
        "QListWidget::item:hover { background-color: #F9FAFB; }"
        "QListWidget::item:selected { background: transparent; color: #111827; }"
    );
}

QString ThemeManager::fileRowTitleStyle() {
    return QStringLiteral("font-weight: 600; color: #111827; font-size: 13px;");
}

QString ThemeManager::fileRowPathStyle() {
    return QStringLiteral("color: #9CA3AF; font-size: 11px;");
}

QString ThemeManager::fileRowOpenBtnStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #2563EB; "
        "border: 1px solid #BFDBFE; border-radius: 6px; "
        "padding: 4px 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #EFF6FF; }"
    );
}

QString ThemeManager::fileRowFavBtnStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #F59E0B; "
        "border: 1px solid #FDE68A; border-radius: 6px; "
        "padding: 4px 12px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #FEF3C7; }"
    );
}

QString ThemeManager::fileRowDeleteBtnStyle() {
    return QStringLiteral(
        "QPushButton { background: transparent; color: #EF4444; "
        "border: 1px solid #FECACA; border-radius: 6px; "
        "padding: 4px 14px; font-weight: 600; font-size: 12px; }"
        "QPushButton:hover { background: #FEE2E2; }"
    );
}

// ── Settings View ───────────────────────────────────────────────────────────

QString ThemeManager::settingsInputStyle() {
    return QStringLiteral(
        "QLineEdit { border: 1px solid #E2E8F0; border-radius: 8px; "
        "padding: 10px 14px; background: #F9FAFB; color: #111827; "
        "font-weight: 500; font-size: 13px; }"
        "QLineEdit:focus { border-color: #2563EB; background: #FFFFFF; }"
    );
}

QString ThemeManager::settingsLabelStyle() {
    return QStringLiteral(
        "font-weight: 600; color: #374151; margin-top: 12px; font-size: 13px;"
    );
}

QString ThemeManager::settingsTitleStyle() {
    return QStringLiteral(
        "font-size: 20px; font-weight: bold; color: #111827;"
    );
}

QString ThemeManager::settingsSubtitleStyle() {
    return QStringLiteral(
        "color: #6B7280; margin-bottom: 16px; font-size: 13px;"
    );
}

// ── Result Lists (search results, notes) ────────────────────────────────────

QString ThemeManager::resultListStyle() {
    return QStringLiteral(
        "QListWidget { border: 1px solid #E2E8F0; border-radius: 8px; "
        "background: #F9FAFB; padding: 4px; outline: none; }"
        "QListWidget::item { padding: 8px 10px; border-radius: 6px; "
        "margin: 1px; color: #374151; }"
        "QListWidget::item:hover { background: #EFF6FF; }"
        "QListWidget::item:selected { background: #DBEAFE; color: #1E40AF; }"
    );
}

// ── Home View ───────────────────────────────────────────────────────────────

QString ThemeManager::homeGreetingStyle() {
    return QStringLiteral(
        "font-size: 24px; font-weight: bold; color: #111827;"
    );
}

QString ThemeManager::homeSubGreetingStyle() {
    return QStringLiteral(
        "font-size: 14px; color: #6B7280; margin-bottom: 24px;"
    );
}

QString ThemeManager::homeSectionTitleStyle() {
    return QStringLiteral(
        "font-size: 15px; font-weight: 600; color: #111827;"
    );
}
