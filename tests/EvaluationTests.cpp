#include <vector>
#include <utility>
#include <atomic>
#include <chrono>
#define private public
#include "ChessBoard.h"
#undef private
#include "AIEngine.h"
#include <cassert>
#include <iostream>
#include <cstring>

struct EvaluationTestAccess : AIEngine {
    int Score(const ChessBoard& board, PieceColor side) const {
        return Evaluate(board, side);
    }
    ChessMove SelectRoot(ChessBoard& board, ChessMove book) {
        std::memset(m_rootBook, 0, sizeof(m_rootBook));
        m_rootBook[book.fromY * 9 + book.fromX][book.toY * 9 + book.toX] = true;
        m_startTime = std::chrono::steady_clock::now();
        m_nodes = 0; m_timedOut = false;
        ChessMove selected;
        SearchRoot(board, 1, -1000000, 1000000, selected, 150);
        assert(!m_timedOut);
        return selected;
    }
    void CheckSearchVariety(ChessBoard& board, bool unique) {
        std::memset(m_rootBook, 0, sizeof(m_rootBook));
        m_startTime = std::chrono::steady_clock::now();
        m_nodes = 0; m_timedOut = false;
        ChessMove bestMove;
        std::vector<ChessMove> safe;
        const auto hash = board.GetHash();
        const int best = SearchRoot(board, 2, -1000000, 1000000, bestMove, 30, nullptr, &safe);
        assert(!m_timedOut && !safe.empty());
        assert(unique ? safe.size() == 1 : safe.size() > 1);
        for (const auto& move : safe) {
            SearchUndo undo; board.MakeSearchMove(move, undo);
            const int score = -Search(board, 1, -1000000, 1000000, 1, true);
            board.UndoSearchMove(move, undo);
            assert(score >= best - 25 && score <= best && board.GetHash() == hash);
        }
        safe.clear();
        SearchRoot(board, 2, -1000000, -999999, bestMove, 30, nullptr, &safe);
        assert(safe.empty()); // 截断根节点不构成完整候选集合。
        RequestStop(); safe.clear();
        SearchRoot(board, 2, -1000000, 1000000, bestMove, 30, nullptr, &safe);
        assert(m_timedOut && safe.empty()); // 超时或取消不提交部分评分。
        m_stopRequested.store(false);
    }
};

static void Put(ChessBoard& board, PieceType type, int x, int y) {
    board.m_board[x][y] = new ChessPiece(type, x, y);
}

int main() {
    EvaluationTestAccess ai;
    ChessBoard opening;
    opening.Initialize();
    assert(ai.Score(opening, RED_P) == 0);
    assert(opening.MovePiece(7, 7, 4, 7));
    assert(ai.Score(opening, RED_P) == -ai.Score(opening, BLACK_P));
    ChessBoard reflected;
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            ChessPiece* piece = opening.GetPiece(x, y);
            if (piece) Put(reflected, static_cast<PieceType>(
                (piece->GetType() + 7) % 14), 8 - x, 9 - y);
        }
    assert(ai.Score(opening, RED_P) == ai.Score(reflected, BLACK_P));
    auto pawnValue = [&](int x, int y, PieceType type) {
        ChessBoard board;
        Put(board, type, x, y);
        return ai.Score(board, type == PIECE_R_SOLDIER ? RED_P : BLACK_P);
    };
    const int edge = pawnValue(0, 6, PIECE_R_SOLDIER);
    const int flank = pawnValue(2, 6, PIECE_R_SOLDIER);
    const int center = pawnValue(4, 6, PIECE_R_SOLDIER);
    assert(center > flank && flank > edge);
    assert(edge == pawnValue(8, 6, PIECE_R_SOLDIER));
    assert(center == pawnValue(4, 3, PIECE_B_SOLDIER));
    assert(pawnValue(4, 4, PIECE_R_SOLDIER) > center);
    assert(pawnValue(4, 1, PIECE_R_SOLDIER) > pawnValue(4, 0, PIECE_R_SOLDIER));
    ChessBoard pawnPair;
    Put(pawnPair, PIECE_R_SOLDIER, 3, 4);
    Put(pawnPair, PIECE_R_SOLDIER, 4, 4);
    assert(ai.Score(pawnPair, RED_P) > pawnValue(3, 4, PIECE_R_SOLDIER) +
        pawnValue(4, 4, PIECE_R_SOLDIER));
    const int beforeSearchMove = ai.Score(opening, BLACK_P);
    SearchUndo reversible;
    opening.MakeSearchMove({7, 0, 6, 2}, reversible);
    opening.UndoSearchMove({7, 0, 6, 2}, reversible);
    assert(ai.Score(opening, BLACK_P) == beforeSearchMove);
    ChessBoard developed(opening), cramped(opening);
    SearchUndo undo;
    developed.MakeSearchMove({7, 0, 6, 2}, undo);
    cramped.MakeSearchMove({1, 2, 2, 2}, undo);
    assert(ai.Score(developed, BLACK_P) > ai.Score(cramped, BLACK_P));
    ChessBoard guarded(opening), exposed(opening);
    delete exposed.m_board[3][0]; exposed.m_board[3][0] = nullptr;
    delete exposed.m_board[5][0]; exposed.m_board[5][0] = nullptr;
    // 缺双士的损失应超过双士本身的子力值。
    assert(ai.Score(guarded, BLACK_P) - ai.Score(exposed, BLACK_P) > 400);
    ChessBoard tactic;
    tactic.m_currentPlayer = BLACK_P;
    Put(tactic, PIECE_B_GENERAL, 4, 0);
    Put(tactic, PIECE_R_GENERAL, 4, 9);
    Put(tactic, PIECE_R_SOLDIER, 4, 5);
    Put(tactic, PIECE_B_CHARIOT, 0, 0);
    Put(tactic, PIECE_R_CHARIOT, 0, 3);
    const auto tacticalMove = ai.SelectRoot(tactic, {0, 0, 1, 0});
    assert(tacticalMove.fromX == 0 && tacticalMove.fromY == 0 &&
        tacticalMove.toX == 0 && tacticalMove.toY == 3); // 不让库偏好盖过明显得车。
    ai.CheckSearchVariety(tactic, true);
    ChessBoard balanced; balanced.Initialize();
    assert(balanced.MovePiece(4, 6, 4, 5));
    ai.CheckSearchVariety(balanced, false);
    std::cout << "轻量评估测试通过；边兵=" << edge << "，三七兵=" << flank
        << "，中兵=" << center << "。\n";
}
