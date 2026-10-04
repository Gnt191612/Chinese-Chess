#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <iosfwd>
#include "ChessPiece.h"

struct ChessMove {
    int fromX = -1, fromY = -1, toX = -1, toY = -1;
};

enum class GameEndReason { None, NoLegalMoves, PerpetualCheck, RepetitionDraw, DemoEnd };
struct RepetitionState {
    uint64_t hash;
    int mover;
    bool check;
};

struct SearchUndo {
    ChessPiece* captured = nullptr;
    bool gameOver = false;
    uint64_t hash = 0;
    uint64_t ruleHash = 0;
    size_t repetitionSize = 0;
    GameEndReason endReason = GameEndReason::None;
    int winner = -1;
};

// 棋盘类
class ChessBoard {
public:
    ChessBoard();
    ~ChessBoard();

    // 显式定义复制构造函数和赋值操作符
    ChessBoard(const ChessBoard& other);
    ChessBoard& operator=(const ChessBoard& other);

    // 初始化棋盘
    void Initialize();

    // 绘制棋盘
    void Draw() const;

    // 获取指定位置的棋子
    ChessPiece* GetPiece(int x, int y) const;

    // 移动棋子
    bool MovePiece(int fromX, int fromY, int toX, int toY);

    // 判断游戏是否结束
    bool IsGameOver() const { return m_gameOver; }
    int GetWinner() const { return m_winner; }
    GameEndReason GetEndReason() const { return m_endReason; }
    uint64_t GetRuleHash() const { return m_ruleHash; }
    void EndDemo();
    bool ImportPosition(const std::array<int, 90>& pieces, PieceColor player, bool ended = false);
    void WriteState(std::ostream& output) const;
    bool ReadState(std::istream& input);

    // 获取当前玩家颜色
    PieceColor GetCurrentPlayer() const { return m_currentPlayer; }

    // 切换玩家
    void SwitchPlayer();

    int GenerateLegalMoves(ChessMove moves[], int capacity) const;
    void MakeSearchMove(const ChessMove& move, SearchUndo& undo);
    void UndoSearchMove(const ChessMove& move, const SearchUndo& undo);
    uint64_t GetHash() const { return m_hash; }

    // 获取所有合法移动
    std::vector<std::pair<int, int>> GetValidMoves(int x, int y) const;

    // 判断是否被将军
    bool IsCheck(PieceColor color) const;

    // 判断是否被将死
    bool IsCheckmate(PieceColor color) const;

    // 判断指定一方是否还有合法着法（象棋中困毙同样判负）
    bool HasLegalMove(PieceColor color) const;

    void DrawWithSelection(int selectedX, int selectedY, int hiddenX = -1, int hiddenY = -1) const;

private:
    // 初始化棋子位置
    void SetupPieces();

    // 判断移动是否合法
    bool IsPseudoLegal(int fromX, int fromY, int toX, int toY) const;
    bool WouldLeaveInCheck(int fromX, int fromY, int toX, int toY) const;
    void MakeMoveUnchecked(int fromX, int fromY, int toX, int toY);
    void RecomputeHash();

    // 判断两将是否照面
    bool IsGeneralFacing() const;

    // 判断棋子移动路径是否畅通
    bool IsPathClear(int fromX, int fromY, int toX, int toY) const;

    // 棋盘状态
    ChessPiece* m_board[9][10];  // 9列10行的棋盘
    bool m_gameOver;
    PieceColor m_currentPlayer;  // 当前玩家颜色
    uint64_t m_hash;
    std::vector<RepetitionState> m_repetition;
    uint64_t m_ruleHash = 0;
    GameEndReason m_endReason = GameEndReason::None;
    int m_winner = -1;
    void RecordPosition(PieceColor mover);
    void ResetRepetition();
};
