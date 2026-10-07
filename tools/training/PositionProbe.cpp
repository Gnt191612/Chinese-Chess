#include "ChessBoard.h"
#include "AIEngine.h"
#include "LightFeatures.h"
#include <iostream>
#include <sstream>
#include <string>
#include <cctype>

// 持续读取FEN，输出红方视角基线分、共享特征及当前合法着；不启动图形窗口。
int main() {
    AIEngine engine;
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string ranks, side;
        input >> ranks >> side;
        std::array<int, 90> pieces;
        pieces.fill(-1);
        int x = 0, y = 0;
        bool valid = side == "w" || side == "b";
        const std::string types = "kabnrcp";
        for (char character : ranks) {
            if (character == '/') { if (x != 9) valid = false; x = 0; ++y; continue; }
            if (character >= '1' && character <= '9') { x += character - '0'; if (x > 9) valid = false; continue; }
            char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            if (lower == 'h') lower = 'n';
            if (lower == 'e') lower = 'b';
            auto type = types.find(lower);
            if (type == std::string::npos || x >= 9 || y >= 10) { valid = false; break; }
            pieces[y * 9 + x++] = static_cast<int>(type) + (std::isupper(static_cast<unsigned char>(character)) ? 0 : 7);
        }
        ChessBoard board;
        if (!valid || x != 9 || y != 9 || !board.ImportPosition(pieces, side == "w" ? RED_P : BLACK_P)) {
            std::cout << "ERROR" << std::endl; continue;
        }
        std::cout << engine.EvaluateForTraining(board) - LightFeatures::Correction(board, RED_P) << '|';
        for (double feature : LightFeatures::Extract(board)) std::cout << feature << ',';
        std::cout << '|';
        ChessMove moves[256];
        const int count = board.GenerateLegalMoves(moves, 256);
        for (int index = 0; index < count; ++index) {
            const auto& move = moves[index];
            std::cout << char('a' + move.fromX) << char('0' + 9 - move.fromY)
                << char('a' + move.toX) << char('0' + 9 - move.toY) << ',';
        }
        std::cout << std::endl;
    }
}
