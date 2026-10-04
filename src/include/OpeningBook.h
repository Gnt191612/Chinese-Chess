#pragma once
#include "ChessBoard.h"
#include "DeepOpeningLines.h"
#include <array>
#include <vector>

// 自编的常见起手坐标，外部材料仅用于事实核对；来源见 docs/OPENING_BOOK.md。
namespace OpeningBook {
constexpr int FamilyCount = 14;
inline int Weight(int family) {
    const int weights[] = {0,45,25,20,10,15,45,35,50,50,50,50,50,50,20};
    return family >= 1 && family <= FamilyCount ? weights[family] : 0;
}
struct Entry {
    uint64_t hash;
    std::array<int8_t, 90> position;
    ChessMove move;
    int family;
};
inline std::array<int8_t, 90> Position(const ChessBoard& board) {
    std::array<int8_t, 90> result;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 9; ++x) {
            ChessPiece* piece = board.GetPiece(x, y);
            result[y * 9 + x] = static_cast<int8_t>(piece ? piece->GetType() : PIECE_NONE);
        }
    return result;
}
inline const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = [] {
        std::vector<Entry> result;
        // 常见中炮体系、起马、飞相、进兵；并生成左右镜像。
        std::vector<std::vector<ChessMove>> lines = {
            {{7,7,4,7},{7,0,6,2},{7,9,6,7},{8,0,7,0},
             {8,9,7,9},{1,0,2,2},{2,6,2,5},{6,3,6,4},{7,9,7,3},{7,2,8,2},
             {7,3,6,3},{8,2,8,1},{1,9,2,7},{8,1,6,1},{1,7,0,7},{7,0,7,2}},
            {{7,7,4,7},{7,0,6,2},{7,9,6,7},{6,3,6,4},
             {8,9,7,9},{8,0,7,0},{7,9,7,3},{1,0,2,2},{2,6,2,5},{7,2,8,2}},
            {{7,9,6,7},{6,3,6,4},{1,7,3,7},{7,0,6,2},{1,9,2,7},{1,0,2,2}},
            {{6,9,4,7},{7,2,5,2},{2,6,2,5},{7,0,6,2},{1,9,2,7},{8,0,7,0}},
            {{2,6,2,5},{1,2,4,2},{1,9,2,7},{1,0,2,2}},
            // 反宫马：马、士角炮、马，配合红方五六炮。
            {{7,7,4,7},{1,0,2,2},{7,9,6,7},{7,2,5,2},
             {8,9,7,9},{7,0,6,2},{1,7,3,7},{0,0,1,0}},
            // 顺炮直车对横车。
            {{7,7,4,7},{7,2,4,2},{7,9,6,7},{7,0,6,2},
             {8,9,7,9},{8,0,8,1},{1,9,2,7},{8,1,3,1}},
            // 左炮封车转列炮，不能把炮过河一概当作坏棋。
            {{7,7,4,7},{7,0,6,2},{7,9,6,7},{8,0,7,0},
             {8,9,7,9},{7,2,7,6},{6,6,6,5},{1,2,4,2}},
            // 直接列炮及正常出子。
            {{7,7,4,7},{1,2,4,2},{7,9,6,7},{7,0,6,2},
             {8,9,7,9},{8,0,7,0},{1,9,2,7},{1,0,2,2}},
            // 红方先出左马，仍可转入屏风马布局。
            {{7,7,4,7},{7,0,6,2},{1,9,2,7},{1,0,2,2},
             {7,9,6,7},{8,0,7,0},{8,9,7,9},{6,3,6,4}},
            // 仙人指路对卒底炮、对挺兵、飞象，独立短路线。
            {{2,6,2,5},{1,2,2,2},{7,9,6,7},{7,0,6,2},{8,9,7,9},{8,0,7,0}},
            {{2,6,2,5},{6,3,6,4},{7,9,6,7},{7,0,6,2},{1,9,2,7},{1,0,2,2}},
            {{2,6,2,5},{2,0,4,2},{7,9,6,7},{7,0,6,2},{1,9,2,7},{1,0,2,2}},
            // 飞相对挺卒，起马对跳马。
            {{6,9,4,7},{6,3,6,4},{7,9,6,7},{7,0,6,2},{1,9,2,7},{1,0,2,2}},
            {{7,9,6,7},{7,0,6,2},{2,6,2,5},{6,3,6,4},{1,9,2,7},{1,0,2,2}},
            // 过宫炮对跳马、挺卒。
            {{7,7,3,7},{7,0,6,2},{7,9,6,7},{6,3,6,4},{1,9,2,7},{1,0,2,2}},
            {{7,7,3,7},{6,3,6,4},{7,9,6,7},{7,0,6,2},{1,9,2,7},{1,0,2,2}}
        };
        std::vector<int> families = {1,1,10,8,5,2,3,4,4,1,6,7,14,9,11,12,13};
        for (const auto& line : DeepOpeningLines::Lines()) {
            lines.push_back(line); families.push_back(1);
        }
        size_t lineIndex = 0;
        for (const auto& line : lines) {
            for (bool mirror : {false, true}) {
                ChessBoard board;
                board.Initialize();
                for (ChessMove move : line) {
                    if (mirror) { move.fromX = 8 - move.fromX; move.toX = 8 - move.toX; }
                    const auto position = Position(board);
                    const auto hash = board.GetHash();
                    const bool black = board.GetCurrentPlayer() == BLACK_P;
                    if (!board.MovePiece(move.fromX, move.fromY, move.toX, move.toY)) break;
                    if (black) result.push_back({hash, position, move, families[lineIndex]});
                }
            }
            ++lineIndex;
        }
        return result;
    }();
    return entries;
}
inline std::vector<ChessMove> Candidates(const ChessBoard& board, int family = 0) {
    std::vector<ChessMove> result;
    if (board.IsGameOver() || board.GetCurrentPlayer() != BLACK_P) return result;
    const auto position = Position(board);
    for (const auto& entry : Entries()) {
        if (entry.hash != board.GetHash() || entry.position != position) continue;
        if (family && entry.family != family) continue;
        bool duplicate = false;
        for (const auto& move : result)
            if (move.fromX == entry.move.fromX && move.fromY == entry.move.fromY &&
                move.toX == entry.move.toX && move.toY == entry.move.toY) duplicate = true;
        if (!duplicate) result.push_back(entry.move);
    }
    return result;
}
}
