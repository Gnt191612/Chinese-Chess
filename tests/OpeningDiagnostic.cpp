#include "AIEngine.h"
#include "ExperienceBook.h"
#include "OpeningBook.h"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    for (int cannonX : {7, 1}) {
        for (bool useExperience : {false, true}) {
            ChessBoard board;
            board.Initialize();
            if (!board.MovePiece(cannonX, 7, 4, 7)) return 1;
            AIEngine ai;
            ExperienceBook experience("x64/Release/experience.dat");
            if (useExperience) ai.SetExperienceBook(&experience);
            const auto start = std::chrono::steady_clock::now();
            const auto move = ai.GetBestMove(board);
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            bool inBook = false;
            for (const auto& candidate : OpeningBook::Candidates(board))
                if (move.first.first == candidate.fromX && move.first.second == candidate.fromY &&
                    move.second.first == candidate.toX && move.second.second == candidate.toY)
                    inBook = true;
            assert(inBook && elapsed < 1500 && ai.GetCompletedDepth() >= 2);
            const bool legal = board.MovePiece(move.first.first, move.first.second,
                move.second.first, move.second.second);
            std::cout << "红炮起始列=" << cannonX << "，启用经验=" << useExperience
                << "，经验条数=" << experience.GetEntryCount()
                << "，黑方应手=(" << move.first.first << "," << move.first.second
                << ")->(" << move.second.first << "," << move.second.second
                << ")，耗时=" << elapsed << "毫秒，完整层数=" << ai.GetCompletedDepth()
                << "，节点=" << ai.GetSearchedNodes() << "，命中谱着=" << inBook
                << "，合法=" << legal << std::endl;
            if (!legal) return 1;
        }
    }
    ChessBoard central;
    central.Initialize();
    assert(central.MovePiece(7, 7, 4, 7));
    AIEngine diverse;
    int counts[5] = {};
    for (int round = 0; round < 32; ++round) {
        diverse.SetOpeningFamily(0);
        const auto move = diverse.GetBestMove(central);
        const int family = diverse.GetOpeningFamily();
        assert(family >= 1 && family <= 4);
        ++counts[family];
        bool matched = false;
        for (const auto& candidate : OpeningBook::Candidates(central, family))
            if (candidate.fromX == move.first.first && candidate.fromY == move.first.second &&
                candidate.toX == move.second.first && candidate.toY == move.second.second) matched = true;
        assert(matched && diverse.GetElapsedMilliseconds() < 1500);
        if (round < 4) {
            ChessBoard next(central);
            assert(next.MovePiece(move.first.first, move.first.second, move.second.first, move.second.second));
            assert(next.MovePiece(7, 9, 6, 7));
            const auto reply = diverse.GetBestMove(next);
            assert(diverse.GetOpeningFamily() == family);
            assert(next.MovePiece(reply.first.first, reply.first.second, reply.second.first, reply.second.second));
        }
    }
    int kinds = 0;
    for (int family = 1; family <= 4; ++family) {
        if (counts[family]) ++kinds;
        std::cout << "布局编号=" << family << "，32盘中选择次数=" << counts[family] << '\n';
    }
    assert(kinds >= 2);
    for (const auto& line : DeepOpeningLines::Lines()) {
        ChessBoard late; late.Initialize();
        for (size_t i = 0; i + 1 < line.size(); ++i) {
            const auto& move = line[i];
            assert(late.MovePiece(move.fromX, move.fromY, move.toX, move.toY));
        }
        diverse.SetOpeningFamily(1);
        const auto move = diverse.GetBestMove(late);
        const int elapsed = diverse.GetElapsedMilliseconds();
        assert(elapsed < 10000);
        bool inBook = false;
        for (const auto& candidate : OpeningBook::Candidates(late, 1))
            if (candidate.fromX == move.first.first && candidate.fromY == move.first.second &&
                candidate.toX == move.second.first && candidate.toY == move.second.second) inBook = true;
        assert(late.MovePiece(move.first.first, move.first.second, move.second.first, move.second.second));
        std::cout << "深谱末节点：第" << line.size() / 2 << "回合，耗时=" << elapsed
            << "毫秒，返回谱着=" << inBook << '\n';
    }
    for (const ChessMove first : std::vector<ChessMove>{{2,6,2,5},{6,6,6,5},
            {6,9,4,7},{7,9,6,7},{7,7,3,7}}) {
        ChessBoard board; board.Initialize();
        assert(board.MovePiece(first.fromX, first.fromY, first.toX, first.toY));
        assert(OpeningBook::Candidates(board).size() >= 2);
        int seen[15] = {}; int variants = 0;
        for (int round = 0; round < 24; ++round) {
            diverse.SetOpeningFamily(0);
            const auto move = diverse.GetBestMove(board);
            const int family = diverse.GetOpeningFamily();
            assert(family >= 5 && family <= 14);
            if (!seen[family]++) ++variants;
            ChessBoard applied(board);
            assert(applied.MovePiece(move.first.first, move.first.second, move.second.first, move.second.second));
            assert(diverse.GetElapsedMilliseconds() < 1500);
        }
        std::cout << "起手=(" << first.fromX << ',' << first.fromY << ")，24盘布局种数=" << variants << '\n';
        if (first.fromY == 6) {
            std::cout << "仙人指路应手：卒底炮=" << seen[6] << "，对兵=" << seen[7] << '\n';
            assert(seen[6] && seen[7]);
        }
        assert(variants >= 2);
    }
    return 0;
}
