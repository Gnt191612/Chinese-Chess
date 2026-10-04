#include <vector>
#include <future>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <iostream>
#define private public
#include "GameLogic.h"
#undef private
#include "SoundEffects.h"

static void Clear(ChessBoard& board) {
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            delete board.m_board[x][y];
            board.m_board[x][y] = nullptr;
        }
    board.m_currentPlayer = RED_P;
    board.m_gameOver = false;
}

int main() {
    int failures = 0;
    GameLogic undoGame;
    const auto initialHash = undoGame.m_board.GetHash();
    if (undoGame.UndoTurn()) ++failures;
    auto finishAnimation = [](GameLogic& game) {
        game.m_animationStartedAt -= std::chrono::milliseconds(321);
        return game.UpdatePresentation();
    };
    auto playerMove = [](GameLogic& game, int x, int y, int tx, int ty) {
        game.HandleClick(50 + x * 60, 50 + y * 60);
        game.HandleClick(50 + tx * 60, 50 + ty * 60);
    };
    auto reply = [](GameLogic& game, int x, int y, int tx, int ty) {
        game.m_aiPositionHash = game.m_board.GetHash();
        std::promise<AIEngine::Move> result;
        game.m_aiFuture = result.get_future();
        result.set_value({{x, y}, {tx, ty}});
        game.AIPlay();
    };
    playerMove(undoGame, 4, 6, 4, 5);
    if (!undoGame.IsAnimating() || !undoGame.UndoTurn() ||
        undoGame.m_board.GetHash() != initialHash || !undoGame.IsPlayerTurn() ||
        undoGame.UpdatePresentation() != 0) ++failures;
    playerMove(undoGame, 4, 6, 4, 5);
    if (finishAnimation(undoGame) != 1) ++failures;
    undoGame.AIPlay();
    if (!undoGame.UndoTurn() || undoGame.IsAIThinking() ||
        undoGame.m_board.GetHash() != initialHash) ++failures;
    for (int x : {0, 2, 4}) {
        playerMove(undoGame, x, 6, x, 5);
        if (finishAnimation(undoGame) != 1) ++failures;
        reply(undoGame, x, 3, x, 4);
        if (finishAnimation(undoGame) != 1) ++failures;
    }
    for (int index = 0; index < 3; ++index)
        if (!undoGame.UndoTurn()) ++failures;
    if (undoGame.CanUndo() || undoGame.m_board.GetHash() != initialHash ||
        !undoGame.m_pendingExperience.empty()) ++failures;
    playerMove(undoGame, 4, 6, 4, 5);
    finishAnimation(undoGame);
    reply(undoGame, 4, 3, 4, 4);
    finishAnimation(undoGame);
    const auto beforeCapture = undoGame.m_board.GetHash();
    playerMove(undoGame, 4, 5, 4, 4);
    if (finishAnimation(undoGame) != 2 || !undoGame.UndoTurn() ||
        undoGame.m_board.GetHash() != beforeCapture ||
        undoGame.m_board.GetPiece(4, 4)->GetColor() != BLACK_P) ++failures;
    GameLogic checkGame;
    Clear(checkGame.m_board);
    checkGame.m_board.m_board[4][0] = new ChessPiece(PIECE_B_GENERAL, 4, 0);
    checkGame.m_board.m_board[4][9] = new ChessPiece(PIECE_R_GENERAL, 4, 9);
    checkGame.m_board.m_board[4][5] = new ChessPiece(PIECE_R_SOLDIER, 4, 5);
    checkGame.m_board.m_board[0][5] = new ChessPiece(PIECE_R_CHARIOT, 0, 5);
    playerMove(checkGame, 0, 5, 0, 0);
    if (finishAnimation(checkGame) != 5 || !checkGame.UndoTurn()) ++failures;
    playerMove(checkGame, 0, 5, 0, 0);
    finishAnimation(checkGame);
    reply(checkGame, 4, 0, 4, 1);
    finishAnimation(checkGame);
    if (!checkGame.UndoTurn()) ++failures;
    // 终局仍可撤销，不把已撤销的结果写入经验库。
    delete checkGame.m_board.m_board[0][5];
    checkGame.m_board.m_board[0][5] = nullptr;
    checkGame.m_board.m_board[0][0] = new ChessPiece(PIECE_R_CHARIOT, 0, 0);
    playerMove(checkGame, 0, 0, 4, 0);
    if (!checkGame.m_board.IsGameOver() || !checkGame.UndoTurn() ||
        checkGame.m_board.IsGameOver() || !checkGame.m_board.GetPiece(4, 0)) ++failures;
    const auto moveWave = SoundEffects::MakeWave(1);
    const auto captureWave = SoundEffects::MakeWave(2);
    const auto checkWave = SoundEffects::MakeWave(5);
    if (std::string(moveWave.data(), 4) != "RIFF" ||
        std::string(moveWave.data() + 8, 4) != "WAVE" ||
        moveWave.size() >= captureWave.size() || captureWave.size() >= checkWave.size())
        ++failures;
    GameLogic blackPlayer;
    blackPlayer.SetPlayerColor(BLACK_P);
    if (blackPlayer.IsPlayerTurn()) {
        std::cout << "漏洞：选择黑方后仍显示玩家回合，红方AI不会开局。\n";
        ++failures;
    }

    ChessBoard ended;
    ended.Initialize();
    ended.m_gameOver = true;
    if (ended.MovePiece(4, 6, 4, 5)) {
        std::cout << "漏洞：棋盘移动接口仍接受已结束对局中的着法。\n";
        ++failures;
    }

    GameLogic rejectedAI;
    rejectedAI.m_board.MovePiece(4, 6, 4, 5);
    rejectedAI.SetPlayerTurn(false);
    rejectedAI.m_aiPositionHash = rejectedAI.m_board.GetHash();
    std::promise<AIEngine::Move> result;
    rejectedAI.m_aiFuture = result.get_future();
    result.set_value({{0, 0}, {1, 1}});
    rejectedAI.AIPlay();
    if (!rejectedAI.m_pendingExperience.empty()) ++failures;
    if (rejectedAI.IsPlayerTurn() && rejectedAI.m_board.GetCurrentPlayer() == BLACK_P) {
        std::cout << "漏洞：拒绝非法AI着法后，界面回合与棋盘回合发生分离。\n";
        ++failures;
    }

    GameLogic staleAI;
    staleAI.m_board.MovePiece(4, 6, 4, 5);
    staleAI.SetPlayerTurn(false);
    staleAI.m_aiPositionHash = staleAI.m_board.GetHash();
    std::promise<AIEngine::Move> staleResult;
    staleAI.m_aiFuture = staleResult.get_future();
    staleResult.set_value({{4, 3}, {4, 4}});
    staleAI.m_board.MovePiece(0, 3, 0, 4);
    const auto staleHash = staleAI.m_board.GetHash();
    staleAI.AIPlay();
    if (staleAI.m_board.GetHash() != staleHash || !staleAI.IsPlayerTurn()) ++failures;

    for (int iteration = 0; iteration < 5; ++iteration) {
        GameLogic cancelled;
        cancelled.m_board.MovePiece(4, 6, 4, 5);
        cancelled.SetPlayerTurn(false);
        cancelled.AIPlay();
        const auto start = std::chrono::steady_clock::now();
        cancelled.Initialize();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= 1000 || cancelled.IsAIThinking() || !cancelled.IsPlayerTurn()) ++failures;
    }

    ChessBoard facing;
    Clear(facing);
    facing.m_board[4][0] = new ChessPiece(PIECE_B_GENERAL, 4, 0);
    facing.m_board[4][9] = new ChessPiece(PIECE_R_GENERAL, 4, 9);
    facing.m_board[4][5] = new ChessPiece(PIECE_R_CHARIOT, 4, 5);
    if (facing.MovePiece(4, 5, 3, 5)) {
        std::cout << "漏洞：允许撤掉将帅之间的遮挡子。\n";
        ++failures;
    }

    ChessBoard opening;
    opening.Initialize();
    ChessMove moves[256];
    const int count = opening.GenerateLegalMoves(moves, 256);
    if (count != 44) {
        std::cout << "异常：开局合法着数量不是44，实际为" << count << "。\n";
        ++failures;
    }
    for (int index = 0; index < count; ++index) {
        const auto hash = opening.GetHash();
        SearchUndo undo;
        opening.MakeSearchMove(moves[index], undo);
        if (opening.IsCheck(RED_P)) ++failures;
        opening.UndoSearchMove(moves[index], undo);
        if (opening.GetHash() != hash || opening.GetCurrentPlayer() != RED_P) ++failures;
        ChessBoard actual(opening);
        if (!actual.MovePiece(moves[index].fromX, moves[index].fromY,
                moves[index].toX, moves[index].toY)) ++failures;
    }
    std::cout << "审计发现数量：" << failures << "；开局走法及搜索撤销交叉检查已完成。\n";
    return failures ? 1 : 0;
}
