#pragma once
#include <graphics.h>
#include <chrono>
#include <cmath>

class VictoryEffects {
public:
    bool Update(bool redWon, bool moving) {
        if (!redWon || moving) { m_active = false; return false; }
        if (m_active) return false;
        m_active = true;
        m_started = std::chrono::steady_clock::now();
        return true;
    }
    bool IsActive() const { return m_active; }
    void Draw() const {
        DrawAt(std::chrono::duration<double>(std::chrono::steady_clock::now() - m_started).count());
    }
    void DrawAt(double time) const {
        if (!m_active || time < 0 || time >= 3.6) return;
        setlinestyle(PS_SOLID, 2);
        // 少量确定性金色碎光，无额外线程、不遮挡右侧操作按钮。
        for (int index = 0; index < 54; ++index) {
            const double angle = index * 2.399963;
            const double radius = time * (45 + index % 7 * 8);
            const int x = 290 + static_cast<int>(std::cos(angle) * radius);
            const int y = 265 + static_cast<int>(std::sin(angle) * radius + time * time * 18);
            if (x < 35 || x > 545 || y < 35 || y > 615) continue;
            setlinecolor(index % 2 ? RGB(195, 129, 38) : RGB(245, 203, 103));
            line(x - 3, y, x + 3, y);
            line(x, y - 3, x, y + 3);
        }
        const double enter = time < 0.45 ? time / 0.45 : 1;
        const int lift = static_cast<int>(26 * (1 - enter) * (1 - enter));
        setfillcolor(RGB(116, 73, 42));
        solidroundrect(100, 246 + lift, 488, 428 + lift, 22, 22);
        for (int inset = 0; inset < 8; ++inset) {
            setfillcolor(RGB(133 + inset * 2, 37 + inset, 30 + inset));
            solidroundrect(96 + inset, 238 + lift + inset, 484 - inset, 420 + lift - inset, 20, 20);
        }
        setlinecolor(RGB(232, 187, 94));
        roundrect(105, 247 + lift, 475, 411 + lift, 16, 16);
        roundrect(111, 253 + lift, 469, 405 + lift, 12, 12);
        for (int side : {-1, 1}) {
            const int x = side < 0 ? 120 : 460;
            line(x, 267 + lift, x - side * 17, 267 + lift);
            line(x, 267 + lift, x, 280 + lift);
            line(x, 391 + lift, x - side * 17, 391 + lift);
            line(x, 391 + lift, x, 378 + lift);
        }
        setfillcolor(RGB(234, 188, 86)); solidcircle(290, 237 + lift, 43);
        setlinecolor(RGB(128, 67, 30)); circle(290, 237 + lift, 36);
        setfillcolor(RGB(150, 44, 33)); solidcircle(290, 237 + lift, 30);
        setbkmode(TRANSPARENT);
        settextstyle(43, 0, _T("楷体"));
        settextcolor(RGB(255, 235, 185));
        outtextxy(290 - textwidth(_T("胜")) / 2, 213 + lift, _T("胜"));
        settextstyle(42, 0, _T("楷体"));
        outtextxy(290 - textwidth(_T("红方获胜")) / 2, 289 + lift, _T("红方获胜"));
        settextstyle(20, 0, _T("楷体")); settextcolor(RGB(238, 196, 123));
        outtextxy(290 - textwidth(_T("妙手争先 · 棋开得胜")) / 2, 344 + lift, _T("妙手争先 · 棋开得胜"));
        settextstyle(16, 0, _T("楷体"));
        outtextxy(290 - textwidth(_T("可悔棋继续切磋")) / 2, 380 + lift, _T("可悔棋继续切磋"));
        setlinestyle(PS_SOLID, 1);
    }
private:
    bool m_active = false;
    std::chrono::steady_clock::time_point m_started;
};
