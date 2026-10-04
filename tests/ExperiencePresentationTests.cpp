#include <vector>
#include <utility>
#define private public
#include "ChessBoard.h"
#undef private
#include "OpeningBook.h"
#include "VictoryEffects.h"
#include "TacticalEffects.h"
#include "MoveEffects.h"
#include "SoundEffects.h"
#include <cassert>
#include <iostream>
#include <cstring>

int main() {
    assert(OpeningBook::Entries().size() == 168);
    for (const auto& line : DeepOpeningLines::Lines()) {
        assert(line.size() >= 20 && line.size() <= 30);
        for (bool mirror : {false, true}) {
            ChessBoard replay; replay.Initialize();
            for (ChessMove move : line) {
                if (mirror) { move.fromX = 8 - move.fromX; move.toX = 8 - move.toX; }
                if (replay.GetCurrentPlayer() == BLACK_P) {
                    bool found = false;
                    for (const auto& candidate : OpeningBook::Candidates(replay, 1))
                        if (candidate.fromX == move.fromX && candidate.fromY == move.fromY &&
                            candidate.toX == move.toX && candidate.toY == move.toY) found = true;
                    assert(found);
                }
                assert(replay.MovePiece(move.fromX, move.fromY, move.toX, move.toY));
            }
        }
    }
    for (int cannonX : {7, 1}) {
        ChessBoard board;
        board.Initialize();
        assert(board.MovePiece(cannonX, 7, 4, 7));
        const auto candidates = OpeningBook::Candidates(board);
        assert(candidates.size() >= 4);
        const ChessMove move = candidates[0];
        assert(board.MovePiece(move.fromX, move.fromY, move.toX, move.toY));
        assert(OpeningBook::Candidates(board).empty()); // 不给红方自动走棋。
    }
    for (const auto& entry : OpeningBook::Entries()) {
        assert(entry.move.fromX >= 0 && entry.move.toX < 9);
        assert(entry.position[entry.move.fromY * 9 + entry.move.fromX] >= 7);
        ChessBoard replay;
        replay.m_currentPlayer = BLACK_P;
        replay.m_hash = entry.hash;
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 9; ++x) {
                const int type = entry.position[y * 9 + x];
                if (type >= 0) replay.m_board[x][y] = new ChessPiece(
                    static_cast<PieceType>(type), x, y);
            }
        assert(replay.MovePiece(entry.move.fromX, entry.move.fromY,
            entry.move.toX, entry.move.toY));
    }
    ChessBoard unfamiliar;
    unfamiliar.Initialize();
    assert(unfamiliar.MovePiece(4, 6, 4, 5));
    assert(OpeningBook::Candidates(unfamiliar).empty());
    VictoryEffects victory;
    assert(!victory.Update(false, false));
    assert(!victory.Update(true, true)); // 等最后一步动画落地。
    assert(victory.Update(true, false));
    assert(!victory.Update(true, false)); // 每次胜利只播放一次。
    assert(!victory.Update(false, false)); // 悔棋取消特效。
    assert(!victory.IsActive());
    assert(victory.Update(true, false)); // 再次获胜可再次播放。
    const auto wave = SoundEffects::MakeWave(8);
    assert(wave.size() == 44 + 22050 * 1400 / 1000 * 2);
    assert(SoundEffects::MakeWave(2).size() == 44 + 22050 * 280 / 1000 * 2);
    assert(SoundEffects::MakeWave(6).size() == 44 + 22050 * 640 / 1000 * 2);
    TacticalEffects tactical;
    tactical.Start(1, 290, 290, 290, 50); assert(!tactical.IsActive());
    tactical.Start(6, 230, 230, 290, 50); assert(tactical.IsActive());
    assert(TacticalEffects::DurationSeconds(1) == 0);
    assert(TacticalEffects::DurationSeconds(2) == 0.65);
    assert(TacticalEffects::DurationSeconds(4) == 0.85);
    assert(TacticalEffects::DurationSeconds(6) == 0.85);
    for (int event : {2, 4, 6}) {
        const double duration = TacticalEffects::DurationSeconds(event);
        assert(TacticalEffects::RevealAt(event, -1) == 0);
        assert(TacticalEffects::RevealAt(event, 0) == 0);
        assert(TacticalEffects::RevealAt(event, 0.2) == 1);
        assert(TacticalEffects::RevealAt(event, duration - 0.09) > 0);
        assert(TacticalEffects::RevealAt(event, duration) == 0);
        assert(TacticalEffects::RevealAt(event, duration + 1) == 0);
    }
    initgraph(940, 700);
    setbkcolor(RGB(244, 235, 218)); cleardevice();
    ChessBoard display;
    display.Initialize(); display.DrawWithSelection(-1, -1);
    victory.DrawAt(0.9);
    saveimage(_T("tests\\victory-preview.png"));
    cleardevice(); display.DrawWithSelection(-1, -1);
    tactical.DrawAt(0.25);
    saveimage(_T("tests\\ui-preview.png"));
    for (int event : {2, 4, 6}) {
        cleardevice(); display.DrawWithSelection(-1, -1);
        tactical.Start(event, 230, 230, 290, 50);
        tactical.DrawAt(0.35);
        saveimage(event == 2 ? _T("tests\\capture-preview.png") :
            event == 4 ? _T("tests\\check-preview.png") : _T("tests\\capture-check-preview.png"));
        cleardevice(); display.DrawWithSelection(-1, -1);
        IMAGE before, after;
        getimage(&before, 0, 0, 940, 700);
        tactical.DrawAt(TacticalEffects::DurationSeconds(event));
        getimage(&after, 0, 0, 940, 700);
        assert(std::memcmp(GetImageBuffer(&before), GetImageBuffer(&after), 940 * 700 * sizeof(DWORD)) == 0);
    }
    // 圆内无文字处只轻染背景，且屏幕与离屏双缓冲的绘制设备均恢复正确。
    cleardevice(); display.DrawWithSelection(-1, -1);
    const COLORREF beneath = getpixel(325, 332);
    tactical.Start(4, 230, 230, 290, 50); tactical.DrawAt(0.35);
    const COLORREF tinted = getpixel(325, 332);
    assert(tinted != beneath && tinted != RGB(169, 51, 39));
    assert(std::abs(int(GetRValue(tinted)) - int(GetRValue(beneath))) < 20);
    assert(std::abs(int(GetGValue(tinted)) - int(GetGValue(beneath))) < 20);
    assert(GetWorkingImage() == nullptr);
    IMAGE offscreen(940, 700);
    SetWorkingImage(&offscreen); cleardevice(); display.DrawWithSelection(-1, -1);
    tactical.DrawAt(0.35); assert(GetWorkingImage() == &offscreen);
    SetWorkingImage(nullptr);
    tactical.Clear(); assert(!tactical.IsActive());
    const ChessMove moving{7,9,6,7};
    const auto start = MoveEffects::Sample(moving, -1);
    const auto middle = MoveEffects::Sample(moving, 0.5);
    const auto end = MoveEffects::Sample(moving, 2);
    assert(start.x == 470 && start.y == 590 && start.progress == 0);
    assert(middle.x == 440 && middle.y == 530 && middle.progress == 0.5);
    assert(end.x == 410 && end.y == 470 && end.progress == 1);
    // 横向、纵向、斜向、马步及演示长步均不偏离直线路径，不越界、不反向。
    for (const ChessMove move : {ChessMove{0,0,8,0}, ChessMove{8,9,8,0},
        ChessMove{2,9,4,7}, moving, ChessMove{4,9,4,0}, ChessMove{0,3,0,4}}) {
        double previousProgress = -1;
        const int dx = (move.toX - move.fromX) * BoardLayout::Spacing;
        const int dy = (move.toY - move.fromY) * BoardLayout::Spacing;
        for (int step = 0; step <= 100; ++step) {
            const auto frame = MoveEffects::Sample(move, step / 100.0);
            assert(frame.progress >= previousProgress && frame.progress <= 1);
            previousProgress = frame.progress;
            const int ox = frame.x - BoardLayout::Position(move.fromX);
            const int oy = frame.y - BoardLayout::Position(move.fromY);
            assert(std::abs(ox * dy - oy * dx) <= (std::abs(dx) + std::abs(dy)) / 2);
            assert(frame.x >= 50 && frame.x <= 530 && frame.y >= 50 && frame.y <= 590);
        }
        assert(MoveEffects::DurationMs(move) >= 180 && MoveEffects::DurationMs(move) <= 320);
    }
    assert(MoveEffects::DurationMs(ChessMove{0,3,0,4}) < MoveEffects::DurationMs(ChessMove{4,9,4,0}));
    ChessBoard empty;
    cleardevice(); empty.DrawWithSelection(-1, -1);
    int markCount = 0, cornerCount = 0;
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            if (!BoardLayout::HasSetupMark(x, y)) continue;
            ++markCount;
            for (int side : {-1, 1})
                for (int vertical : {-1, 1}) {
                    const auto pixel = getpixel(BoardLayout::Position(x) + side * 4,
                        BoardLayout::Position(y) + vertical * 8);
                    const bool inside = !((x == 0 && side == -1) || (x == 8 && side == 1));
                    assert((pixel == RGB(113, 76, 41)) == inside);
                    if (inside) ++cornerCount;
                }
        }
    assert(markCount == 14 && cornerCount == 48);
    saveimage(_T("tests\\board-marks-preview.png"));
    cleardevice(); display.DrawWithSelection(-1, -1, 7, 9);
    const auto original = display.GetHash();
    MoveEffects::Draw(*display.GetPiece(7, 9), moving, 0.5);
    assert(display.GetHash() == original);
    saveimage(_T("tests\\move-preview.png"));
    closegraph();
    std::cout << "开局路线、胜利生命周期和音效测试通过。\n";
}
