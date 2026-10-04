#include "ChessBoard.h"
#include <algorithm>
#include "BoardLayout.h"
#include <iostream>
#include <istream>
#include <ostream>

namespace {
uint64_t Mix64(uint64_t value) {
    value += 0x9e3779b97f4a7c15ull;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ull;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebull;
    return value ^ (value >> 31);
}

uint64_t PieceHash(PieceType type, int x, int y) {
    return Mix64(static_cast<uint64_t>(type + 1) * 90ull + y * 9 + x);
}

const uint64_t SIDE_HASH = Mix64(2000);
}

ChessBoard::ChessBoard() : m_gameOver(false), m_currentPlayer(RED_P), m_hash(0) {
    // 初始化棋盘为空
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 10; ++j) {
            m_board[i][j] = nullptr;
        }
    }
}

ChessBoard::ChessBoard(const ChessBoard& other)
    : m_gameOver(other.m_gameOver), m_currentPlayer(other.m_currentPlayer), m_hash(other.m_hash),
    m_repetition(other.m_repetition), m_ruleHash(other.m_ruleHash),
    m_endReason(other.m_endReason), m_winner(other.m_winner) {
    // 深拷贝所有棋子
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 10; ++j) {
            if (other.m_board[i][j]) {
                m_board[i][j] = new ChessPiece(*other.m_board[i][j]);
            }
            else {
                m_board[i][j] = nullptr;
            }
        }
    }
}

ChessBoard& ChessBoard::operator=(const ChessBoard& other) {
    if (this != &other) {
        // 释放当前对象的资源
        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 10; ++j) {
                delete m_board[i][j];
                m_board[i][j] = nullptr;
            }
        }

        // 复制其他对象的数据
        m_gameOver = other.m_gameOver;
        m_currentPlayer = other.m_currentPlayer;
        m_hash = other.m_hash;
        m_repetition = other.m_repetition;
        m_ruleHash = other.m_ruleHash;
        m_endReason = other.m_endReason;
        m_winner = other.m_winner;

        // 深拷贝所有棋子
        for (int i = 0; i < 9; ++i) {
            for (int j = 0; j < 10; ++j) {
                if (other.m_board[i][j]) {
                    m_board[i][j] = new ChessPiece(*other.m_board[i][j]);
                }
                else {
                    m_board[i][j] = nullptr;
                }
            }
        }
    }
    return *this;
}

ChessBoard::~ChessBoard() {
    // 释放所有棋子内存
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 10; ++j) {
            if (m_board[i][j]) {
                delete m_board[i][j];
                m_board[i][j] = nullptr;
            }
        }
    }
}

void ChessBoard::Initialize() {
    // 释放所有棋子内存
    for (int i = 0; i < 9; ++i) {
        for (int j = 0; j < 10; ++j) {
            if (m_board[i][j]) {
                delete m_board[i][j];
                m_board[i][j] = nullptr;
            }
        }
    }

    m_gameOver = false;
    m_currentPlayer = RED_P;

    // 设置棋子初始位置
    SetupPieces();
    RecomputeHash();
    m_endReason = GameEndReason::None;
    m_winner = -1;
    ResetRepetition();
}

void ChessBoard::ResetRepetition() {
    m_repetition.clear();
    m_repetition.push_back({m_hash, -1, false});
    m_ruleHash = Mix64(m_hash);
}

void ChessBoard::RecordPosition(PieceColor mover) {
    const bool check = IsCheck(m_currentPlayer);
    m_repetition.push_back({m_hash, mover, check});
    m_ruleHash = Mix64(m_ruleHash ^ m_hash ^ (uint64_t(mover + 1) << 1) ^ uint64_t(check));
    if (m_gameOver) return;
    // 三个完整重复循环（同一局面含行棋方共出现四次），不能仅凭连续将军次数判负。
    size_t occurrences[4];
    int count = 0;
    for (size_t index = m_repetition.size(); index > 0 && count < 4; --index)
        if (m_repetition[index - 1].hash == m_hash) occurrences[count++] = index - 1;
    if (count < 4) return;
    const size_t length = occurrences[0] - occurrences[1];
    if (!length || occurrences[1] - occurrences[2] != length ||
        occurrences[2] - occurrences[3] != length) return;
    for (size_t index = occurrences[3] + 1; index <= occurrences[1]; ++index)
        if (m_repetition[index].hash != m_repetition[index + length].hash) return;
    bool alwaysCheck[2] = {true, true}, moved[2] = {};
    for (size_t index = occurrences[3] + 1; index <= occurrences[0]; ++index) {
        const auto& state = m_repetition[index];
        if (state.mover < 0 || state.mover > 1) return;
        moved[state.mover] = true;
        alwaysCheck[state.mover] = alwaysCheck[state.mover] && state.check;
    }
    if (!moved[0] || !moved[1]) return;
    if (!alwaysCheck[0] && !alwaysCheck[1]) return;
    m_gameOver = true;
    if (alwaysCheck[0] != alwaysCheck[1]) {
        m_endReason = GameEndReason::PerpetualCheck;
        m_winner = alwaysCheck[0] ? BLACK_P : RED_P;
    } else {
        // 双方同犯长将作和；非将军循环及复杂长捉棋例尚未实现。
        m_endReason = GameEndReason::RepetitionDraw;
        m_winner = -1;
    }
}

