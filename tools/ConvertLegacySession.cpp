#include "GameLogic.h"
#include <fstream>
#include <iostream>

// 从只含棋局的旧版备份生成正常缓存，拒绝覆盖已有缓存。
int main(int argc, char** argv) {
    if (argc != 3 || std::ifstream(argv[2]).good()) return 1;
    GameLogic game;
    if (!game.RestoreLegacy(argv[1])) return 2;
    game.EnableSession(argv[2]);
    if (game.SaveFailed()) return 3;
    std::cout << "旧棋局已转换为可续下缓存，迁移盘学习已禁用。\n";
    return 0;
}
