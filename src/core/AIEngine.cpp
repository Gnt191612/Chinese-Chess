#include "AIEngine.h"
#include "ExperienceBook.h"
#include "OpeningBook.h"
#include <algorithm>
#include <climits>
#include <cstring>
#include <random>

namespace {
const int WIN_SCORE = 1000000;
const int MAX_SEARCH_DEPTH = 32;
const int QUIESCENCE_DEPTH = 6;
const int ASPIRATION_WINDOW = 50;
const int FAST_BOOK_MARGIN = 150; // 短搜索不作为布局理论裁判，只拦截明显战术损失。
const int SEARCH_VARIETY_MARGIN = 25;
const size_t TT_BYTES = 64ull * 1024ull * 1024ull;
std::atomic<int> lastOpeningFamily{0};
}

AIEngine::AIEngine(int softLimitMs, int hardLimitMs)
    : m_softLimitMs(softLimitMs), m_hardLimitMs(hardLimitMs) {
    m_transpositionTable.resize(TT_BYTES / sizeof(TTEntry));
}

void AIEngine::RequestStop() {
    m_stopRequested.store(true);
}

void AIEngine::RememberOpeningFamily(int family) {
    if (family > 0 && family <= OpeningBook::FamilyCount) lastOpeningFamily.store(family);
}

