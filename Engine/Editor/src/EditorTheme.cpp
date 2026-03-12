#include "Aetherion/Editor/EditorTheme.h"

#include <QApplication>
#include <QFont>
#include <QFontInfo>
#include <QStyleFactory>

namespace Aetherion::Editor::EditorTheme {
namespace {
QString ColorHex(Semantic semantic) {
  switch (semantic) {
  case Semantic::PrimaryText:
    return QStringLiteral("#E2E8F0");
  case Semantic::SecondaryText:
    return QStringLiteral("#A7B3C4");
  case Semantic::MutedText:
    return QStringLiteral("#7B8798");
  case Semantic::Accent:
    return QStringLiteral("#4E8DC7");
  case Semantic::AccentHover:
    return QStringLiteral("#68A5DF");
  case Semantic::AccentPressed:
    return QStringLiteral("#2F6EAA");
  case Semantic::Success:
    return QStringLiteral("#62B883");
  case Semantic::Warning:
    return QStringLiteral("#D1A35B");
  case Semantic::Error:
    return QStringLiteral("#CF6B6B");
  }

  return QStringLiteral("#E2E8F0");
}

QString BuildSharedButtonStyle(const QString &background, const QString &border,
                               const QString &text, const QString &hover,
                               const QString &pressed,
                               const QString &disabledBackground) {
  return QStringLiteral(
             "QPushButton {"
             " background-color: %1;"
             " color: %2;"
             " border: 1px solid %3;"
             " border-radius: 5px;"
             " padding: 6px 12px;"
             " font-weight: 600;"
             "}"
             "QPushButton:hover { background-color: %4; border-color: %4; }"
             "QPushButton:pressed { background-color: %5; border-color: %5; }"
             "QPushButton:disabled { background-color: %6; color: %7; border-color: %6; }")
      .arg(background, text, border, hover, pressed, disabledBackground,
           ColorHex(Semantic::MutedText));
}
} // namespace

QString Hex(Semantic semantic) { return ColorHex(semantic); }

QColor Color(Semantic semantic) { return QColor(ColorHex(semantic)); }

void ApplyApplicationTheme() {
  QApplication::setStyle(QStyleFactory::create("Fusion"));

  QFont appFont(QStringLiteral("Segoe UI"), 9);
  if (!QFontInfo(appFont).exactMatch()) {
    appFont = QApplication::font();
    appFont.setPointSize(9);
  }
  appFont.setStyleStrategy(QFont::PreferAntialias);
  QApplication::setFont(appFont);

  if (qApp) {
    qApp->setStyleSheet(BuildApplicationStyleSheet());
  }
}

QString BuildApplicationStyleSheet() {
  const QString base0 = QStringLiteral("#1B2027");
  const QString base1 = QStringLiteral("#222832");
  const QString base2 = QStringLiteral("#2A313C");
  const QString base3 = QStringLiteral("#313A47");
  const QString inset = QStringLiteral("#151A20");
  const QString border = QStringLiteral("#3B4654");
  const QString borderStrong = QStringLiteral("#506074");
  const QString accent = ColorHex(Semantic::Accent);
  const QString accentHover = ColorHex(Semantic::AccentHover);
  const QString accentPressed = ColorHex(Semantic::AccentPressed);
  const QString text = ColorHex(Semantic::PrimaryText);
  const QString textSecondary = ColorHex(Semantic::SecondaryText);
  const QString textMuted = ColorHex(Semantic::MutedText);
  const QString selection = QStringLiteral("#385B81");
  const QString selectionSoft = QStringLiteral("#304861");
  const QString disabled = QStringLiteral("#232A33");
  const QString statusBg = QStringLiteral("#202730");

  return QStringLiteral(R"(
    * {
      color: %1;
      font-family: "Segoe UI", "Tahoma", sans-serif;
      font-size: 9pt;
      selection-background-color: %2;
      selection-color: %1;
      outline: none;
    }

    QMainWindow, QDialog, QWidget {
      background-color: %3;
    }

    QFrame {
      border: none;
    }

    QLabel#panelHeading {
      color: %1;
      font-size: 10pt;
      font-weight: 600;
      letter-spacing: 0.2px;
    }

    QLabel#secondaryText, QLabel#statusInfoText {
      color: %4;
    }