void ChessBoard::EndDemo() {
    m_gameOver = true;
    m_endReason = GameEndReason::DemoEnd;
    m_winner = -1;
}

bool ChessBoard::ImportPosition(const std::array<int, 90>& pieces, PieceColor player, bool ended) {
    if (player != RED_P && player != BLACK_P) return false;
    int counts[14] = {};
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 9; ++x) {
            const int type = pieces[y * 9 + x];
            if (type < -1 || type > 13) return false;
            if (type < 0) continue;
            if (++counts[type] > (type % 7 == 6 ? 5 : type % 7 == 0 ? 1 : 2)) return false;
            if (type % 7 == 0 && (x < 3 || x > 5 ||
                (type == PIECE_R_GENERAL ? y < 7 : y > 2))) return false;
        }
    if ((!ended && (counts[0] != 1 || counts[7] != 1)) || counts[0] + counts[7] < 1) return false;
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            delete m_board[x][y];
            const int type = pieces[y * 9 + x];
            m_board[x][y] = type < 0 ? nullptr : new ChessPiece(static_cast<PieceType>(type), x, y);
        }
    m_currentPlayer = player;
    m_gameOver = ended;
    m_endReason = ended ? GameEndReason::NoLegalMoves : GameEndReason::None;
    m_winner = ended ? (player == RED_P ? BLACK_P : RED_P) : -1;
    RecomputeHash(); ResetRepetition();
    return true;
}

void ChessBoard::WriteState(std::ostream& output) const {
    output << int(m_currentPlayer) << ' ' << m_gameOver << ' ' << m_winner << ' '
        << int(m_endReason) << '\n';
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 9; ++x)
            output << (m_board[x][y] ? int(m_board[x][y]->GetType()) : -1) << ' ';
        output << '\n';
    }
    output << m_repetition.size() << '\n';
    for (const auto& state : m_repetition)
        output << state.hash << ' ' << state.mover << ' ' << state.check << '\n';
}

bool ChessBoard::ReadState(std::istream& input) {
    int player, ended, winner, reason;
    if (!(input >> player >> ended >> winner >> reason) || ended < 0 || ended > 1 ||
        winner < -1 || winner > 1 || reason < 0 || reason > int(GameEndReason::DemoEnd) ||
        (!ended && (winner != -1 || reason != 0)) || (ended && reason == 0)) return false;
    if ((reason == int(GameEndReason::DemoEnd) || reason == int(GameEndReason::RepetitionDraw)) &&
        winner != -1) return false;
    if ((reason == int(GameEndReason::NoLegalMoves) || reason == int(GameEndReason::PerpetualCheck)) &&
        winner < 0) return false;
    std::array<int, 90> pieces;
    for (int& type : pieces) if (!(input >> type)) return false;
    ChessBoard loaded;
    if (!loaded.ImportPosition(pieces, static_cast<PieceColor>(player), ended != 0)) return false;
    size_t count;
    if (!(input >> count) || count == 0 || count > 1000000) return false;
    std::vector<RepetitionState> history;
    uint64_t context = 0;
    for (size_t index = 0; index < count; ++index) {
        uint64_t hash; int mover, check;
        if (!(input >> hash >> mover >> check) || check < 0 || check > 1 ||
            (index == 0 ? mover != -1 : mover < 0 || mover > 1)) return false;
        history.push_back({hash, mover, check != 0});
        context = index == 0 ? Mix64(hash) :
            Mix64(context ^ hash ^ (uint64_t(mover + 1) << 1) ^ check);
    }
    if (history.back().hash != loaded.m_hash) return false;
    loaded.m_repetition = std::move(history);
    loaded.m_ruleHash = context;
    loaded.m_endReason = static_cast<GameEndReason>(reason);
    loaded.m_winner = winner;
    *this = loaded;
    return true;
}

