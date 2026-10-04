#define main ChineseChessMain
#include "../src/core/main.cpp"
#undef main
#include <iostream>

int main() {
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT);
    ImmAssociateContext(GetHWnd(), HIMC{});
    GameLogic game;
    bool running = true, muted = false;
    int failures = 0;
    const auto initialHash = game.GetBoard().GetHash();
    auto press = [&](BYTE key, bool repeat = false) {
        // 小写输入对应同一个字母虚拟键，不能把字符编码冒充虚拟键码。
        if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
        // 向本测试创建的窗口发送真实键盘消息，再经 EasyX 队列读取。
        SendMessage(GetHWnd(), WM_KEYDOWN, key,
            repeat ? (LPARAM(1) << 30) | 1 : 1);
        ExMessage msg;
        bool received = false;
        while (peekmessage(&msg, EX_KEY)) {
            if (msg.message == WM_KEYDOWN && msg.vkcode == key) received = true;
            HandleShortcut(msg, game, running, muted);
        }
        if (!received) ++failures;
        SendMessage(GetHWnd(), WM_KEYUP, key, (LPARAM(3) << 30) | 1);
    };
    auto move = [&]() {
        game.HandleClick(290, 410);
        game.HandleClick(290, 350);
    };
    press('M');
    if (!muted) ++failures;
    press('M', true);
    if (!muted) ++failures;
    press('M');
    if (muted) ++failures;
    move();
    press('U', true);
    if (!game.CanUndo()) ++failures;
    press('U');
    if (game.CanUndo() || game.GetBoard().GetHash() != initialHash) ++failures;
    move();
    press('R', true);
    if (!game.CanUndo()) ++failures;
    press('R');
    if (game.CanUndo() || game.GetBoard().GetHash() != initialHash) ++failures;
    press(VK_ESCAPE, true);
    if (!running) ++failures;
    press('m');
    if (!muted) ++failures;
    press('m', true);
    if (!muted) ++failures;
    press('m');
    if (muted) ++failures;
    move();
    press('u');
    if (game.CanUndo() || game.GetBoard().GetHash() != initialHash) ++failures;
    move();
    press('r');
    if (game.CanUndo() || game.GetBoard().GetHash() != initialHash) ++failures;
    press(VK_ESCAPE);
    if (running) ++failures;
    closegraph();
    std::cout << "窗口快捷键测试失败数量：" << failures << "。\n";
    return failures ? 1 : 0;
}
