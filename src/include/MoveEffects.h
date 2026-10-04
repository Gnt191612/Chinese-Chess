#pragma once
#include "ChessBoard.h"
#include "BoardLayout.h"
#include <cmath>

// 只计算绘制位置，正式棋盘与鼠标命中坐标始终不变。
namespace MoveEffects {
struct Frame { int x, y; double progress; };
inline int DurationMs(const ChessMove& move) {
    const double dx = move.toX - move.fromX, dy = move.toY - move.fromY;
    const int duration = 180 + static_cast<int>(16 * std::sqrt(dx * dx + dy * dy));
    return duration > 320 ? 320 : duration;
}
inline Frame Sample(const ChessMove& move, double time) {
    const double t = time < 0 ? 0 : time > 1 ? 1 : time;
    // 两端速度、加速度均归零，棋子贴着棋盘平稳滑行，不漂浮、不越过落点。
    const double progress = t * t * t * (10 + t * (-15 + 6 * t));
    return {BoardLayout::Position(move.fromX) + static_cast<int>(std::lround((move.toX - move.fromX) * BoardLayout::Spacing * progress)),
        BoardLayout::Position(move.fromY) + static_cast<int>(std::lround((move.toY - move.fromY) * BoardLayout::Spacing * progress)), progress};
}
inline void Draw(const ChessPiece& piece, const ChessMove& move, double time) {
    const auto frame = Sample(move, time);
    setlinestyle(PS_SOLID, 1);
    // 收尾时四角轻轻收拢；保留棋子本身的木质投影，不再叠加第二层阴影。
    if (time > 0.55 && time < 1) {
        const int targetX = BoardLayout::Position(move.toX), targetY = BoardLayout::Position(move.toY);
        const double settle = (time - 0.55) / 0.45;
        const int reach = 30 + static_cast<int>(std::lround(6 * (1 - settle)));
        const double strength = std::sin(3.14159265359 * settle);
        setlinecolor(RGB(220 - static_cast<int>(43 * strength),
            178 - static_cast<int>(52 * strength), 119 - static_cast<int>(59 * strength)));
        for (int sx : {-1, 1})
            for (int sy : {-1, 1}) {
                const int x = targetX + sx * reach, y = targetY + sy * reach;
                line(x - sx * 6, y, x, y);
                line(x, y, x, y - sy * 6);
            }
    }
    piece.Draw(frame.x, frame.y);
    setlinestyle(PS_SOLID, 1);
}
}