void ChessBoard::SwitchPlayer() {
    m_currentPlayer = (m_currentPlayer == RED_P) ? BLACK_P : RED_P;
    m_hash ^= SIDE_HASH;
}

void ChessBoard::RecomputeHash() {
    m_hash = m_currentPlayer == BLACK_P ? SIDE_HASH : 0;
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y)
            if (m_board[x][y]) m_hash ^= PieceHash(m_board[x][y]->GetType(), x, y);
}

void ChessBoard::SetupPieces() {
    // 红方棋子，下方
    m_board[0][9] = new ChessPiece(PIECE_R_CHARIOT, 0, 9);
    m_board[1][9] = new ChessPiece(PIECE_R_HORSE, 1, 9);
    m_board[2][9] = new ChessPiece(PIECE_R_ELEPHANT, 2, 9);
    m_board[3][9] = new ChessPiece(PIECE_R_ADVISOR, 3, 9);
    m_board[4][9] = new ChessPiece(PIECE_R_GENERAL, 4, 9);
    m_board[5][9] = new ChessPiece(PIECE_R_ADVISOR, 5, 9);
    m_board[6][9] = new ChessPiece(PIECE_R_ELEPHANT, 6, 9);
    m_board[7][9] = new ChessPiece(PIECE_R_HORSE, 7, 9);
    m_board[8][9] = new ChessPiece(PIECE_R_CHARIOT, 8, 9);
    m_board[1][7] = new ChessPiece(PIECE_R_CANNON, 1, 7);
    m_board[7][7] = new ChessPiece(PIECE_R_CANNON, 7, 7);
    m_board[0][6] = new ChessPiece(PIECE_R_SOLDIER, 0, 6);
    m_board[2][6] = new ChessPiece(PIECE_R_SOLDIER, 2, 6);
    m_board[4][6] = new ChessPiece(PIECE_R_SOLDIER, 4, 6);
    m_board[6][6] = new ChessPiece(PIECE_R_SOLDIER, 6, 6);
    m_board[8][6] = new ChessPiece(PIECE_R_SOLDIER, 8, 6);

    // 黑方棋子，上方
    m_board[0][0] = new ChessPiece(PIECE_B_CHARIOT, 0, 0);
    m_board[1][0] = new ChessPiece(PIECE_B_HORSE, 1, 0);
    m_board[2][0] = new ChessPiece(PIECE_B_ELEPHANT, 2, 0);
    m_board[3][0] = new ChessPiece(PIECE_B_ADVISOR, 3, 0);
    m_board[4][0] = new ChessPiece(PIECE_B_GENERAL, 4, 0);
    m_board[5][0] = new ChessPiece(PIECE_B_ADVISOR, 5, 0);
    m_board[6][0] = new ChessPiece(PIECE_B_ELEPHANT, 6, 0);
    m_board[7][0] = new ChessPiece(PIECE_B_HORSE, 7, 0);
    m_board[8][0] = new ChessPiece(PIECE_B_CHARIOT, 8, 0);
    m_board[1][2] = new ChessPiece(PIECE_B_CANNON, 1, 2);
    m_board[7][2] = new ChessPiece(PIECE_B_CANNON, 7, 2);
    m_board[0][3] = new ChessPiece(PIECE_B_SOLDIER, 0, 3);
    m_board[2][3] = new ChessPiece(PIECE_B_SOLDIER, 2, 3);
    m_board[4][3] = new ChessPiece(PIECE_B_SOLDIER, 4, 3);
    m_board[6][3] = new ChessPiece(PIECE_B_SOLDIER, 6, 3);
    m_board[8][3] = new ChessPiece(PIECE_B_SOLDIER, 8, 3);
}