    QLabel#hintText {
      color: %5;
      font-style: italic;
    }

    QLabel#statusErrorText {
      color: %6;
    }

    QLabel#statusSuccessText {
      color: %7;
    }

    QLabel#toolbarSectionLabel {
      color: %5;
      font-size: 8pt;
      font-weight: 700;
      padding: 0 8px;
      text-transform: uppercase;
    }

    QLabel#statusTag {
      color: %1;
      background-color: %8;
      border: 1px solid %9;
      border-radius: 4px;
      padding: 2px 8px;
      font-size: 8pt;
      font-weight: 600;
    }

    QMenuBar {
      background-color: %10;
      border-bottom: 1px solid %9;
      padding: 2px 6px;
    }

    QMenuBar::item {
      background-color: transparent;
      color: %4;
      padding: 6px 10px;
      margin: 0 2px;
      border-radius: 4px;
    }

    QMenuBar::item:selected {
      background-color: %11;
      color: %1;
    }

    QMenu {
      background-color: %10;
      color: %1;
      border: 1px solid %9;
      padding: 4px;
    }

    QMenu::item {
      padding: 6px 20px 6px 10px;
      border-radius: 4px;
    }

    QMenu::item:selected {
      background-color: %2;
    }

    QMenu::separator {
      height: 1px;
      background-color: %9;
      margin: 4px 8px;
    }

    QToolBar#MainToolBar {
      background-color: %10;
      border-top: 1px solid %12;
      border-bottom: 1px solid %9;
      padding: 5px 8px;
      spacing: 4px;
    }

    QToolBar#MainToolBar::separator {
      width: 10px;
    }

    QToolButton {
      background-color: %11;
      color: %4;
      border: 1px solid %9;
      border-radius: 5px;
      padding: 5px 10px;
      min-height: 24px;
    }

    QToolButton:hover {
      background-color: %13;
      border-color: %14;
      color: %1;
    }

    QToolButton:pressed {
      background-color: %15;
      border-color: %15;
      color: %1;
    }

    QToolButton:checked {
      background-color: %2;
      border-color: %14;
      color: %1;
    }

    QPushButton {
      background-color: %11;
      color: %1;
      border: 1px solid %9;
      border-radius: 5px;
      padding: 6px 12px;
      font-weight: 600;
    }

    QPushButton:hover {
      background-color: %13;
      border-color: %14;
    }

    QPushButton:pressed {
      background-color: %15;
      border-color: %15;
    }

    QPushButton:disabled {
      background-color: %16;
      color: %5;
      border-color: %16;
    }

    QPushButton#accentButton {
      background-color: %2;
      border-color: %14;
      color: %1;
    }

    QPushButton#accentButton:hover {
      background-color: %14;
      border-color: %14;
    }

    QPushButton#accentButton:pressed {
      background-color: %15;
      border-color: %15;
    }

    QPushButton#dangerButton:hover {
      background-color: %6;
      border-color: %6;
      color: %1;
    }

    QStatusBar {
      background-color: %10;
      color: %4;
      border-top: 1px solid %9;
    }

    QStatusBar::item {
      border: none;
    }

    QTabWidget::pane {
      border: 1px solid %9;
      background-color: %3;
      top: -1px;
    }

    QTabBar::tab {
      background-color: %11;
      color: %4;
      padding: 7px 12px;
      border: 1px solid %9;
      border-bottom: none;
      border-top-left-radius: 5px;
      border-top-right-radius: 5px;
      margin-right: 3px;
      min-width: 82px;
    }

    QTabBar::tab:hover {
      background-color: %13;
      color: %1;
    }

    QTabBar::tab:selected {
      background-color: %3;
      color: %1;
      border-color: %14;
    }

    QTabWidget#BottomPanelTabs QTabBar::tab {
      min-width: 104px;
    }

    QLineEdit, QTextEdit, QPlainTextEdit, QAbstractSpinBox, QComboBox {
      background-color: %17;
      color: %1;
      border: 1px solid %9;
      border-radius: 5px;
      padding: 5px 8px;
    }

    QLineEdit:hover, QTextEdit:hover, QPlainTextEdit:hover, QAbstractSpinBox:hover, QComboBox:hover {
      border-color: %12;
    }

    QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QAbstractSpinBox:focus, QComboBox:focus {
      border-color: %14;
      background-color: %3;
    }

    QComboBox::drop-down {
      subcontrol-origin: padding;
      subcontrol-position: top right;
      width: 20px;
      border-left: 1px solid %9;
    }

    QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
      width: 16px;
      background-color: transparent;
      border: none;
    }

    QTreeView, QListView, QListWidget, QTableView, QTreeWidget, QTextEdit#consoleOutput {
      background-color: %17;
      border: 1px solid %9;
      border-radius: 5px;
      color: %1;
    }

    QTreeView::item, QListView::item, QListWidget::item, QTreeWidget::item {
      padding: 4px;
    }

    QTreeView::item:hover, QListView::item:hover, QListWidget::item:hover, QTreeWidget::item:hover {
      background-color: %11;
    }

    QTreeView::item:selected, QListView::item:selected, QListWidget::item:selected, QTreeWidget::item:selected {
      background-color: %18;
      color: %1;
    }

    QHeaderView::section {
      background-color: %10;
      color: %4;
      padding: 6px;
      border: none;
      border-right: 1px solid %9;
      border-bottom: 1px solid %9;
      font-weight: 600;
    }

    QGroupBox {
      border: 1px solid %9;
      border-radius: 6px;
      margin-top: 16px;
      padding-top: 8px;
      font-weight: 600;
    }

    QGroupBox::title {
      subcontrol-origin: margin;
      subcontrol-position: top left;
      left: 8px;
      padding: 0 4px;
      color: %4;
    }

    QCheckBox, QRadioButton {
      spacing: 6px;
      color: %4;
    }

    QCheckBox::indicator {
      width: 14px;
      height: 14px;
      border: 1px solid %12;
      border-radius: 3px;
      background-color: %17;
    }

    QCheckBox::indicator:checked {
      background-color: %2;
      border-color: %14;
    }

    QSplitter::handle {
      background-color: %9;
    }

    QSplitter::handle:hover {
      background-color: %14;
    }

    QScrollBar:vertical {
      background-color: transparent;
      width: 11px;
      margin: 0;
    }

    QScrollBar::handle:vertical {
      background-color: %12;
      min-height: 24px;
      border-radius: 5px;
      margin: 2px;
    }

    QScrollBar::handle:vertical:hover {
      background-color: %18;
    }

    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
    QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
      background: none;
      width: 0;
      height: 0;
    }

    QScrollBar:horizontal {
      background-color: transparent;
      height: 11px;
      margin: 0;
    }

    QScrollBar::handle:horizontal {
      background-color: %12;
      min-width: 24px;
      border-radius: 5px;
      margin: 2px;
    }

    QScrollBar::handle:horizontal:hover {
      background-color: %18;
    }

    QWidget#viewportOverlay, QWidget#viewportMetrics {
      background-color: rgba(28, 34, 41, 210);
      border: 1px solid rgba(104, 165, 223, 120);
      border-radius: 7px;
    }

    QLabel#viewportHudKey {
      color: %1;
      background-color: rgba(104, 165, 223, 36);
      border: 1px solid rgba(104, 165, 223, 70);
      border-radius: 4px;
      padding: 1px 5px;
      font-weight: 600;
    }

    QLabel#viewportHudPrimary, QLabel#perfHudLabel {
      color: %1;
      font-weight: 600;
    }

    QLabel#viewportHudAccent {
      color: %14;
      font-weight: 600;
    }

    QToolTip {
      background-color: %10;
      color: %1;
      border: 1px solid %9;
      padding: 5px 7px;
    }
  )")
      .arg(text, accent, base0, textSecondary, textMuted,
           ColorHex(Semantic::Error), ColorHex(Semantic::Success), statusBg,
           border, base1, base2, base3, selectionSoft, accentHover,
           accentPressed, disabled, inset, selection);
}

