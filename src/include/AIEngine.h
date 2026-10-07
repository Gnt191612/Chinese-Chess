#pragma once
#include "ChessBoard.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

class ExperienceBook;

class AIEngine {
public:
    using Move = std::pair<std::pair<int, int>, std::pair<int, int>>;

    explicit AIEngine(int softLimitMs = 8500, int hardLimitMs = 9000);
    Move GetBestMove(const ChessBoard& board, const std::atomic<bool>* cancellation = nullptr);
    void SetExperienceBook(const ExperienceBook* book) { m_experienceBook = book; }
    void RequestStop();
    int EvaluateForTraining(const ChessBoard& board) const { return Evaluate(board, RED_P); }
    void SetOpeningFamily(int family) { m_openingFamily = family; }
    int GetOpeningFamily() const { return m_openingFamily; }
    static void RememberOpeningFamily(int family);
    void SetPlanMove(const ChessMove& move) { m_planMove = move; }
    int GetCompletedDepth() const { return m_completedDepth.load(); }
    int GetElapsedMilliseconds() const { return m_elapsedMs.load(); }
    int GetTimeLimitMilliseconds() const { return m_hardLimitMs; }
    uint64_t GetSearchedNodes() const { return m_lastNodeCount.load(); }

private:
    friend struct EvaluationTestAccess;
    enum Bound : uint8_t { EXACT, LOWER, UPPER };
    struct TTEntry {
        uint64_t key = 0;
        uint64_t ruleContext = 0;
        int score = 0;
        int depth = -1;
        Bound bound = EXACT;
        ChessMove bestMove;
    };

    static const int MAX_MOVES = 256;
    static const int MAX_PLY = 64;

    int Evaluate(const ChessBoard& board, PieceColor perspective) const;
    int Search(ChessBoard& board, int depth, int alpha, int beta, int ply, bool pvNode);
    int SearchRoot(ChessBoard& board, int depth, int alpha, int beta, ChessMove& bestMove,
        int bookMargin = 30, std::vector<ChessMove>* safeBookMoves = nullptr,
        std::vector<ChessMove>* safeSearchMoves = nullptr);
    int Quiescence(ChessBoard& board, int alpha, int beta, int ply, int remainingDepth);
    int GenerateOrderedMoves(const ChessBoard& board, ChessMove moves[], int ply,
        const ChessMove* preferredMove = nullptr) const;
    bool ShouldStop();
    bool IsSameMove(const ChessMove& left, const ChessMove& right) const;
    bool IsCapture(const ChessBoard& board, const ChessMove& move) const;
    Move ToPublicMove(const ChessMove& move) const;

    const int PIECE_VALUES[14] = {
        10000, 200, 200, 400, 900, 450, 100,
        10000, 200, 200, 400, 900, 450, 100
    };

    std::vector<TTEntry> m_transpositionTable;
    const ExperienceBook* m_experienceBook = nullptr;
    int m_rootExperience[90][90] = {};
    bool m_rootBook[90][90] = {};
    int m_history[2][90][90] = {};
    ChessMove m_killers[MAX_PLY][2];
    std::atomic<bool> m_stopRequested{ false };
    std::atomic<int> m_completedDepth{ 0 };
    std::atomic<int> m_elapsedMs{ 0 };
    std::atomic<uint64_t> m_lastNodeCount{ 0 };
    std::chrono::steady_clock::time_point m_startTime;
    int m_softLimitMs;
    int m_hardLimitMs;
    uint64_t m_nodes = 0;
    bool m_timedOut = false;
    bool m_fastBookSearch = false;
    int m_openingFamily = 0;
    ChessMove m_planMove;
    const std::atomic<bool>* m_cancellation = nullptr;
};