bool ChessBoard::IsGeneralFacing() const {
    // 寻找红将和黑将的位置
    int redGeneralX = -1, redGeneralY = -1;
    int blackGeneralX = -1, blackGeneralY = -1;

    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            if (m_board[x][y]) {
                if (m_board[x][y]->GetType() == PIECE_R_GENERAL) {
                    redGeneralX = x;
                    redGeneralY = y;
                }
                else if (m_board[x][y]->GetType() == PIECE_B_GENERAL) {
                    blackGeneralX = x;
                    blackGeneralY = y;
                }
            }
        }
    }

    // 如果一方的将/帅已经被吃，返回false
    if (redGeneralX == -1 || blackGeneralX == -1) {
        return false;
    }

    // 两将在同一列
    if (redGeneralX == blackGeneralX) {
        // 检查中间是否有其他棋子阻挡
        for (int y = blackGeneralY + 1; y < redGeneralY; ++y) {
            if (m_board[redGeneralX][y]) {
                return false;
            }
        }
        return true;
    }

    return false;
}

bool ChessBoard::MovePiece(int fromX, int fromY, int toX, int toY) {
    if (m_gameOver) return false;
    if (fromX < 0 || fromX >= 9 || fromY < 0 || fromY >= 10 ||
        toX < 0 || toX >= 9 || toY < 0 || toY >= 10) {
        return false;
    }

    ChessPiece* fromPiece = m_board[fromX][fromY];
    if (!fromPiece) return false;

    // 检查是否是当前玩家的棋子
    if ((m_currentPlayer == RED_P && fromPiece->IsBlack()) ||
        (m_currentPlayer == BLACK_P && fromPiece->IsRed())) {
        return false;
    }

    // 检查移动是否合法
    if (!IsPseudoLegal(fromX, fromY, toX, toY) || WouldLeaveInCheck(fromX, fromY, toX, toY)) {
        return false;
    }

    MakeMoveUnchecked(fromX, fromY, toX, toY);
    SwitchPlayer();
    m_gameOver = !HasLegalMove(m_currentPlayer);
    if (m_gameOver) {
        m_endReason = GameEndReason::NoLegalMoves;
        m_winner = m_currentPlayer == RED_P ? BLACK_P : RED_P;
    }
    RecordPosition(fromPiece->GetColor());

    return true;
}

void ChessBoard::MakeMoveUnchecked(int fromX, int fromY, int toX, int toY) {
    ChessPiece* moving = m_board[fromX][fromY];
    m_hash ^= PieceHash(moving->GetType(), fromX, fromY);
    if (m_board[toX][toY]) m_hash ^= PieceHash(m_board[toX][toY]->GetType(), toX, toY);
    delete m_board[toX][toY];
    m_board[toX][toY] = moving;
    m_board[fromX][fromY] = nullptr;
    m_board[toX][toY]->SetPosition(toX, toY);
    m_hash ^= PieceHash(moving->GetType(), toX, toY);
}

int ChessBoard::GenerateLegalMoves(ChessMove moves[], int capacity) const {
    int count = 0;
    for (int fromX = 0; fromX < 9; ++fromX) {
        for (int fromY = 0; fromY < 10; ++fromY) {
            ChessPiece* piece = m_board[fromX][fromY];
            if (!piece || piece->GetColor() != m_currentPlayer) continue;
            for (int toX = 0; toX < 9; ++toX) {
                for (int toY = 0; toY < 10; ++toY) {
                    if (IsPseudoLegal(fromX, fromY, toX, toY) &&
                        !WouldLeaveInCheck(fromX, fromY, toX, toY)) {
                        if (count < capacity) moves[count] = { fromX, fromY, toX, toY };
                        ++count;
                    }
                }
            }
        }
    }
    return (std::min)(count, capacity);
}

void ChessBoard::MakeSearchMove(const ChessMove& move, SearchUndo& undo) {
    undo.captured = m_board[move.toX][move.toY];
    undo.gameOver = m_gameOver;
    undo.hash = m_hash;
    undo.ruleHash = m_ruleHash;
    undo.repetitionSize = m_repetition.size();
    undo.endReason = m_endReason;
    undo.winner = m_winner;
    ChessPiece* moving = m_board[move.fromX][move.fromY];
    m_hash ^= PieceHash(moving->GetType(), move.fromX, move.fromY);
    if (undo.captured) m_hash ^= PieceHash(undo.captured->GetType(), move.toX, move.toY);
    m_board[move.toX][move.toY] = moving;
    m_board[move.fromX][move.fromY] = nullptr;
    moving->SetPosition(move.toX, move.toY);
    m_hash ^= PieceHash(moving->GetType(), move.toX, move.toY);
    SwitchPlayer();
    m_gameOver = false;
    m_endReason = GameEndReason::None;
    m_winner = -1;
    RecordPosition(moving->GetColor());
}

