#pragma once

#include <QColor>
#include <QString>

namespace Aetherion::Editor::EditorTheme {

enum class Semantic {
  PrimaryText,
  SecondaryText,
  MutedText,
  Accent,
  AccentHover,
  AccentPressed,
  Success,
  Warning,
  Error,
};

QString Hex(Semantic semantic);
QColor Color(Semantic semantic);

void ApplyApplicationTheme();
QString BuildApplicationStyleSheet();

QString SecondaryTextStyle(bool italic = false);
QString SearchFieldStyleSheet();
QString AccentButtonStyleSheet();
QString NeutralButtonStyleSheet();
QString DangerButtonStyleSheet();
QString ComponentToggleStyleSheet();
QString ComponentHeaderFrameStyleSheet();
QString CodeEditorStyleSheet();
QString BuildCopilotPanelStyleSheet();

} // namespace Aetherion::Editor::EditorTheme
