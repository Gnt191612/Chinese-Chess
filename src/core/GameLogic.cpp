#include "GameLogic.h"
#include <graphics.h>
#include "BoardLayout.h"
#include "MoveEffects.h"
#include <conio.h>
#include <fstream>
#include <sstream>

GameLogic::GameLogic()
    : m_playerTurn(true),  // 红方先手
    m_playerColor(RED_P), // 玩家默认控制红方
    m_selectedX(-1),
    m_selectedY(-1) {
    m_aiEngine.SetExperienceBook(&m_experienceBook);
    m_board.Initialize();
}

GameLogic::~GameLogic() {
    CancelAI();
    CommitFinishedExperience();
    SaveSession();
}

void GameLogic::Initialize() {
    CancelAI();
    CommitFinishedExperience();
    m_turnHistory.clear();
    m_animationBoard.reset();
    m_tacticalEffects.Clear();
    m_moveSound = 0;
    m_board.Initialize();
    m_pendingExperience.clear();
    m_learningDisabled = false;
    m_experienceCommitted = false;
    m_openingFamily = 0;
    m_playerTurn = m_board.GetCurrentPlayer() == m_playerColor;
    m_selectedX = -1;
    m_selectedY = -1;
    SaveSession();
}

void GameLogic::HandleClick(int x, int y) {
    if (m_board.IsGameOver() || IsAnimating()) {
        return;
    }

    int boardX, boardY;
    if (!ScreenToBoard(x, y, boardX, boardY)) {
        SetSelectedPosition(-1, -1); // 取消选择
        return;
    }

    ChessPiece* piece = m_board.GetPiece(boardX, boardY);
    if (FinishDemo(boardX, boardY)) return;
    if (!m_playerTurn) {
        if (piece && piece->GetType() == PIECE_R_GENERAL) SetSelectedPosition(boardX, boardY);
        else SetSelectedPosition(-1, -1);
        return;
    }

    // 如果已经有选中的棋子
    if (m_selectedX != -1 && m_selectedY != -1) {
        // 检查是否是取消选择
        if (boardX == m_selectedX && boardY == m_selectedY) {
            SetSelectedPosition(-1, -1); // 取消选择
            return;
        }
        
        // 尝试移动棋子
        TurnState previous = { m_board, m_pendingExperience.size() };
        if (ApplyMove({m_selectedX, m_selectedY, boardX, boardY})) {
            m_turnHistory.push_back(previous);
            m_playerTurn = false; // 切换为AI回合
            SaveSession();
        }
        SetSelectedPosition(-1, -1); // 无论移动成功与否都取消选择
    }
    // 如果没有选中的棋子，选择当前点击的棋子（如果是当前玩家的棋子）
    else if (piece && piece->GetColor() == m_playerColor) {
        SetSelectedPosition(boardX, boardY);
    }
}

bool GameLogic::ScreenToBoard(int screenX, int screenY, int& boardX, int& boardY) {
    const int origin = BoardLayout::Origin;
    const int spacing = BoardLayout::Spacing;
    const int hitRadius = spacing / 2;

    if (screenX < origin - hitRadius || screenX >= origin + 8 * spacing + hitRadius ||
        screenY < origin - hitRadius || screenY >= origin + 9 * spacing + hitRadius) {
        boardX = boardY = -1;
        return false;
    }

    boardX = (screenX - origin + hitRadius) / spacing;
    boardY = (screenY - origin + hitRadius) / spacing;
    return true;
}