QString SecondaryTextStyle(bool italic) {
  return italic
             ? QStringLiteral("color: %1; font-style: italic;")
                   .arg(ColorHex(Semantic::MutedText))
             : QStringLiteral("color: %1;")
                   .arg(ColorHex(Semantic::SecondaryText));
}

QString SearchFieldStyleSheet() { return QString(); }

QString AccentButtonStyleSheet() {
  return BuildSharedButtonStyle(ColorHex(Semantic::Accent),
                                ColorHex(Semantic::AccentHover),
                                ColorHex(Semantic::PrimaryText),
                                ColorHex(Semantic::AccentHover),
                                ColorHex(Semantic::AccentPressed),
                                QStringLiteral("#232A33"));
}

QString NeutralButtonStyleSheet() {
  return BuildSharedButtonStyle(QStringLiteral("#2A313C"),
                                QStringLiteral("#3B4654"),
                                ColorHex(Semantic::PrimaryText),
                                QStringLiteral("#313A47"),
                                QStringLiteral("#252C35"),
                                QStringLiteral("#232A33"));
}

QString DangerButtonStyleSheet() {
  return BuildSharedButtonStyle(QStringLiteral("#2A313C"),
                                QStringLiteral("#3B4654"),
                                ColorHex(Semantic::SecondaryText),
                                ColorHex(Semantic::Error),
                                QStringLiteral("#A35050"),
                                QStringLiteral("#232A33"));
}

