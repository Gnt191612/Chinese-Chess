#include <vector>
#include <future>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <fstream>
#include <sstream>
#include <iostream>
#define private public
#include "GameLogic.h"
#undef private
#include <cassert>
#include <cstdio>

static ChessBoard CheckingBoard(bool mirror = false) {
    std::array<int, 90> pieces; pieces.fill(-1);
    pieces[0 * 9 + 5] = PIECE_B_GENERAL;
    pieces[9 * 9 + 4] = PIECE_R_GENERAL;
    pieces[8 * 9 + 4] = PIECE_B_CHARIOT;
    if (mirror) {
        std::array<int, 90> reflected; reflected.fill(-1);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 9; ++x)
                if (pieces[y * 9 + x] >= 0)
                    reflected[(9 - y) * 9 + 8 - x] = (pieces[y * 9 + x] + 7) % 14;
        pieces = reflected;
    }
    ChessBoard board;
    assert(board.ImportPosition(pieces, mirror ? BLACK_P : RED_P));
    return board;
}
static void Cycle(ChessBoard& board, bool mirror = false, bool search = false) {
    for (ChessMove move : std::vector<ChessMove>{{4,9,3,9},{4,8,3,8},{3,9,4,9},{3,8,4,8}}) {
        if (mirror) {
            move.fromX = 8 - move.fromX; move.toX = 8 - move.toX;
            move.fromY = 9 - move.fromY; move.toY = 9 - move.toY;
        }
        if (search) { SearchUndo undo; board.MakeSearchMove(move, undo); }
        else assert(board.MovePiece(move.fromX, move.fromY, move.toX, move.toY));
    }
}
static void FinishAnimation(GameLogic& game) {
    game.m_animationStartedAt -= std::chrono::milliseconds(321);
    game.UpdatePresentation();
}
static void PlayerPawn(GameLogic& game) {
    game.HandleClick(290, 410); game.HandleClick(290, 350);
}