AIEngine::Move AIEngine::GetBestMove(const ChessBoard& board, const std::atomic<bool>* cancellation) {
    m_cancellation = cancellation;
    if (board.IsGameOver() || (cancellation && cancellation->load()))
        return ToPublicMove(ChessMove{});
    ChessBoard working(board);
    ChessMove rootMoves[MAX_MOVES];
    int rootCount = working.GenerateLegalMoves(rootMoves, MAX_MOVES);
    if (rootCount == 0) return ToPublicMove(ChessMove{});
    if (rootCount == 1) return ToPublicMove(rootMoves[0]);

    std::memset(m_rootExperience, 0, sizeof(m_rootExperience));
    std::memset(m_rootBook, 0, sizeof(m_rootBook));
    const auto bookCandidates = OpeningBook::Candidates(board, m_openingFamily);
    for (const auto& candidate : bookCandidates) {
        for (int index = 0; index < rootCount; ++index) {
            if (IsSameMove(candidate, rootMoves[index]))
                m_rootBook[candidate.fromY * 9 + candidate.fromX]
                    [candidate.toY * 9 + candidate.toX] = true;
        }
    }
    ChessMove experiencedBest = rootMoves[0];
    int bestExperience = INT_MIN;
    if (m_experienceBook) {
        for (int index = 0; index < rootCount; ++index) {
            const ChessMove& move = rootMoves[index];
            int from = move.fromY * 9 + move.fromX;
            int to = move.toY * 9 + move.toX;
            int bonus = m_experienceBook->GetMoveBonus(board.GetHash(), move);
            m_rootExperience[from][to] = bonus;
            if (bonus > bestExperience) {
                bestExperience = bonus;
                experiencedBest = move;
            }
        }
    }

    std::fill(m_transpositionTable.begin(), m_transpositionTable.end(), TTEntry{});
    std::memset(m_history, 0, sizeof(m_history));
    for (int ply = 0; ply < MAX_PLY; ++ply)
        m_killers[ply][0] = m_killers[ply][1] = ChessMove{};
    m_stopRequested.store(false);
    m_completedDepth.store(0);
    m_elapsedMs.store(0);
    m_lastNodeCount.store(0);
    m_nodes = 0;
    m_timedOut = false;
    m_fastBookSearch = false;
    m_startTime = std::chrono::steady_clock::now();

    ChessMove completedBest = experiencedBest;
    int previousScore = 0;
    bool fastBookAccepted = false;
    if (!bookCandidates.empty() && !board.IsCheck(board.GetCurrentPlayer())) {
        // 精确命中的谱着先做全根节点短时复核；失败后仍保留原总步时预算。
        m_fastBookSearch = true;
        std::vector<ChessMove> completedSafeMoves;
        for (int depth = 1; depth <= 3; ++depth) {
            ChessMove candidate = completedBest;
            std::vector<ChessMove> safeMoves;
            const int score = SearchRoot(working, depth, -WIN_SCORE, WIN_SCORE,
                candidate, FAST_BOOK_MARGIN, &safeMoves);
            if (m_timedOut) break;
            completedBest = candidate;
            previousScore = score;
            m_completedDepth.store(depth);
            completedSafeMoves = std::move(safeMoves);
        }
        m_fastBookSearch = false;
        if (m_completedDepth.load() >= 2 && !m_openingFamily && !completedSafeMoves.empty()) {
            // 只在复核通过的布局中按权重选一次，后续棋步不重新抽签。
            struct Choice { int family; ChessMove move; int weight; };
            std::vector<Choice> choices;
            int total = 0;
            for (int family = 1; family <= OpeningBook::FamilyCount; ++family) {
                for (const auto& move : OpeningBook::Candidates(board, family)) {
                    bool safe = false;
                    for (const auto& tested : completedSafeMoves)
                        if (IsSameMove(move, tested)) safe = true;
                    if (!safe) continue;
                    const int weight = family == lastOpeningFamily.load() ?
                        (std::max)(1, OpeningBook::Weight(family) / 8) : OpeningBook::Weight(family);
                    choices.push_back({family, move, weight}); total += weight;
                    break;
                }
            }
            if (total) {
                static thread_local std::mt19937 random(std::random_device{}());
                int ticket = std::uniform_int_distribution<int>(1, total)(random);
                for (const auto& choice : choices) {
                    ticket -= choice.weight;
                    if (ticket > 0) continue;
                    m_openingFamily = choice.family;
                    lastOpeningFamily.store(choice.family);
                    completedBest = choice.move;
                    break;
                }
            }
        }
        fastBookAccepted = m_completedDepth.load() >= 2 &&
            m_rootBook[completedBest.fromY * 9 + completedBest.fromX]
                [completedBest.toY * 9 + completedBest.toX] &&
            previousScore > -WIN_SCORE + MAX_PLY;
        m_timedOut = false;
    }
    std::vector<ChessMove> completedSearchMoves;
    for (int depth = 1; !fastBookAccepted && depth <= MAX_SEARCH_DEPTH; ++depth) {
        int alpha = depth == 1 ? -WIN_SCORE : previousScore - ASPIRATION_WINDOW;
        int beta = depth == 1 ? WIN_SCORE : previousScore + ASPIRATION_WINDOW;
        ChessMove iterationBest = completedBest;
        std::vector<ChessMove> safeMoves;
        int score = SearchRoot(working, depth, alpha, beta, iterationBest, 30, nullptr, &safeMoves);
        if (m_timedOut) break;

        if (score <= alpha || score >= beta) {
            safeMoves.clear();
            score = SearchRoot(working, depth, -WIN_SCORE, WIN_SCORE, iterationBest, 30, nullptr, &safeMoves);
            if (m_timedOut) break;
        }

        completedBest = iterationBest;
        previousScore = score;
        completedSearchMoves = std::move(safeMoves);
        m_completedDepth.store(depth);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_startTime).count();
        m_elapsedMs.store(static_cast<int>(elapsed));
        if (elapsed >= m_softLimitMs || score >= WIN_SCORE - MAX_PLY) break;
    }

    if (!fastBookAccepted && m_completedDepth.load() >= 2 &&
        !board.IsCheck(board.GetCurrentPlayer()) && completedSearchMoves.size() > 1) {
        // 只在最后完整迭代的精确评分近似着中变化；同子延续加权，原路退回降权。
        std::vector<int> weights;
        int total = 0;
        for (const auto& move : completedSearchMoves) {
            int weight = 4;
            if (move.fromX == m_planMove.toX && move.fromY == m_planMove.toY) {
                weight = move.toX == m_planMove.fromX && move.toY == m_planMove.fromY ? 1 : 6;
            }
            weights.push_back(weight); total += weight;
        }
        static thread_local std::mt19937 random(std::random_device{}());
        int ticket = std::uniform_int_distribution<int>(1, total)(random);
        for (size_t index = 0; index < completedSearchMoves.size(); ++index) {
            ticket -= weights[index];
            if (ticket <= 0) { completedBest = completedSearchMoves[index]; break; }
        }
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_startTime).count();
    m_elapsedMs.store(static_cast<int>(elapsed));
    m_lastNodeCount.store(m_nodes);
    return ToPublicMove(completedBest);
}