void ChessBoard::UndoSearchMove(const ChessMove& move, const SearchUndo& undo) {
    ChessPiece* moving = m_board[move.toX][move.toY];
    m_board[move.fromX][move.fromY] = moving;
    m_board[move.toX][move.toY] = undo.captured;
    moving->SetPosition(move.fromX, move.fromY);
    m_currentPlayer = (m_currentPlayer == RED_P) ? BLACK_P : RED_P;
    m_gameOver = undo.gameOver;
    m_hash = undo.hash;
    m_ruleHash = undo.ruleHash;
    m_repetition.resize(undo.repetitionSize);
    m_endReason = undo.endReason;
    m_winner = undo.winner;
}

void ChessBoard::DrawWithSelection(int selectedX, int selectedY, int hiddenX, int hiddenY) const {
    using namespace BoardLayout;
    setlinestyle(PS_SOLID, 1);
    setlinecolor(RGB(91, 55, 32));
    setfillcolor(RGB(102, 65, 40));
    fillroundrect(13, 13, 569, 629, 16, 16);
    setfillcolor(RGB(220, 178, 119));
    setlinecolor(RGB(171, 124, 71));
    fillrectangle(27, 27, 555, 615);

    // 固定纹理不会闪烁，也无需额外素材文件。
    for (int y = 29; y < 615; y += 4) {
        const int shade = (y * 17) % 13;
        setlinecolor(RGB(215 + shade, 171 + shade, 112 + shade));
        line(29, y, 553, y);
    }
    setlinecolor(RGB(113, 76, 41));
    rectangle(31, 31, 551, 611);
    setlinestyle(PS_SOLID, 1);
    for (int index = 0; index < 10; ++index)
        line(Position(0), Position(index), Position(8), Position(index));
    for (int index = 0; index < 9; ++index) {
        if (index == 0 || index == 8)
            line(Position(index), Position(0), Position(index), Position(9));
        else {
            line(Position(index), Position(0), Position(index), Position(4));
            line(Position(index), Position(5), Position(index), Position(9));
        }
    }
    for (int base : {0, 7}) {
        line(Position(3), Position(base), Position(5), Position(base + 2));
        line(Position(5), Position(base), Position(3), Position(base + 2));
    }
    // 炮位、兵卒位的四角定位纹；边线兵卒位只绘制朝棋盘内侧的两角。
    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y) {
            if (!HasSetupMark(x, y)) continue;
            for (int side : {-1, 1}) {
                if ((x == 0 && side == -1) || (x == 8 && side == 1)) continue;
                for (int vertical : {-1, 1}) {
                    const int cornerX = Position(x) + side * 4;
                    const int cornerY = Position(y) + vertical * 4;
                    line(cornerX, cornerY + vertical * 8, cornerX, cornerY);
                    line(cornerX, cornerY, cornerX + side * 8, cornerY);
                }
            }
        }
    setbkmode(TRANSPARENT);
    settextcolor(RGB(101, 65, 35));
    settextstyle(34, 0, _T("楷体"));
    outtextxy(155, 305, _T("楚 河"));
    outtextxy(350, 305, _T("汉 界"));

    for (int x = 0; x < 9; ++x)
        for (int y = 0; y < 10; ++y)
            if (m_board[x][y] && (x != hiddenX || y != hiddenY))
                m_board[x][y]->Draw(Position(x), Position(y),
                    x == selectedX && y == selectedY);

    // 提示放在棋子之后，吃子目标只描边，不遮住棋字。
    ChessPiece* selected = GetPiece(selectedX, selectedY);
    if (selected && selected->GetColor() == m_currentPlayer && !m_gameOver) {
        setlinestyle(PS_SOLID, 2);
        setlinecolor(RGB(151, 70, 42));
        setfillcolor(RGB(151, 70, 42));
        for (const auto& move : GetValidMoves(selectedX, selectedY)) {
            const int x = Position(move.first), y = Position(move.second);
            if (GetPiece(move.first, move.second)) circle(x, y, PieceRadius + 3);
            else solidcircle(x, y, 5);
        }
    }
}

ChessPiece* ChessBoard::GetPiece(int x, int y) const {
    if (x < 0 || x >= 9 || y < 0 || y >= 10) {
        return nullptr;
    }
    return m_board[x][y];
}