int main(int argc, char** argv) {
    for (bool mirror : {false, true}) {
        ChessBoard board = CheckingBoard(mirror);
        Cycle(board, mirror); Cycle(board, mirror);
        assert(!board.IsGameOver()); // 连续四次将军不等于长将判负。
        std::stringstream state;
        board.WriteState(state);
        ChessBoard restored;
        assert(restored.ReadState(state));
        assert(restored.GetHash() == board.GetHash());
        assert(restored.GetRuleHash() == board.GetRuleHash());
        Cycle(restored, mirror);
        assert(restored.IsGameOver());
        assert(restored.GetEndReason() == GameEndReason::PerpetualCheck);
        assert(restored.GetWinner() == (mirror ? BLACK_P : RED_P));
        assert(!restored.MovePiece(0, 0, 0, 1));
    }
    ChessBoard searched = CheckingBoard();
    Cycle(searched, false, true); Cycle(searched, false, true);
    ChessMove first = {4,9,3,9}; SearchUndo undo;
    const auto hash = searched.GetRuleHash();
    searched.MakeSearchMove(first, undo); searched.UndoSearchMove(first, undo);
    assert(searched.GetRuleHash() == hash && !searched.IsGameOver());
    Cycle(searched, false, true);
    assert(searched.GetWinner() == RED_P);
    std::array<int, 90> quiet; quiet.fill(-1);
    quiet[4] = PIECE_B_GENERAL; quiet[9 * 9 + 4] = PIECE_R_GENERAL;
    quiet[5 * 9 + 4] = PIECE_R_SOLDIER;
    quiet[9 * 9] = PIECE_R_CHARIOT; quiet[8] = PIECE_B_CHARIOT;
    ChessBoard noCheck; assert(noCheck.ImportPosition(quiet, RED_P));
    for (int round = 0; round < 3; ++round)
        for (const auto& move : std::vector<ChessMove>{{0,9,0,8},{8,0,8,1},{0,8,0,9},{8,1,8,0}})
            assert(noCheck.MovePiece(move.fromX, move.fromY, move.toX, move.toY));
    assert(!noCheck.IsGameOver()); // 不把普通重复或长捉误判为长将。

    const std::string path = "tests/session_test.xqsession";
    uint64_t savedHash;
    {
        GameLogic game;
        game.EnableSession(path);
        PlayerPawn(game);
        savedHash = game.GetBoard().GetHash();
        assert(!game.SaveFailed());
    }
    {
        GameLogic restored;
        assert(restored.RestoreSession(path));
        assert(restored.GetBoard().GetHash() == savedHash && !restored.IsPlayerTurn());
        assert(!restored.IsAIThinking()); // 恢复棋局，不恢复旧线程或失效future。
        FinishAnimation(restored);
        restored.m_aiPositionHash = savedHash;
        std::promise<AIEngine::Move> reply;
        restored.m_aiFuture = reply.get_future(); reply.set_value({{4,3},{4,4}});
        restored.AIPlay(); FinishAnimation(restored);
        assert(restored.IsPlayerTurn() && restored.SaveSession());
    }
    {
        GameLogic restored;
        assert(restored.RestoreSession(path));
        assert(restored.IsPlayerTurn() && restored.m_pendingExperience.size() == 1);
        assert(restored.UndoTurn());
        ChessBoard initial; initial.Initialize();
        assert(restored.GetBoard().GetHash() == initial.GetHash());
        assert(restored.m_pendingExperience.empty());
        PlayerPawn(restored); FinishAnimation(restored); restored.AIPlay();
        assert(restored.IsAIThinking());
        const auto beforeDemo = restored.GetBoard().GetHash();
        const auto learned = restored.m_experienceBook.GetEntryCount();
        restored.HandleClick(290, 590); restored.HandleClick(290, 50);
        assert(restored.GetBoard().GetEndReason() == GameEndReason::DemoEnd);
        assert(restored.GetBoard().GetWinner() == -1 && restored.LearningDisabled());
        assert(!restored.IsAIThinking() && restored.GetBoard().GetHash() == beforeDemo);
        assert(restored.m_experienceBook.GetEntryCount() == learned);
        assert(restored.SaveSession());
    }
    {
        GameLogic restored;
        assert(restored.RestoreSession(path));
        assert(restored.LearningDisabled() && restored.GetBoard().GetWinner() == -1);
        assert(restored.UndoTurn() && restored.LearningDisabled());
        restored.Initialize(); assert(!restored.LearningDisabled());
    }
    {
        GameLogic unchanged;
        const auto original = unchanged.GetBoard().GetHash();
        const std::string malformed = "tests/session_broken.xqsession";
        std::ofstream output(malformed); output << "XQSESSION 1\n0 0\n"; output.close();
        assert(!unchanged.RestoreSession(malformed));
        assert(unchanged.GetBoard().GetHash() == original);
        std::remove(malformed.c_str());
        unchanged.EnableSession("tests/not_existing_directory/session.xqsession");
        assert(unchanged.SaveFailed());
    }
    std::remove(path.c_str());
    for (bool black : {false, true}) {
        std::array<int, 90> pieces; pieces.fill(-1);
        pieces[4] = PIECE_B_GENERAL; pieces[9 * 9 + 4] = PIECE_R_GENERAL;
        pieces[5 * 9 + 4] = PIECE_R_SOLDIER;
        pieces[2 * 9] = PIECE_R_CHARIOT; pieces[2 * 9 + 4] = PIECE_B_SOLDIER;
        if (black) {
            std::array<int, 90> mirror; mirror.fill(-1);
            for (int y = 0; y < 10; ++y)
                for (int x = 0; x < 9; ++x)
                    if (pieces[y * 9 + x] >= 0)
                        mirror[(9 - y) * 9 + 8 - x] = (pieces[y * 9 + x] + 7) % 14;
            pieces = mirror;
        }
        GameLogic effect;
        assert(effect.GetBoard().ImportPosition(pieces, black ? BLACK_P : RED_P));
        if (black) {
            std::promise<AIEngine::Move> move;
            effect.m_aiFuture = move.get_future();
            effect.m_aiPositionHash = effect.GetBoard().GetHash();
            move.set_value({{8,7},{4,7}});
            effect.SetPlayerTurn(false); effect.AIPlay();
        } else {
            effect.HandleClick(50, 170); effect.HandleClick(290, 170);
        }
        assert(effect.IsAnimating());
        const auto landed = effect.GetBoard().GetHash();
        assert(effect.UpdatePresentation() == 0);
        effect.m_animationStartedAt -= std::chrono::milliseconds(321);
        assert(effect.UpdatePresentation() == 6 && effect.HasTacticalEffect());
        assert(effect.UpdatePresentation() == 0 && effect.GetBoard().GetHash() == landed);
        effect.Initialize(); assert(!effect.HasTacticalEffect());
    }
    {
        const std::string oldFormat = "tests/session_v1.xqsession";
        ChessBoard board; board.Initialize();
        {
            std::ofstream output(oldFormat);
            output << "XQSESSION 1\n0 0\n";
            board.WriteState(output);
            output << "0\n0\n";
        }
        GameLogic restored;
        restored.m_openingFamily = 4;
        assert(restored.RestoreSession(oldFormat) && restored.m_openingFamily == 0);
        assert(restored.GetBoard().GetHash() == board.GetHash());
        restored.m_sessionPath.clear();
        std::remove(oldFormat.c_str());
    }
    {
        const std::string togglePath = "tests/session_toggle.xqsession";
        uint64_t oldHash;
        {
            GameLogic old;
            PlayerPawn(old);
            oldHash = old.GetBoard().GetHash();
            old.m_openingFamily = 2;
            old.EnableSession(togglePath);
        }
        {
            GameLogic two;
            const auto freshHash = two.GetBoard().GetHash();
            assert(two.PrepareSessions(togglePath));
            assert(two.HasPreviousSession() && !two.IsPreviousSession());
            assert(two.GetBoard().GetHash() == freshHash && !two.CanUndo());
            assert(two.m_openingFamily == 0);
            assert(two.SwitchSession() && two.IsPreviousSession());
            assert(two.GetBoard().GetHash() == oldHash && two.CanUndo());
            assert(two.m_openingFamily == 2);
            two.AIPlay(); assert(two.IsAIThinking());
            assert(two.SwitchSession() && !two.IsAIThinking());
            assert(two.GetBoard().GetHash() == freshHash);
            two.m_openingFamily = 3;
            two.HandleClick(170, 410); two.HandleClick(170, 350);
            const auto newHash = two.GetBoard().GetHash();
            assert(newHash != freshHash && newHash != oldHash);
            for (int i = 0; i < 10; ++i) {
                assert(two.SwitchSession());
                assert(two.GetBoard().GetHash() == (two.IsPreviousSession() ? oldHash : newHash));
                assert(two.CanUndo());
                assert(two.m_openingFamily == (two.IsPreviousSession() ? 2 : 3));
            }
            assert(two.SwitchSession() && two.UndoTurn());
            assert(two.GetBoard().GetHash() == freshHash);
            assert(two.SwitchSession() && two.GetBoard().GetHash() == newHash);
            two.HandleClick(290, 590); two.HandleClick(290, 50);
            assert(two.LearningDisabled());
            assert(two.SwitchSession() && !two.LearningDisabled());
            assert(two.SwitchSession() && two.LearningDisabled());
            assert(two.GetBoard().GetEndReason() == GameEndReason::DemoEnd);
            assert(two.UndoTurn() && two.LearningDisabled());
        }
        {
            GameLogic next;
            assert(next.PrepareSessions(togglePath));
            assert(!next.IsPreviousSession() && !next.CanUndo());
            assert(next.SwitchSession() && next.CanUndo());
            assert(next.m_openingFamily == 3);
        }
        std::remove(togglePath.c_str());
    }
    const std::string legacyPath = argc >= 2 ? argv[1] : "tests/legacy_test.xqrecover";
    if (argc < 2) {
        ChessBoard initial; initial.Initialize();
        std::ofstream output(legacyPath);
        output << "XQRECOVER 1\n0\n0 0\n";
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 9; ++x) {
                auto piece = initial.GetPiece(x, y);
                output << (piece ? int(piece->GetType()) : -1) << ' ';
            }
    }
    GameLogic recovered;
    assert(recovered.RestoreLegacy(legacyPath));
    std::ifstream original(legacyPath);
    std::string magic; int version, player, ended; size_t turns;
    original >> magic >> version >> turns >> player >> ended;
    assert(recovered.GetBoard().GetCurrentPlayer() == player && recovered.m_turnHistory.size() == turns);
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 9; ++x) {
            int type; original >> type;
            const auto piece = recovered.GetBoard().GetPiece(x, y);
            assert((piece ? int(piece->GetType()) : -1) == type);
        }
    assert(recovered.LearningDisabled());
    if (argc == 3) {
        GameLogic cached;
        assert(cached.RestoreSession(argv[2]));
        assert(cached.GetBoard().GetHash() == recovered.GetBoard().GetHash());
        assert(cached.m_turnHistory.size() == turns && cached.LearningDisabled());
        for (size_t i = 0; i < turns; ++i)
            assert(cached.m_turnHistory[i].board.GetHash() == recovered.m_turnHistory[i].board.GetHash());
    }
    if (argc < 2) std::remove(legacyPath.c_str());
    std::cout << "长将、缓存恢复、悔棋、演示隔离及旧盘迁移测试通过；保留轮数=" << turns << "。\n";
}