int AIEngine::SearchRoot(ChessBoard& board, int depth, int alpha, int beta,
    ChessMove& bestMove, int bookMargin, std::vector<ChessMove>* safeBookMoves,
    std::vector<ChessMove>* safeSearchMoves) {
    ChessMove moves[MAX_MOVES];
    int count = GenerateOrderedMoves(board, moves, 0, &bestMove);
    int best = -WIN_SCORE;
    int bookBest = -WIN_SCORE;
    ChessMove bookMove;
    bool firstMove = true;
    std::vector<std::pair<ChessMove, int>> testedBooks;
    std::vector<std::pair<ChessMove, int>> exactMoves;
    bool completedAll = true;
    for (int index = 0; index < count; ++index) {
        if (ShouldStop()) return 0;
        SearchUndo undo;
        board.MakeSearchMove(moves[index], undo);
        int score;
        const bool book = m_rootBook[moves[index].fromY * 9 + moves[index].fromX]
            [moves[index].toY * 9 + moves[index].toX];
        bool exact = book;
        if (book || (safeSearchMoves && firstMove)) {
            // 库着使用完整窗口取得可比较的分数，不把零窗口上界当成真实评分。
            score = -Search(board, depth - 1, -WIN_SCORE, WIN_SCORE, 1, true);
            firstMove = false;
            exact = true;
        } else if (safeSearchMoves) {
            score = -Search(board, depth - 1, -alpha - 1,
                -alpha + SEARCH_VARIETY_MARGIN, 1, false);
            if (!m_timedOut && score >= alpha - SEARCH_VARIETY_MARGIN) {
                score = -Search(board, depth - 1, -WIN_SCORE, WIN_SCORE, 1, true);
                exact = true;
            }
        } else if (firstMove) {
            score = -Search(board, depth - 1, -beta, -alpha, 1, true);
            firstMove = false;
        } else {
            score = -Search(board, depth - 1, -alpha - 1, -alpha, 1, false);
            if (!m_timedOut && score > alpha && score < beta)
                score = -Search(board, depth - 1, -beta, -alpha, 1, true);
        }
        board.UndoSearchMove(moves[index], undo);
        if (m_timedOut) return 0;
        if (book && score > bookBest) { bookBest = score; bookMove = moves[index]; }
        if (book && safeBookMoves) testedBooks.push_back({moves[index], score});
        if (exact && safeSearchMoves) exactMoves.push_back({moves[index], score});
        if (score > best) {
            best = score;
            bestMove = moves[index];
        }
        alpha = (std::max)(alpha, best);
        if (alpha >= beta) { completedAll = false; break; }
    }
    if (safeSearchMoves && completedAll && best > -WIN_SCORE + MAX_PLY && best < WIN_SCORE - MAX_PLY)
        for (const auto& tested : exactMoves)
            if (tested.second >= best - SEARCH_VARIETY_MARGIN) safeSearchMoves->push_back(tested.first);
    if (safeBookMoves && best > -WIN_SCORE + MAX_PLY && best < WIN_SCORE - MAX_PLY)
        for (const auto& tested : testedBooks)
            if (tested.second >= best - bookMargin) safeBookMoves->push_back(tested.first);
    if (bookMove.fromX >= 0 && bookBest >= best - bookMargin &&
        best > -WIN_SCORE + MAX_PLY && best < WIN_SCORE - MAX_PLY) bestMove = bookMove;
    return best;
}

