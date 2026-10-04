#include "AIEngine.h"
#include <cassert>
#include <chrono>
#include <iostream>

int main() {
    ChessBoard board;
    board.Initialize();
    assert(board.MovePiece(4, 6, 4, 5));

    AIEngine ai;
    auto start = std::chrono::steady_clock::now();
    auto move = ai.GetBestMove(board);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    assert(move.first.first >= 0);
    assert(elapsed < 10000);
    std::cout << "Timed search: " << elapsed << " ms, completed depth: "
        << ai.GetCompletedDepth() << ", nodes: " << ai.GetSearchedNodes() << "\n";
    return 0;
}
