#pragma once
#include "ChessBoard.h"
#include "AIEngine.h"
#include "ExperienceBook.h"
#include "TacticalEffects.h"
#include <future>
#include <memory>

// 游戏逻辑类
class GameLogic {
public:
    GameLogic();
    ~GameLogic();

    // 初始化游戏
    void Initialize();
    bool UndoTurn();
    bool CanUndo() const { return !m_turnHistory.empty(); }
    bool IsAnimating() const;
    bool HasTacticalEffect() const { return m_tacticalEffects.IsActive(); }
    void DrawBoard() const;
    int UpdatePresentation();
    bool SaveSession();
    bool RestoreSession(const std::string& path);
    bool RestoreLegacy(const std::string& path);
    bool PrepareSessions(const std::string& path);
    bool SwitchSession();
    bool HasPreviousSession() const { return bool(m_otherGame); }
    bool IsPreviousSession() const { return m_showingPrevious; }
    void EnableSession(const std::string& path) { m_sessionPath = path; SaveSession(); }
    bool SaveFailed() const { return m_saveFailed; }
    bool LearningDisabled() const { return m_learningDisabled; }


    // 处理点击事件
    void HandleClick(int x, int y);

    // 将窗口坐标映射到最近的棋盘交叉点
    static bool ScreenToBoard(int screenX, int screenY, int& boardX, int& boardY);

    // AI走棋
    void AIPlay();
    void CancelAI();
    bool IsAIThinking() const { return m_aiFuture.valid(); }
    int GetAISearchDepth() const { return m_aiEngine.GetCompletedDepth(); }
    int GetAIElapsedMilliseconds() const { return m_aiEngine.GetElapsedMilliseconds(); }
    int GetAIRemainingMilliseconds() const {
        if (!m_aiFuture.valid()) return 0;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_aiStartedAt).count();
        const auto remaining = m_aiEngine.GetTimeLimitMilliseconds() - elapsed;
        return remaining > 0 ? static_cast<int>(remaining) : 0;
    }

    // 获取棋盘
    ChessBoard& GetBoard() { return m_board; }

    // 判断是否是玩家回合
    bool IsPlayerTurn() const { return m_playerTurn; }

    // 设置玩家颜色
    void SetPlayerColor(PieceColor color);

    void SetPlayerTurn(bool isPlayerTurn) { m_playerTurn = isPlayerTurn; }

    void SetSelectedPosition(int x, int y);
    void GetSelectedPosition(int& x, int& y) const;

private:
    ChessBoard m_board;
    AIEngine m_aiEngine;
    ExperienceBook m_experienceBook;
    std::future<AIEngine::Move> m_aiFuture;
    std::vector<ExperienceSample> m_pendingExperience;
    struct TurnState {
        ChessBoard board;
        size_t experienceCount;
    };
    std::vector<TurnState> m_turnHistory;
    std::unique_ptr<ChessBoard> m_animationBoard;
    ChessMove m_animatedMove;
    std::chrono::steady_clock::time_point m_animationStartedAt;
    int m_moveSound = 0;
    TacticalEffects m_tacticalEffects;
    uint64_t m_aiPositionHash = 0;
    std::atomic<bool> m_aiCancelled{ false };
    std::chrono::steady_clock::time_point m_aiStartedAt;
    bool m_playerTurn;      // 是否是玩家回合
    PieceColor m_playerColor; // 玩家颜色

    // 选中的棋子位置
    int m_selectedX = -1;
    int m_selectedY = -1;

    bool ApplyMove(const ChessMove& move);
    void CommitFinishedExperience();
    bool FinishDemo(int toX, int toY);
    std::string m_sessionPath;
    bool m_saveFailed = false;
    bool m_learningDisabled = false;
    bool m_experienceCommitted = false;
    int m_openingFamily = 0;
    struct SavedGame {
        ChessBoard board;
        std::vector<TurnState> turns;
        std::vector<ExperienceSample> samples;
        bool learningDisabled = false;
        bool experienceCommitted = false;
        int openingFamily = 0;
    };
    std::unique_ptr<SavedGame> m_otherGame;
    bool m_showingPrevious = false;
};