QString ComponentToggleStyleSheet() {
  return QStringLiteral(
             "QPushButton {"
             " text-align: left;"
             " font-weight: 600;"
             " padding: 6px 8px;"
             " background-color: #2A313C;"
             " border: 1px solid #3B4654;"
             " border-radius: 5px;"
             " color: %1;"
             "}"
             "QPushButton:checked {"
             " background-color: #314050;"
             " border-color: %2;"
             "}")
      .arg(ColorHex(Semantic::PrimaryText), ColorHex(Semantic::AccentHover));
}

QString ComponentHeaderFrameStyleSheet() {
  return QStringLiteral(
      "background-color: #2A313C; border: 1px solid #3B4654; border-radius: 6px;");
}

QString CodeEditorStyleSheet() {
  return QStringLiteral(
      "QTextEdit { background-color: #151A20; color: %1; border: 1px solid #3B4654; "
      "border-radius: 6px; font-family: Consolas, 'Courier New', monospace; }")
      .arg(ColorHex(Semantic::PrimaryText));
}

QString BuildCopilotPanelStyleSheet() {
  const QString accent = ColorHex(Semantic::Accent);
  const QString accentHover = ColorHex(Semantic::AccentHover);
  const QString accentPressed = ColorHex(Semantic::AccentPressed);
  const QString success = ColorHex(Semantic::Success);
  const QString primary = ColorHex(Semantic::PrimaryText);
  const QString secondary = ColorHex(Semantic::SecondaryText);
  const QString muted = ColorHex(Semantic::MutedText);

  return QStringLiteral(R"(
      AICopilotPanel {
          background-color: #1B2027;
      }

      #activityFrame {
          background-color: #222832;
          border-bottom: 1px solid #3B4654;
      }

      #activityIcon {
          color: %1;
          font-size: 12px;
      }

      #activityLabel {
          color: %2;
          font-weight: 600;
          font-size: 12px;
      }

      #activityDetails, #logHeader {
          color: %3;
          font-size: 11px;
      }

      #currentToolFrame {
          background-color: #26303B;
          border-radius: 6px;
          margin: 2px 0;
          border: 1px solid #3B4654;
      }

      #toolNameLabel {
          color: %4;
          font-weight: 600;
          font-size: 11px;
      }

      #toolParamsLabel {
          color: %5;
          font-size: 10px;
          font-family: Consolas, monospace;
      }

      #activityLogFrame, #codeViewerHeader, #inputFrame {
          background-color: #222832;
          border-top: 1px solid #3B4654;
      }

      #activityLog, #chatHistory, #codeViewer {
          background-color: #151A20;
          color: %2;
          border: 1px solid #3B4654;
          border-radius: 5px;
      }

      #chatFrame, #codeViewerFrame {
          background-color: #1B2027;
      }

      #codeViewerTitle {
          color: %2;
          font-weight: 600;
      }

      #inputField {
          background-color: #151A20;
          border: 1px solid #3B4654;
          border-radius: 5px;
          padding: 6px 8px;
          color: %2;
      }

      #inputField:focus {
          border-color: %6;
      }

      #submitButton, #copyCodeBtn {
          background-color: %6;
          color: %2;
          border: 1px solid %7;
          border-radius: 5px;
          padding: 6px 12px;
          font-weight: 600;
      }

      #submitButton:hover, #copyCodeBtn:hover {
          background-color: %7;
          border-color: %7;
      }

      #submitButton:pressed, #copyCodeBtn:pressed {
          background-color: %8;
          border-color: %8;
      }

      #submitButton:disabled, #copyCodeBtn:disabled {
          background-color: #232A33;
          color: %3;
          border-color: #232A33;
      }

      #closeCodeBtn {
          background-color: #2A313C;
          color: %5;
          border: 1px solid #3B4654;
          border-radius: 4px;
      }

      #closeCodeBtn:hover {
          background-color: #CF6B6B;
          border-color: #CF6B6B;
          color: %2;
      }

      #mainSplitter::handle {
          background-color: #3B4654;
          width: 2px;
      }

      #mainSplitter::handle:hover {
          background-color: %7;
      }
    )")
      .arg(success, primary, muted, accent, secondary, accent, accentHover,
           accentPressed);
}

} // namespace Aetherion::Editor::EditorTheme