int AIEngine::Search(ChessBoard& board, int depth, int alpha, int beta,
    int ply, bool pvNode) {
    if (ShouldStop()) return 0;
    if (board.IsGameOver()) return board.GetWinner() < 0 ? 0 :
        (board.GetWinner() == board.GetCurrentPlayer() ? WIN_SCORE - ply : -WIN_SCORE + ply);
    if (ply >= MAX_PLY - 1) return Evaluate(board, board.GetCurrentPlayer());
    if (depth <= 0) return Quiescence(board, alpha, beta, ply, QUIESCENCE_DEPTH);

    const int originalAlpha = alpha;
    const uint64_t key = board.GetHash();
    TTEntry& entry = m_transpositionTable[key % m_transpositionTable.size()];
    ChessMove preferred;
    bool hasPreferred = false;
    if (entry.key == key) {
        preferred = entry.bestMove;
        hasPreferred = preferred.fromX >= 0;
        if (!pvNode && entry.ruleContext == board.GetRuleHash() && entry.depth >= depth) {
            if (entry.bound == EXACT) return entry.score;
            if (entry.bound == LOWER && entry.score >= beta) return entry.score;
            if (entry.bound == UPPER && entry.score <= alpha) return entry.score;
        }
    }

    ChessMove moves[MAX_MOVES];
    int count = GenerateOrderedMoves(board, moves, ply, hasPreferred ? &preferred : nullptr);
    if (count == 0) return -WIN_SCORE + ply;

    int best = -WIN_SCORE;
    ChessMove bestMove = moves[0];
    bool firstMove = true;
    const int colorIndex = board.GetCurrentPlayer() == RED_P ? 0 : 1;
    for (int index = 0; index < count; ++index) {
        bool capture = IsCapture(board, moves[index]);
        SearchUndo undo;
        board.MakeSearchMove(moves[index], undo);
        int score;
        if (firstMove) {
            score = -Search(board, depth - 1, -beta, -alpha, ply + 1, pvNode);
            firstMove = false;
        } else {
            score = -Search(board, depth - 1, -alpha - 1, -alpha, ply + 1, false);
            if (!m_timedOut && score > alpha && score < beta)
                score = -Search(board, depth - 1, -beta, -alpha, ply + 1, pvNode);
        }
        board.UndoSearchMove(moves[index], undo);
        if (m_timedOut) return 0;

        if (score > best) {
            best = score;
            bestMove = moves[index];
        }
        alpha = (std::max)(alpha, best);
        if (alpha >= beta) {
            if (!capture) {
                if (!IsSameMove(moves[index], m_killers[ply][0])) {
                    m_killers[ply][1] = m_killers[ply][0];
                    m_killers[ply][0] = moves[index];
                }
                int from = moves[index].fromY * 9 + moves[index].fromX;
                int to = moves[index].toY * 9 + moves[index].toX;
                m_history[colorIndex][from][to] += depth * depth;
            }
            break;
        }
    }

    entry.key = key;
    entry.ruleContext = board.GetRuleHash();
    entry.depth = depth;
    entry.score = best;
    entry.bestMove = bestMove;
    entry.bound = best <= originalAlpha ? UPPER : (best >= beta ? LOWER : EXACT);
    return best;
}

int AIEngine::Quiescence(ChessBoard& board, int alpha, int beta,
    int ply, int remainingDepth) {
    if (ShouldStop()) return 0;
    if (board.IsGameOver()) return board.GetWinner() < 0 ? 0 :
        (board.GetWinner() == board.GetCurrentPlayer() ? WIN_SCORE - ply : -WIN_SCORE + ply);
    bool inCheck = board.IsCheck(board.GetCurrentPlayer());
    int standPat = inCheck ? -WIN_SCORE + ply : Evaluate(board, board.GetCurrentPlayer());
    if (remainingDepth <= 0 && !inCheck) return standPat;
    if (ply >= MAX_PLY - 1) {
        if (!board.HasLegalMove(board.GetCurrentPlayer())) return -WIN_SCORE + ply;
        return Evaluate(board, board.GetCurrentPlayer());
    }
    if (!inCheck) {
        if (standPat >= beta) return standPat;
        alpha = (std::max)(alpha, standPat);
    }

    ChessMove moves[MAX_MOVES];
    int count = GenerateOrderedMoves(board, moves, ply);
    if (count == 0) return -WIN_SCORE + ply;
    int best = standPat;
    for (int index = 0; index < count; ++index) {
        if (!inCheck && !IsCapture(board, moves[index])) continue;
        SearchUndo undo;
        board.MakeSearchMove(moves[index], undo);
        int score = -Quiescence(board, -beta, -alpha, ply + 1, remainingDepth - 1);
        board.UndoSearchMove(moves[index], undo);
        if (m_timedOut) return 0;
        best = (std::max)(best, score);
        alpha = (std::max)(alpha, best);
        if (alpha >= beta) break;
    }
    return best;
}

