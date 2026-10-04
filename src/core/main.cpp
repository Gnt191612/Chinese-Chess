#include <graphics.h>
#include "GameLogic.h"
#include "BoardLayout.h"
#include <cstring>
#include "SoundEffects.h"
#include "VictoryEffects.h"
#include <imm.h>
#include <fstream>
#pragma comment(lib, "imm32.lib")

// 游戏窗口尺寸
const int WINDOW_WIDTH = BoardLayout::WindowWidth;
const int WINDOW_HEIGHT = BoardLayout::WindowHeight;

static void Label(int centerX, int y, const TCHAR* text, int size, COLORREF color) {
    settextstyle(size, 0, _T("楷体"));
    settextcolor(color);
    outtextxy(centerX - textwidth(text) / 2, y, text);
}

static void Card(int left, int top, int right, int bottom, COLORREF color) {
    setlinestyle(PS_SOLID, 1);
    setlinecolor(RGB(183, 148, 96));
    setfillcolor(color);
    fillroundrect(left, top, right, bottom, 12, 12);
}

static void HandleShortcut(const ExMessage& msg, GameLogic& game,
        bool& running, bool& muted) {
    // 只处理图形窗口中的首次按下，忽略长按产生的重复消息。
    if (msg.message != WM_KEYDOWN || msg.prevdown) return;
    // 字母虚拟键码始终为大写编码，Shift 和 Caps Lock 不影响快捷键。
    switch (msg.vkcode) {
    case VK_ESCAPE:
        running = false;
        break;
    case 'R':
        SoundEffects::Stop();
        game.Initialize();
        break;
    case 'U':
        if (game.UndoTurn()) SoundEffects::Stop();
        break;
    case 'M':
        muted = !muted;
        if (muted) SoundEffects::Stop();
        break;
    }
}