bool ChessBoard::IsPseudoLegal(int fromX, int fromY, int toX, int toY) const {
    if (fromX < 0 || fromX >= 9 || fromY < 0 || fromY >= 10 ||
        toX < 0 || toX >= 9 || toY < 0 || toY >= 10 ||
        (fromX == toX && fromY == toY)) return false;

    ChessPiece* fromPiece = m_board[fromX][fromY];
    ChessPiece* toPiece = m_board[toX][toY];

    if (!fromPiece) return false;

    // 不能吃自己的棋子
    if (toPiece && fromPiece->GetColor() == toPiece->GetColor()) {
        return false;
    }

    // 根据棋子类型检查移动规则
    switch (fromPiece->GetType()) {
    case PIECE_R_GENERAL:
    case PIECE_B_GENERAL: {
        ChessPiece* target = m_board[toX][toY];
        if (toX == fromX && target &&
            ((fromPiece->IsRed() && target->GetType() == PIECE_B_GENERAL) ||
             (fromPiece->IsBlack() && target->GetType() == PIECE_R_GENERAL)) &&
            IsPathClear(fromX, fromY, toX, toY)) return true;

        // 将/帅只能在九宫格内移动，每次一格
        if ((fromPiece->IsRed() && (toX < 3 || toX > 5 || toY < 7 || toY > 9)) ||
            (fromPiece->IsBlack() && (toX < 3 || toX > 5 || toY < 0 || toY > 2))) {
            return false;
        }

        // 只能移动一格
        if (abs(toX - fromX) + abs(toY - fromY) != 1) return false;
        break;
    }

    case PIECE_R_ADVISOR:
    case PIECE_B_ADVISOR: {
        // 士只能在九宫格内斜走一格
        if ((fromPiece->IsRed() && (toX < 3 || toX > 5 || toY < 7 || toY > 9)) ||
            (fromPiece->IsBlack() && (toX < 3 || toX > 5 || toY < 0 || toY > 2))) {
            return false;
        }

        if (abs(toX - fromX) != 1 || abs(toY - fromY) != 1) {
            return false;
        }
        break;
    }

    case PIECE_R_ELEPHANT:
    case PIECE_B_ELEPHANT: {
        // 象/相走田，不能过河
        if (abs(toX - fromX) != 2 || abs(toY - fromY) != 2) {
            return false;
        }

        // 不能过河
        if ((fromPiece->IsRed() && toY < 5) ||
            (fromPiece->IsBlack() && toY > 4)) {
            return false;
        }

        // 检查是否被塞象眼
        if (m_board[(fromX + toX) / 2][(fromY + toY) / 2]) {
            return false;
        }
        break;
    }

    case PIECE_R_HORSE:
    case PIECE_B_HORSE: {
        // 马走日
        if (!((abs(toX - fromX) == 1 && abs(toY - fromY) == 2) ||
            (abs(toX - fromX) == 2 && abs(toY - fromY) == 1))) {
            return false;
        }

        // 检查是否被蹩马腿
        if (abs(toX - fromX) == 2) { // 横向移动
            int middleX = (fromX + toX) / 2;
            if (m_board[middleX][fromY]) {
                return false;
            }
        }
        else { // 纵向移动
            int middleY = (fromY + toY) / 2;
            if (m_board[fromX][middleY]) {
                return false;
            }
        }
        break;
    }

    case PIECE_R_CHARIOT:
    case PIECE_B_CHARIOT: {
        // 车直线移动
        if (fromX != toX && fromY != toY) {
            return false;
        }

        // 检查路径是否畅通
        if (!IsPathClear(fromX, fromY, toX, toY)) {
            return false;
        }
        break;
    }

    case PIECE_R_CANNON:
    case PIECE_B_CANNON: {
        // 炮直线移动
        if (fromX != toX && fromY != toY) {
            return false;
        }

        // 计算路径上的棋子数量
        int pieceCount = 0;
        if (fromX == toX) {
            for (int y = min(fromY, toY) + 1; y < max(fromY, toY); ++y) {
                if (m_board[fromX][y]) ++pieceCount;
            }
        }
        else {
            for (int x = min(fromX, toX) + 1; x < max(fromX, toX); ++x) {
                if (m_board[x][fromY]) ++pieceCount;
            }
        }

        // 移动时路径上不能有其他棋子
        if (!toPiece && pieceCount != 0) {
            return false;
        }

        // 吃子时路径上必须有一个棋子
        if (toPiece && pieceCount != 1) {
            return false;
        }
        break;
    }

    case PIECE_R_SOLDIER:
    case PIECE_B_SOLDIER: {
        // 兵/卒只能前进，过河后可以横向移动
        if (fromPiece->IsRed()) {
            // 红兵只能向上移动，过河后可以横向移动
            if (toY > fromY) return false; // 不能后退

            if (fromY >= 5) { // 未过河(红方在y>=5区域)
                if (toX != fromX) return false; // 不能横向移动
                if (toY != fromY - 1) return false; // 只能前进一格
            }
            else { // 已过河
                if (abs(toX - fromX) + abs(toY - fromY) != 1) return false; // 只能移动一格
                if (toY > fromY) return false; // 不能后退
            }
        }
        else {
            // 黑卒只能向下移动，过河后可以横向移动
            if (toY < fromY) return false; // 不能后退

            if (fromY <= 4) { // 未过河(黑方在y<=4区域)
                if (toX != fromX) return false; // 不能横向移动
                if (toY != fromY + 1) return false; // 只能前进一格
            }
            else { // 已过河
                if (abs(toX - fromX) + abs(toY - fromY) != 1) return false; // 只能移动一格
                if (toY < fromY) return false; // 不能后退
            }
        }
        break;
    }

    default:
        return false;
    }

    return true;
}