void GameLogic::AIPlay() {
    if (m_playerTurn || m_board.IsGameOver() || IsAnimating()) {
        return;
    }

    if (!m_aiFuture.valid()) {
        m_aiEngine.SetOpeningFamily(m_openingFamily);
        m_aiEngine.SetPlanMove(m_pendingExperience.empty() ? ChessMove{} : m_pendingExperience.back().move);
        ChessBoard snapshot(m_board);
        m_aiPositionHash = m_board.GetHash();
        m_aiCancelled.store(false);
        m_aiStartedAt = std::chrono::steady_clock::now();
        m_aiFuture = std::async(std::launch::async, [this, snapshot]() {
            return m_aiEngine.GetBestMove(snapshot, &m_aiCancelled);
        });
        return;
    }

    if (m_aiFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready)
        return;

    auto move = m_aiFuture.get();
    m_openingFamily = m_aiEngine.GetOpeningFamily();
    if (m_board.GetHash() != m_aiPositionHash ||
        m_board.GetCurrentPlayer() == m_playerColor) {
        m_playerTurn = m_board.GetCurrentPlayer() == m_playerColor;
        return;
    }
    if (move.first.first != -1 &&
        ApplyMove({move.first.first, move.first.second,
            move.second.first, move.second.second})) {
        ChessMove learnedMove = {
            move.first.first, move.first.second, move.second.first, move.second.second
        };
        m_pendingExperience.push_back({ m_aiPositionHash, learnedMove });
    }
    m_playerTurn = m_board.GetCurrentPlayer() == m_playerColor;
    SaveSession();
}

bool GameLogic::FinishDemo(int toX, int toY) {
    ChessPiece* from = m_board.GetPiece(m_selectedX, m_selectedY);
    ChessPiece* target = m_board.GetPiece(toX, toY);
    if (!from || from->GetType() != PIECE_R_GENERAL || !target ||
        target->GetType() != PIECE_B_GENERAL) return false;
    CancelAI();
    m_tacticalEffects.Clear();
    m_turnHistory.push_back({m_board, m_pendingExperience.size()});
    m_animationBoard = std::make_unique<ChessBoard>(m_board);
    m_animatedMove = {m_selectedX, m_selectedY, toX, toY};
    m_animationStartedAt = std::chrono::steady_clock::now();
    m_moveSound = 2;
    // 只展示跨盘移动，不把违规的帅吃将落到正式棋盘中。
    m_board.EndDemo();
    m_learningDisabled = true;
    SetSelectedPosition(-1, -1);
    SaveSession();
    return true;
}

void GameLogic::CommitFinishedExperience() {
    // 保留本局样本，允许终局后悔棋；下一局或退出时才提交。
    if (!m_board.IsGameOver() || m_learningDisabled || m_experienceCommitted ||
        m_board.GetWinner() < 0) return;
    int winner = m_board.GetWinner();
    PieceColor aiColor = m_playerColor == RED_P ? BLACK_P : RED_P;
    m_experienceBook.RecordGame(m_pendingExperience, winner == aiColor);
    m_experienceCommitted = true;
}

bool GameLogic::ApplyMove(const ChessMove& move) {
    auto previous = std::make_unique<ChessBoard>(m_board);
    const bool capture = m_board.GetPiece(move.toX, move.toY) != nullptr;
    if (!m_board.MovePiece(move.fromX, move.fromY, move.toX, move.toY)) return false;
    m_tacticalEffects.Clear();
    m_animationBoard = std::move(previous);
    m_animatedMove = move;
    m_animationStartedAt = std::chrono::steady_clock::now();
    m_moveSound = (capture ? 2 : 1) |
        (m_board.IsCheck(m_board.GetCurrentPlayer()) ? 4 : 0);
    return true;
}

bool GameLogic::IsAnimating() const {
    return m_animationBoard && std::chrono::steady_clock::now() - m_animationStartedAt
        < std::chrono::milliseconds(MoveEffects::DurationMs(m_animatedMove));
}

int GameLogic::UpdatePresentation() {
    if (!m_animationBoard || IsAnimating()) return 0;
    m_animationBoard.reset();
    const int sound = m_moveSound;
    m_moveSound = 0;
    int generalX = -100, generalY = -100;
    if (sound & 4) {
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 9; ++x) {
                const auto piece = m_board.GetPiece(x, y);
                if (piece && piece->GetColor() == m_board.GetCurrentPlayer() &&
                    (piece->GetType() == PIECE_R_GENERAL || piece->GetType() == PIECE_B_GENERAL)) {
                    generalX = BoardLayout::Position(x); generalY = BoardLayout::Position(y);
                }
            }
    }
    m_tacticalEffects.Start(sound, BoardLayout::Position(m_animatedMove.toX),
        BoardLayout::Position(m_animatedMove.toY), generalX, generalY);
    return sound;
}