int main(int argc, char* argv[]) {
    bool preview = false;
    std::string recovery;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--preview") == 0) preview = true;
        else if (std::strcmp(argv[index], "--recover") == 0 && index + 1 < argc)
            recovery = argv[++index];
    }
    // 禁止 Windows 对窗口进行位图缩放，确保鼠标坐标与绘制坐标一致。
    using SetProcessDPIAwareFunction = BOOL(WINAPI*)();
    auto setProcessDPIAware = reinterpret_cast<SetProcessDPIAwareFunction>(
        GetProcAddress(GetModuleHandle(_T("user32.dll")), "SetProcessDPIAware"));
    if (setProcessDPIAware) setProcessDPIAware();

    // 初始化图形窗口，开启双缓冲和控制台
    initgraph(WINDOW_WIDTH, WINDOW_HEIGHT, EW_SHOWCONSOLE);
    // 棋盘没有文字输入框，仅解除本窗口的输入法关联，防止拼音吞掉小写快捷键。
    ImmAssociateContext(GetHWnd(), HIMC{});

    // 设置窗口标题
    SetWindowText(GetHWnd(), _T("中国象棋人机对战"));

    // 初始化游戏逻辑
    GameLogic game;
    game.SetPlayerColor(RED_P); // 设置玩家颜色
    char executable[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, executable, MAX_PATH);
    const std::string executablePath(executable);
    const std::string sessionPath = executablePath.substr(0, executablePath.find_last_of("\\/") + 1)
        + "session.xqsession";
    bool restoreFailed = false;
    if (!recovery.empty()) {
        if (!game.RestoreLegacy(recovery)) { closegraph(); return 2; }
        if (!preview) game.EnableSession(sessionPath);
    } else if (!preview) {
        restoreFailed = !game.PrepareSessions(sessionPath);
    }

    // 创建内存缓冲图像
    IMAGE imgBuffer(WINDOW_WIDTH, WINDOW_HEIGHT);

    // 游戏主循环
    bool running = true;
    bool muted = false;
    VictoryEffects victory;

    while (running) {
        // 1. 处理用户输入事件
        ExMessage msg;
        while (running && peekmessage(&msg, EX_MOUSE | EX_KEY)) {
            switch (msg.message) {
            case WM_KEYDOWN:
                HandleShortcut(msg, game, running, muted);
                break;
            case WM_LBUTTONDOWN:
                // 处理鼠标左键点击事件
                if (BoardLayout::Inside(msg.x, msg.y, 600, BoardLayout::UndoTop,
                        750, BoardLayout::UndoBottom)) {
                    if (game.UndoTurn()) SoundEffects::Stop();
                }
                else if (BoardLayout::Inside(msg.x, msg.y, 765, BoardLayout::UndoTop,
                        910, BoardLayout::UndoBottom)) {
                    muted = !muted;
                    if (muted) SoundEffects::Stop();
                }
                else if (BoardLayout::Inside(msg.x, msg.y, BoardLayout::PanelLeft,
                        BoardLayout::RestartTop, 750,
                        BoardLayout::RestartBottom)) {
                    SoundEffects::Stop();
                    game.Initialize();
                }
                else if (BoardLayout::Inside(msg.x, msg.y, 765,
                        BoardLayout::SwitchTop, BoardLayout::PanelRight,
                        BoardLayout::SwitchBottom)) {
                    if (game.SwitchSession()) {
                        SoundEffects::Stop();
                        victory = VictoryEffects{};
                    }
                }
                else if (BoardLayout::Inside(msg.x, msg.y, BoardLayout::PanelLeft,
                        BoardLayout::ExitTop, BoardLayout::PanelRight,
                        BoardLayout::ExitBottom)) running = false;
                else game.HandleClick(msg.x, msg.y);
                break;

            case WM_RBUTTONDOWN:
                // 处理鼠标右键点击事件，取消选择  
                game.SetSelectedPosition(-1, -1);
                break;
            }
        }

        if (!running) break;

        const int soundEvent = game.UpdatePresentation();
        if (!muted && soundEvent) SoundEffects::Play(soundEvent);

        // 2. AI 行动逻辑
        if (!game.IsPlayerTurn() && !game.GetBoard().IsGameOver()) {
            game.AIPlay();
        }
        const bool demo = game.GetBoard().GetEndReason() == GameEndReason::DemoEnd;
        if (victory.Update(game.GetBoard().IsGameOver() &&
                (game.GetBoard().GetWinner() == RED_P || demo),
                game.IsAnimating() || game.HasTacticalEffect()) && !muted)
            SoundEffects::Play(8);

        // 3. 渲染游戏画面（使用双缓冲）
        SetWorkingImage(&imgBuffer); // 切换到内存图像

        setbkcolor(RGB(244, 235, 218));
        cleardevice();
        setbkmode(TRANSPARENT);
        game.DrawBoard();

        setfillcolor(RGB(251, 245, 233));
        setlinecolor(RGB(190, 159, 113));
        setlinestyle(PS_SOLID, 1);
        fillroundrect(585, 15, 925, 675, 18, 18);
        Label(755, 38, _T("中国象棋"), 42, RGB(70, 43, 27));
        Label(755, 91, _T("观棋理 · 品棋趣"), 18, RGB(134, 103, 66));
        Card(600, 130, 910, 192, RGB(153, 48, 36));
        Label(755, 145, _T("玩家 · 执红先行"), 25, RGB(255, 240, 210));
        Card(600, 205, 910, 267, RGB(54, 46, 37));
        Label(755, 220, _T("电脑 · 执黑后行"), 25, RGB(255, 240, 210));

        const bool ended = game.GetBoard().IsGameOver();
        const bool animating = game.IsAnimating();
        const bool thinking = !ended && !animating && !game.IsPlayerTurn();
        const int remaining = game.GetAIRemainingMilliseconds();
        Label(755, 285, animating ? _T("落子中") : ended ? _T("对局结束") :
            thinking ? _T("黑方思考中") : _T("请红方行棋"), 25, RGB(77, 52, 33));
        setlinestyle(PS_SOLID, 7);
        setlinecolor(RGB(223, 205, 175));
        circle(755, 378, 55);
        if (thinking && remaining > 0) {
            setlinecolor(RGB(185, 130, 59));
            arc(700, 323, 810, 433, 1.57079632679,
                1.57079632679 + 6.28318530718 * remaining / 9000.0);
        }
        TCHAR counter[16];
        _stprintf_s(counter, _T("%02d"), (remaining + 999) / 1000);
        Label(755, 344, thinking ? counter : ended ? _T("终") : _T("红"), 46,
            RGB(104, 64, 34));
        Label(755, 398, thinking ? _T("剩余秒数") : _T("当前回合"), 16,
            RGB(134, 103, 66));
        TCHAR detail[64];
        if (ended) {
            const auto reason = game.GetBoard().GetEndReason();
            const TCHAR* result = demo ? _T("红方获胜") :
                reason == GameEndReason::RepetitionDraw ? _T("重复局面 · 和棋") :
                reason == GameEndReason::PerpetualCheck ?
                    (game.GetBoard().GetWinner() == RED_P ? _T("黑方长将判负") : _T("红方长将判负")) :
                    (game.GetBoard().GetWinner() == RED_P ? _T("红方获胜") : _T("黑方获胜"));
            _stprintf_s(detail, _T("%s"), result);
        }
        else if (animating)
            _stprintf_s(detail, _T("落子有声 · 观棋从容"));
        else if (thinking && remaining == 0)
            _stprintf_s(detail, _T("正在完成落子"));
        else if (thinking)
            _stprintf_s(detail, _T("已完成 %d 层"), game.GetAISearchDepth());
        else
            _stprintf_s(detail, _T("祝你棋开得胜"));
        Label(755, 450, detail, 20, RGB(115, 81, 45));
        Card(600, BoardLayout::UndoTop, 750, BoardLayout::UndoBottom,
            game.CanUndo() ? RGB(189, 153, 96) : RGB(229, 212, 182));
        Label(675, 500, _T("悔棋 U/u"), 21, RGB(86, 58, 34));
        Card(765, BoardLayout::UndoTop, 910, BoardLayout::UndoBottom,
            RGB(229, 212, 182));
        Label(837, 500, muted ? _T("静音 M/m") : _T("声音 M/m"), 21, RGB(86, 58, 34));
        Card(600, BoardLayout::RestartTop, 750, BoardLayout::RestartBottom,
            RGB(153, 48, 36));
        Label(675, 554, _T("重开 R/r"), 21, RGB(255, 241, 218));
        Card(765, BoardLayout::SwitchTop, 910, BoardLayout::SwitchBottom,
            game.HasPreviousSession() ? RGB(189, 153, 96) : RGB(229, 212, 182));
        Label(837, 554, !game.HasPreviousSession() ? _T("暂无上局") :
            game.IsPreviousSession() ? _T("回到本次") : _T("恢复上次"), 21, RGB(86, 58, 34));
        Card(600, BoardLayout::ExitTop, 910, BoardLayout::ExitBottom,
            RGB(229, 212, 182));
        Label(755, 610, _T("退出游戏   Esc"), 24, RGB(86, 58, 34));
        Label(755, 654, restoreFailed ? _T("旧缓存读取失败，原文件保留") : game.SaveFailed() ?
            _T("缓存写入失败，请勿关闭棋局") : _T("自动缓存 · 无限悔棋"), 14, RGB(143, 111, 73));
        victory.Draw();

        // 4. 将内存图像复制到屏幕
        SetWorkingImage(NULL); // 切换回屏幕
        putimage(0, 0, &imgBuffer);
        if (preview) {
            saveimage(_T("tests\\ui-preview.png"), &imgBuffer);
            running = false;
        }

        // 5. 控制帧率
        static DWORD frameStart = GetTickCount();
        DWORD frameTime = GetTickCount() - frameStart;
        const DWORD frameInterval = (game.IsAnimating() || game.HasTacticalEffect()) ? 16 : 30;
        if (frameTime < frameInterval) { // 走子与战术提示期间约60帧，静止时保留原刷新预算。
            Sleep(frameInterval - frameTime);
        }
        frameStart = GetTickCount();
    }

    // 关闭图形窗口
    SoundEffects::Stop();
    closegraph();
    return 0;
}