bool ChessBoard::IsPathClear(int fromX, int fromY, int toX, int toY) const {
    if (fromX == toX) {
        for (int y = min(fromY, toY) + 1; y < max(fromY, toY); ++y) {
            if (m_board[fromX][y]) return false;
        }
    }
    else {
        for (int x = min(fromX, toX) + 1; x < max(fromX, toX); ++x) {
            if (m_board[x][fromY]) return false;
        }
    }
    return true;
}

bool ChessBoard::IsCheck(PieceColor color) const {
    // 找到将/帅的位置
    int generalX = -1, generalY = -1;
    PieceType targetGeneral = (color == RED_P) ? PIECE_R_GENERAL : PIECE_B_GENERAL;

    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            if (m_board[x][y] && m_board[x][y]->GetType() == targetGeneral) {
                generalX = x;
                generalY = y;
                break;
            }
        }
        if (generalX != -1) break;
    }

    if (generalX == -1) return true; // 将/帅已经被吃

    // 检查是否被将军
    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            ChessPiece* piece = m_board[x][y];
            if (piece && piece->GetColor() != color) {
                if (IsPseudoLegal(x, y, generalX, generalY)) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool ChessBoard::IsCheckmate(PieceColor color) const {
    return IsCheck(color) && !HasLegalMove(color);
}

bool ChessBoard::HasLegalMove(PieceColor color) const {
    for (int fromX = 0; fromX < 9; ++fromX) {
        for (int fromY = 0; fromY < 10; ++fromY) {
            ChessPiece* piece = m_board[fromX][fromY];
            if (piece && piece->GetColor() == color) {
                auto moves = GetValidMoves(fromX, fromY);
                if (!moves.empty()) return true;
            }
        }
    }

    return false;
}

std::vector<std::pair<int, int>> ChessBoard::GetValidMoves(int x, int y) const {
    std::vector<std::pair<int, int>> moves;
    ChessPiece* piece = GetPiece(x, y);
    if (!piece) return moves;

    for (int tx = 0; tx < 9; ++tx) {
        for (int ty = 0; ty < 10; ++ty) {
            if (IsPseudoLegal(x, y, tx, ty) && !WouldLeaveInCheck(x, y, tx, ty)) {
                moves.emplace_back(tx, ty);
            }
        }
    }

    return moves;
}

bool ChessBoard::WouldLeaveInCheck(int fromX, int fromY, int toX, int toY) const {
    ChessPiece* piece = GetPiece(fromX, fromY);
    if (!piece) return true;
    PieceColor movingColor = piece->GetColor();
    ChessBoard* board = const_cast<ChessBoard*>(this);
    ChessPiece* captured = board->m_board[toX][toY];
    board->m_board[toX][toY] = piece;
    board->m_board[fromX][fromY] = nullptr;
    piece->SetPosition(toX, toY);
    bool inCheck = board->IsCheck(movingColor);
    board->m_board[fromX][fromY] = piece;
    board->m_board[toX][toY] = captured;
    piece->SetPosition(fromX, fromY);
    return inCheck;
}