void GameLogic::DrawBoard() const {
    if (!IsAnimating()) {
        m_board.DrawWithSelection(m_selectedX, m_selectedY);
        m_tacticalEffects.Draw();
        return;
    }
    m_animationBoard->DrawWithSelection(-1, -1,
        m_animatedMove.fromX, m_animatedMove.fromY);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_animationStartedAt).count();
    MoveEffects::Draw(*m_animationBoard->GetPiece(m_animatedMove.fromX, m_animatedMove.fromY),
        m_animatedMove, elapsed / static_cast<double>(MoveEffects::DurationMs(m_animatedMove)));
}

bool GameLogic::UndoTurn() {
    if (m_turnHistory.empty()) return false;
    CancelAI();
    if (m_experienceCommitted) m_learningDisabled = true;
    const TurnState& previous = m_turnHistory.back();
    m_board = previous.board;
    m_pendingExperience.resize(previous.experienceCount);
    m_turnHistory.pop_back();
    m_animationBoard.reset();
    m_moveSound = 0;
    m_tacticalEffects.Clear();
    m_playerTurn = m_board.GetCurrentPlayer() == m_playerColor;
    SetSelectedPosition(-1, -1);
    m_experienceCommitted = false;
    SaveSession();
    return true;
}

bool GameLogic::PrepareSessions(const std::string& path) {
    // 上次棋局固定为另一个槽位，启动时仍显示新棋盘。
    if (!std::ifstream(path).good()) { EnableSession(path); return true; }
    auto fresh = std::make_unique<SavedGame>();
    fresh->board = m_board;
    fresh->turns = m_turnHistory;
    fresh->samples = m_pendingExperience;
    fresh->learningDisabled = m_learningDisabled;
    fresh->experienceCommitted = m_experienceCommitted;
    fresh->openingFamily = m_openingFamily;
    if (!RestoreSession(path)) return false;
    m_otherGame = std::move(fresh);
    m_showingPrevious = true;
    return SwitchSession();
}

bool GameLogic::SwitchSession() {
    if (!m_otherGame) return false;
    CancelAI();
    CommitFinishedExperience();
    std::swap(m_board, m_otherGame->board);
    m_turnHistory.swap(m_otherGame->turns);
    m_pendingExperience.swap(m_otherGame->samples);
    std::swap(m_learningDisabled, m_otherGame->learningDisabled);
    std::swap(m_experienceCommitted, m_otherGame->experienceCommitted);
    std::swap(m_openingFamily, m_otherGame->openingFamily);
    m_showingPrevious = !m_showingPrevious;
    m_playerTurn = m_board.GetCurrentPlayer() == m_playerColor;
    m_animationBoard.reset(); m_moveSound = 0;
    m_tacticalEffects.Clear();
    SetSelectedPosition(-1, -1);
    SaveSession();
    return true;
}

