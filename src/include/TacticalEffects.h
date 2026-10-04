#pragma once
#include <graphics.h>
#include <chrono>
#include <cmath>

// 纯展示层，不延长走子锁定，不参与规则或缓存。
class TacticalEffects {
public:
    void Start(int event, int x, int y, int generalX, int generalY) {
        m_event = event & 6; m_x = x; m_y = y; m_generalX = generalX; m_generalY = generalY;
        m_started = std::chrono::steady_clock::now();
    }
    void Clear() { m_event = 0; }
    static double DurationSeconds(int event) { return event & 4 ? 0.85 : event & 2 ? 0.65 : 0; }
    static double RevealAt(int event, double time) {
        const double duration = DurationSeconds(event);
        if (time <= 0 || time >= duration) return 0;
        const double edge = time < duration - time ? time : duration - time;
        const double t = edge < 0.18 ? edge / 0.18 : 1;
        return t * t * (3 - 2 * t);
    }
    bool IsActive() const { return m_event && Elapsed() < DurationSeconds(m_event); }
    void Draw() const { DrawAt(Elapsed()); }
    void DrawAt(double time) const {
        if (!m_event || time < 0 || time >= DurationSeconds(m_event)) return;
        setbkmode(TRANSPARENT);
        if ((m_event & 2) && time < 0.7) {
            const double p = time / 0.7;
            setlinestyle(PS_SOLID, 3); setlinecolor(RGB(210, 151, 62));
            const int radius = 27 + static_cast<int>(35 * p);
            if (m_x - radius >= 18 && m_x + radius <= 563 && m_y - radius >= 18 && m_y + radius <= 623)
                circle(m_x, m_y, radius);
            for (int i = 0; i < 16; ++i) {
                const double angle = i * 2.399963;
                const int x = m_x + static_cast<int>(std::cos(angle) * (25 + 68 * p));
                const int y = m_y + static_cast<int>(std::sin(angle) * (25 + 68 * p) + 22 * p * p);
                if (x < 20 || x > 560 || y < 20 || y > 620) continue;
                setfillcolor(i % 2 ? RGB(231, 192, 104) : RGB(152, 67, 35));
                solidcircle(x, y, p < 0.45 ? 3 : 2);
            }
        }
        if (m_event & 4) {
            setlinestyle(PS_SOLID, 3); setlinecolor(RGB(177, 49, 37));
            circle(m_generalX, m_generalY, 29 + static_cast<int>(4 * std::sin(time * 13)));
        }
        DrawBadge(RevealAt(m_event, time), time / DurationSeconds(m_event));
        setlinestyle(PS_SOLID, 1);
    }
private:
    static DWORD BlendPixel(DWORD background, DWORD foreground, double alpha) {
        DWORD result = background & 0xff000000;
        for (int shift : {0, 8, 16}) {
            const int back = (background >> shift) & 255;
            const int front = (foreground >> shift) & 255;
            result |= static_cast<DWORD>(std::lround(back + (front - back) * alpha)) << shift;
        }
        return result;
    }
    void DrawBadge(double reveal, double progress) const {
        if (reveal <= 0) return;
        // 仅合成河界附近160×160区域，圆心不移动，不用实心牌匾遮住棋子。
        IMAGE* target = GetWorkingImage();
        IMAGE background;
        getimage(&background, 210, 240, 160, 160);
        IMAGE ink(background);
        SetWorkingImage(&ink);
        const bool check = (m_event & 4) != 0;
        const int radius = 43 + static_cast<int>(5 * reveal);
        setfillcolor(check ? RGB(169, 51, 39) : RGB(159, 103, 37));
        solidcircle(80, 80, radius);
        GdiFlush();
        const DWORD* base = GetImageBuffer(&background);
        DWORD* pixels = GetImageBuffer(&ink);
        for (int i = 0; i < 160 * 160; ++i)
            pixels[i] = BlendPixel(base[i], pixels[i], 0.14);
        setlinestyle(PS_SOLID, 2);
        setlinecolor(check ? RGB(175, 48, 35) : RGB(154, 99, 34));
        circle(80, 80, radius);
        setlinestyle(PS_SOLID, 1); setlinecolor(RGB(235, 195, 116));
        circle(80, 80, radius - 5);
        // 外侧断弧短暂扩散，主体单字只做轻微缩放与淡入淡出。
        const int wave = radius + 5 + static_cast<int>(15 * progress);
        for (int i = 0; i < 4; ++i) {
            const double angle = i * 1.57079632679 + progress * 0.35;
            arc(80 - wave, 80 - wave, 80 + wave, 80 + wave, angle, angle + 0.65);
        }
        const TCHAR* title = check ? _T("将") : _T("吃");
        setbkmode(TRANSPARENT); settextstyle(54 + static_cast<int>(4 * reveal), 0, _T("楷体"));
        settextcolor(check ? RGB(166, 37, 29) : RGB(130, 78, 25));
        outtextxy(80 - textwidth(title) / 2, 80 - textheight(title) / 2, title);
        GdiFlush();
        for (int i = 0; i < 160 * 160; ++i)
            pixels[i] = BlendPixel(base[i], pixels[i], 0.85 * reveal);
        SetWorkingImage(target);
        putimage(210, 240, &ink);
    }
    double Elapsed() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_started).count();
    }
    int m_event = 0, m_x = 0, m_y = 0, m_generalX = 0, m_generalY = 0;
    std::chrono::steady_clock::time_point m_started;
};