int AIEngine::Evaluate(const ChessBoard& board, PieceColor perspective) const {
    // 手工棋理权重，单位与子力分相同；不代表教师训练或实战定标结果。
    ChessPiece* pieces[9][10];
    int guards[2] = {}, elephants[2] = {}, attackUnits = 0;
    int kingX[2] = {-1, -1}, kingY[2] = {-1, -1};
    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            ChessPiece* piece = pieces[x][y] = board.GetPiece(x, y);
            if (!piece) continue;
            const int side = piece->IsRed() ? 0 : 1;
            const int type = piece->GetType() % 7;
            if (type == 0) { kingX[side] = x; kingY[side] = y; }
            if (type == 1) ++guards[side];
            if (type == 2) ++elephants[side];
            if (type == 3 || type == 5) attackUnits += 2;
            if (type == 4) attackUnits += 4;
        }
    }
    auto at = [&](int x, int y) -> ChessPiece* {
        return x >= 0 && x < 9 && y >= 0 && y < 10 ? pieces[x][y] : nullptr;
    };
    // 以当前所在路衡量兵卒作用：中路、三七路和边路不同，红黑左右对称。
    const int pawnFileBonus[9] = {-30, -18, 10, 24, 45, 24, 10, -18, -30};
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    const int horseX[8] = {2, 2, -2, -2, 1, -1, 1, -1};
    const int horseY[8] = {1, -1, 1, -1, 2, 2, -2, -2};
    int score = 0;
    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 10; ++y) {
            ChessPiece* piece = pieces[x][y];
            if (!piece) continue;
            const int side = piece->IsRed() ? 0 : 1;
            const int type = piece->GetType() % 7;
            const int forward = side == 0 ? -1 : 1;
            const int rank = side == 0 ? 9 - y : y;
            const int center = 4 - std::abs(x - 4);
            int value = PIECE_VALUES[piece->GetType()];
            if (type == 6) {
                value += pawnFileBonus[x] + (std::min)(rank, 8) * 6;
                if (rank >= 5) {
                    value += 55 + center * 8;
                    // 过河兵横向互保与后方兵卒的保护。
                    for (int neighborX : {x - 1, x + 1}) {
                        ChessPiece* neighbor = at(neighborX, y);
                        if (neighbor && neighbor->GetColor() == piece->GetColor() &&
                            neighbor->GetType() % 7 == 6) value += 10;
                    }
                }
                ChessPiece* rear = at(x, y - forward);
                if (rear && rear->GetColor() == piece->GetColor() &&
                    rear->GetType() % 7 == 6) value += 10;
                if (rank == 9) value -= 55; // 到底后失去向前推进能力。
            } else if (type == 3) {
                int mobility = 0;
                for (int index = 0; index < 8; ++index) {
                    const int hx = horseX[index], hy = horseY[index];
                    if (at(x + (std::abs(hx) == 2 ? hx / 2 : 0),
                            y + (std::abs(hy) == 2 ? hy / 2 : 0))) continue;
                    const int tx = x + hx, ty = y + hy;
                    if (tx < 0 || tx > 8 || ty < 0 || ty > 9) continue;
                    ChessPiece* target = at(tx, ty);
                    if (!target || target->GetColor() != piece->GetColor()) ++mobility;
                }
                value += mobility * 5 + center * 5 + (std::min)(rank, 6) * 3;
            } else if (type == 4 || type == 5) {
                int mobility = 0;
                for (int direction = 0; direction < 4; ++direction) {
                    bool screen = false;
                    for (int tx = x + dx[direction], ty = y + dy[direction];
                            tx >= 0 && tx < 9 && ty >= 0 && ty < 10;
                            tx += dx[direction], ty += dy[direction]) {
                        ChessPiece* target = pieces[tx][ty];
                        if (!target) { if (!screen) ++mobility; continue; }
                        if (type == 4 || screen) {
                            if (target->GetColor() != piece->GetColor()) ++mobility;
                            break;
                        }
                        screen = true;
                    }
                }
                value += mobility * (type == 4 ? 3 : 2);
                if (type == 4) value += (std::min)(rank, 6) * 2;
                else {
                    value += center * 3;
                    ChessPiece* blocker = at(x, y + forward);
                    if (blocker && blocker->GetColor() == piece->GetColor() &&
                        blocker->GetType() % 7 == 6) value -= 18;
                }
            } else if (type == 0) {
                // 进攻子越多，缺士象和将帅离开底线的风险越大；残局减弱。
                const int phase = (std::min)(attackUnits, 32);
                value -= ((2 - guards[side]) * 24 + (2 - elephants[side]) * 12 +
                    rank * 12 + std::abs(x - 4) * 8) * phase / 32;
            }
            if ((type == 3 || type == 4 || type == 5) && kingX[1 - side] >= 0) {
                const int distance = std::abs(x - kingX[1 - side]) +
                    std::abs(y - kingY[1 - side]);
                value += (std::max)(0, 6 - distance) * (type == 4 ? 4 : 3);
            }
            score += piece->GetColor() == perspective ? value : -value;
        }
    }
    PieceColor opponent = perspective == RED_P ? BLACK_P : RED_P;
    if (board.IsCheck(opponent)) score += 35;
    if (board.IsCheck(perspective)) score -= 35;
    return score;
}