bool GameLogic::SaveSession() {
    if (m_sessionPath.empty()) return true;
    const std::string temporary = m_sessionPath + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << "XQSESSION 2\n" << m_learningDisabled << ' ' << m_experienceCommitted
        << ' ' << m_openingFamily << '\n';
    m_board.WriteState(output);
    output << m_pendingExperience.size() << '\n';
    for (const auto& sample : m_pendingExperience)
        output << sample.positionHash << ' ' << sample.move.fromX << ' ' << sample.move.fromY
            << ' ' << sample.move.toX << ' ' << sample.move.toY << '\n';
    output << m_turnHistory.size() << '\n';
    for (const auto& turn : m_turnHistory) {
        output << turn.experienceCount << '\n';
        turn.board.WriteState(output);
    }
    output.flush();
    const bool complete = output.good();
    output.close();
    // 原子替换，写入失败时保留上一份正式缓存；备份也不包含搜索线程对象。
    if (complete && MoveFileExA(temporary.c_str(), m_sessionPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        m_saveFailed = false;
        return true;
    }
    m_saveFailed = true;
    return false;
}

bool GameLogic::RestoreSession(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() > 128ll * 1024 * 1024) return false;
    input.seekg(0);
    std::string magic; int version, disabled, committed;
    int family = 0;
    if (!(input >> magic >> version >> disabled >> committed) || magic != "XQSESSION" ||
        (version != 1 && version != 2) || disabled < 0 || disabled > 1 || committed < 0 || committed > 1) return false;
    if (version == 2 && (!(input >> family) || family < 0 || family > 14)) return false;
    ChessBoard board;
    if (!board.ReadState(input)) return false;
    if (board.GetEndReason() == GameEndReason::DemoEnd && !disabled) return false;
    size_t count;
    if (!(input >> count) || count > 1000000) return false;
    std::vector<ExperienceSample> samples;
    for (size_t index = 0; index < count; ++index) {
        ExperienceSample sample;
        auto& move = sample.move;
        if (!(input >> sample.positionHash >> move.fromX >> move.fromY >> move.toX >> move.toY) ||
            move.fromX < 0 || move.fromX > 8 || move.toX < 0 || move.toX > 8 ||
            move.fromY < 0 || move.fromY > 9 || move.toY < 0 || move.toY > 9) return false;
        samples.push_back(sample);
    }
    if (!(input >> count) || count > 1000000) return false;
    std::vector<TurnState> turns;
    for (size_t index = 0; index < count; ++index) {
        TurnState turn;
        if (!(input >> turn.experienceCount) || turn.experienceCount > samples.size() ||
            !turn.board.ReadState(input)) return false;
        turns.push_back(turn);
    }
    input >> std::ws;
    if (!input.eof()) return false;
    CancelAI();
    m_board = board;
    m_turnHistory = std::move(turns);
    m_pendingExperience = std::move(samples);
    m_learningDisabled = disabled != 0;
    m_experienceCommitted = committed != 0;
    m_openingFamily = family;
    AIEngine::RememberOpeningFamily(family);
    m_playerColor = RED_P;
    m_playerTurn = m_board.GetCurrentPlayer() == RED_P;
    m_animationBoard.reset(); m_moveSound = 0;
    SetSelectedPosition(-1, -1);
    m_sessionPath = path; m_saveFailed = false;
    m_tacticalEffects.Clear();
    return true;
}

bool GameLogic::RestoreLegacy(const std::string& path) {
    std::ifstream input(path);
    std::string magic; int version; size_t count;
    if (!(input >> magic >> version >> count) || magic != "XQRECOVER" || version != 1 ||
        count > 1000000) return false;
    auto read = [&](ChessBoard& board) {
        int player, ended;
        if (!(input >> player >> ended) || ended < 0 || ended > 1) return false;
        std::array<int, 90> pieces;
        for (int& type : pieces) if (!(input >> type)) return false;
        return board.ImportPosition(pieces, static_cast<PieceColor>(player), ended != 0);
    };
    ChessBoard board;
    if (!read(board)) return false;
    std::vector<TurnState> turns;
    for (size_t index = 0; index < count; ++index) {
        TurnState turn;
        turn.experienceCount = 0;
        if (!read(turn.board)) return false;
        turns.push_back(turn);
    }
    input >> std::ws;
    if (!input.eof()) return false;
    CancelAI();
    m_board = board; m_turnHistory = std::move(turns);
    m_pendingExperience.clear();
    m_learningDisabled = true; m_experienceCommitted = false;
    m_tacticalEffects.Clear();
    m_openingFamily = 0;
    m_playerColor = RED_P; m_playerTurn = m_board.GetCurrentPlayer() == RED_P;
    m_animationBoard.reset(); m_moveSound = 0;
    SetSelectedPosition(-1, -1);
    return true;
}

void GameLogic::CancelAI() {
    if (!m_aiFuture.valid()) return;
    m_aiCancelled.store(true);
    m_aiEngine.RequestStop();
    m_aiFuture.wait();
    m_aiFuture.get();
    m_openingFamily = m_aiEngine.GetOpeningFamily();
}

void GameLogic::SetSelectedPosition(int x, int y) {
    m_selectedX = x;
    m_selectedY = y;
}

void GameLogic::GetSelectedPosition(int& x, int& y) const {
    x = m_selectedX;
    y = m_selectedY;
}

void GameLogic::SetPlayerColor(PieceColor color) {
    m_playerColor = color;
    Initialize();
}
