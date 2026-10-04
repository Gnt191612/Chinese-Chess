#pragma once

// 绘制与鼠标命中共用棋盘坐标。
namespace BoardLayout {
constexpr int Origin = 50;
constexpr int Spacing = 60;
constexpr int PieceRadius = 26;
constexpr int WindowWidth = 940;
constexpr int WindowHeight = 700;
constexpr int PanelLeft = 600;
constexpr int PanelRight = 910;
constexpr int UndoTop = 490;
constexpr int UndoBottom = 532;
constexpr int RestartTop = 544;
constexpr int RestartBottom = 588;
constexpr int SwitchTop = RestartTop;
constexpr int SwitchBottom = RestartBottom;
constexpr int ExitTop = 600;
constexpr int ExitBottom = 644;
inline int Position(int index) { return Origin + index * Spacing; }
inline bool HasSetupMark(int x, int y) {
    return ((y == 2 || y == 7) && (x == 1 || x == 7)) ||
        ((y == 3 || y == 6) && x >= 0 && x <= 8 && x % 2 == 0);
}
inline bool Inside(int x, int y, int left, int top, int right, int bottom) {
    return x >= left && x <= right && y >= top && y <= bottom;
}
}