int AIEngine::GenerateOrderedMoves(const ChessBoard& board, ChessMove moves[], int ply,
    const ChessMove* preferredMove) const {
    int count = board.GenerateLegalMoves(moves, MAX_MOVES);
    int scores[MAX_MOVES];
    int colorIndex = board.GetCurrentPlayer() == RED_P ? 0 : 1;
    for (int index = 0; index < count; ++index) {
        const ChessMove& move = moves[index];
        if (preferredMove && IsSameMove(move, *preferredMove)) scores[index] = 1000000;
        else if (ChessPiece* captured = board.GetPiece(move.toX, move.toY)) {
            ChessPiece* attacker = board.GetPiece(move.fromX, move.fromY);
            scores[index] = 100000 + PIECE_VALUES[captured->GetType()] * 10
                - PIECE_VALUES[attacker->GetType()];
        } else if (ply < MAX_PLY && IsSameMove(move, m_killers[ply][0])) scores[index] = 90000;
        else if (ply < MAX_PLY && IsSameMove(move, m_killers[ply][1])) scores[index] = 80000;
        else {
            int from = move.fromY * 9 + move.fromX;
            int to = move.toY * 9 + move.toX;
            scores[index] = m_history[colorIndex][from][to];
        }
        if (ply == 0) {
            int from = move.fromY * 9 + move.fromX;
            int to = move.toY * 9 + move.toX;
            scores[index] += m_rootExperience[from][to];
            if (m_rootBook[from][to]) scores[index] += 60000;
        }
    }

    for (int index = 1; index < count; ++index) {
        ChessMove move = moves[index];
        int score = scores[index];
        int position = index;
        while (position > 0 && scores[position - 1] < score) {
            moves[position] = moves[position - 1];
            scores[position] = scores[position - 1];
            --position;
        }
        moves[position] = move;
        scores[position] = score;
    }
    return count;
}

bool AIEngine::ShouldStop() {
    if (m_stopRequested.load() || (m_cancellation && m_cancellation->load())) {
        m_timedOut = true;
        return true;
    }
    ++m_nodes;
    if ((m_nodes & 31) != 0) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_startTime).count();
    m_elapsedMs.store(static_cast<int>(elapsed));
    if (elapsed >= (m_fastBookSearch ? (std::min)(m_hardLimitMs, 600) : m_hardLimitMs)) {
        m_timedOut = true;
        return true;
    }
    return false;
}

bool AIEngine::IsSameMove(const ChessMove& left, const ChessMove& right) const {
    return left.fromX == right.fromX && left.fromY == right.fromY
        && left.toX == right.toX && left.toY == right.toY;
}

bool AIEngine::IsCapture(const ChessBoard& board, const ChessMove& move) const {
    return board.GetPiece(move.toX, move.toY) != nullptr;
}

AIEngine::Move AIEngine::ToPublicMove(const ChessMove& move) const {
    return { {move.fromX, move.fromY}, {move.toX, move.toY} };
}
