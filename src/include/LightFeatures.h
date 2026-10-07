#pragma once
#include "ChessBoard.h"
#include <array>
#include <algorithm>
#include <string>
#include <fstream>
#include <cmath>
#include <windows.h>

namespace LightFeatures {
constexpr size_t Count = 10;
inline std::array<double, Count> Extract(const ChessBoard& board) {
    std::array<double, Count> values{};
    for (int y = 0; y < 10; ++y) for (int x = 0; x < 9; ++x) {
        const auto* piece = board.GetPiece(x, y);
        if (!piece) continue;
        const int type = piece->GetType() % 7;
        const double sign = piece->IsRed() ? 1.0 : -1.0;
        if (type >= 1) values[type - 1] += sign;
        if (type == 6) {
            const int rank = piece->IsRed() ? 9 - y : y;
            values[6] += x == 4 ? sign : 0;
            values[7] += x == 2 || x == 6 ? sign : 0;
            values[8] += (std::min)(rank, 8) * sign;
            values[9] += rank >= 5 ? sign : 0;
        }
    }
    return values;
}

inline std::array<double, Count> Load() {
    std::array<double, Count> values{};
    char executable[MAX_PATH]{};
    GetModuleFileNameA(nullptr, executable, MAX_PATH);
    const std::string path(executable);
    std::ifstream input(path.substr(0, path.find_last_of("\\/") + 1) + "evaluation.xqweights");
    std::string version;
    if (!(input >> version) || version != "XQEV1") return {};
    for (double& value : values)
        if (!(input >> value) || !std::isfinite(value) || std::abs(value) > 50) return {};
    if (input >> version) return {};
    return values;
}

inline int Correction(const ChessBoard& board, PieceColor perspective) {
    // 只在启动后首次求值时加载；训练参数可选，缺失时保持原评估。
    static const auto weights = Load();
    static const bool active = [] { for (double value : weights) if (value != 0) return true; return false; }();
    if (!active) return 0;
    const auto features = Extract(board);
    double value = 0;
    for (size_t index = 0; index < Count; ++index) value += weights[index] * features[index];
    return static_cast<int>(std::round(perspective == RED_P ? value : -value));
}
}
