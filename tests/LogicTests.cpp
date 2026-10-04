#include <vector>
#include <utility>
#define private public
#include "ChessBoard.h"
#undef private
#include "AIEngine.h"
#include "GameLogic.h"
#include "ExperienceBook.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

static void ClearBoard(ChessBoard& board) {
    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            delete board.m_board[x][y];
            board.m_board[x][y] = nullptr;
        }
    }
    board.m_gameOver = false;
    board.m_currentPlayer = RED_P;
}

static bool Contains(const std::vector<std::pair<int, int>>& moves, int x, int y) {
    return std::find(moves.begin(), moves.end(), std::make_pair(x, y)) != moves.end();
}

int main() {
    int boardX = -1, boardY = -1;
    assert(GameLogic::ScreenToBoard(50, 50, boardX, boardY));
    assert(boardX == 0 && boardY == 0);
    assert(GameLogic::ScreenToBoard(21, 21, boardX, boardY));
    assert(boardX == 0 && boardY == 0);
    assert(GameLogic::ScreenToBoard(79, 79, boardX, boardY));
    assert(boardX == 0 && boardY == 0);
    assert(GameLogic::ScreenToBoard(80, 80, boardX, boardY));
    assert(boardX == 1 && boardY == 1);
    assert(GameLogic::ScreenToBoard(530, 590, boardX, boardY));
    assert(boardX == 8 && boardY == 9);
    assert(!GameLogic::ScreenToBoard(19, 50, boardX, boardY));
    assert(!GameLogic::ScreenToBoard(560, 50, boardX, boardY));

    const char* experiencePath = "tests\\experience_test.dat";
    std::remove(experiencePath);
    ChessMove testedMove = { 4, 3, 4, 4 };
    std::vector<ExperienceSample> samples = { { 123456789ull, testedMove } };
    {
        ExperienceBook book(experiencePath);
        book.RecordGame(samples, false);
        assert(book.GetMoveBonus(123456789ull, testedMove) == 0);
        book.RecordGame(samples, false);
        assert(book.GetMoveBonus(123456789ull, testedMove) < 0);
    }
    {
        ExperienceBook reloaded(experiencePath);
        assert(reloaded.GetMoveBonus(123456789ull, testedMove) < 0);
    }
    std::remove(experiencePath);

    ChessBoard opening;
    opening.Initialize();
    uint64_t initialHash = opening.GetHash();
    ChessMove searchMoves[256];
    int searchMoveCount = opening.GenerateLegalMoves(searchMoves, 256);
    assert(searchMoveCount > 0);
    SearchUndo searchUndo;
    opening.MakeSearchMove(searchMoves[0], searchUndo);
    assert(opening.GetHash() != initialHash);
    opening.UndoSearchMove(searchMoves[0], searchUndo);
    assert(opening.GetHash() == initialHash);
    assert(opening.GetCurrentPlayer() == RED_P);
    assert(opening.GetValidMoves(1, 9).size() == 2);
    assert(opening.MovePiece(4, 6, 4, 5));
    assert(opening.GetCurrentPlayer() == BLACK_P);
    assert(!opening.MovePiece(4, 5, 4, 4));

    AIEngine ai(100, 150);
    auto aiMove = ai.GetBestMove(opening);
    assert(aiMove.first.first >= 0);
    ChessBoard afterAi(opening);
    assert(afterAi.MovePiece(aiMove.first.first, aiMove.first.second,
        aiMove.second.first, aiMove.second.second));
    assert(afterAi.GetCurrentPlayer() == RED_P);

    ChessBoard pinned;
    pinned.Initialize();
    ClearBoard(pinned);
    pinned.m_board[4][0] = new ChessPiece(PIECE_B_GENERAL, 4, 0);
    pinned.m_board[4][9] = new ChessPiece(PIECE_R_GENERAL, 4, 9);
    pinned.m_board[4][5] = new ChessPiece(PIECE_R_CHARIOT, 4, 5);
    auto pinnedMoves = pinned.GetValidMoves(4, 5);
    assert(!Contains(pinnedMoves, 3, 5));
    assert(!pinned.MovePiece(4, 5, 3, 5));
    assert(!pinned.IsCheck(RED_P));

    delete pinned.m_board[4][5];
    pinned.m_board[4][5] = nullptr;
    assert(pinned.IsCheck(RED_P));
    assert(pinned.IsCheck(BLACK_P));

    GameLogic game;
    assert(game.GetBoard().MovePiece(4, 6, 4, 5));
    game.SetPlayerTurn(false);
    game.AIPlay();
    assert(game.IsAIThinking());
    assert(game.GetAIRemainingMilliseconds() > 8000);
    assert(game.GetAIRemainingMilliseconds() <= 9000);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto cancelStart = std::chrono::steady_clock::now();
    game.Initialize();
    auto cancelElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - cancelStart).count();
    assert(cancelElapsed < 1000);
    assert(game.IsPlayerTurn());
    assert(game.GetAIRemainingMilliseconds() == 0);

    std::cout << "All chess logic tests passed.\n";
    return 0;
}
