#include "ChessPiece.h"
#include <string>
#include "BoardLayout.h"

ChessPiece::ChessPiece() : m_type(PIECE_NONE), m_color(RED_P), m_x(-1), m_y(-1), m_image(nullptr) {}

ChessPiece::ChessPiece(PieceType type, int x, int y)
    : m_type(type), m_x(x), m_y(y), m_image(nullptr) {
    // 根据棋子类型确定颜色
    m_color = (type >= PIECE_R_GENERAL && type <= PIECE_R_SOLDIER) ? RED_P : BLACK_P;

    // 加载棋子图片（实际项目中应该从资源加载）
    // 这里简化处理，实际使用时需要准备对应的图片资源
}



void ChessPiece::Draw(int x, int y, bool selected) const {
    if (m_type == PIECE_NONE) return;
    const int radius = BoardLayout::PieceRadius;
    setlinestyle(PS_SOLID, 1);
    setfillcolor(RGB(144, 106, 63));
    solidcircle(x + 2, y + 4, radius + 1);
    if (selected) {
        setlinecolor(RGB(255, 225, 146));
        setlinestyle(PS_SOLID, 3);
        circle(x, y, radius + 5);
    }
    setlinestyle(PS_SOLID, 1);
    setfillcolor(RGB(231, 201, 150));
    setlinecolor(RGB(132, 92, 47));
    fillcircle(x, y, radius);
    setfillcolor(RGB(251, 235, 203));
    setlinecolor(RGB(255, 247, 225));
    fillcircle(x - 1, y - 2, radius - 3);
    setlinecolor(RGB(182, 143, 90));
    circle(x, y, radius - 6);
    settextcolor(m_color == RED_P ? RGB(164, 41, 32) : RGB(40, 33, 27));
    settextstyle(34, 0, _T("KaiTi"));
    setbkmode(TRANSPARENT);
    const TCHAR* pieceChars[] = {
        _T("\u5e05"), _T("\u4ed5"), _T("\u76f8"), _T("\u9a6c"), _T("\u8f66"), _T("\u70ae"), _T("\u5175"),
        _T("\u5c06"), _T("\u58eb"), _T("\u8c61"), _T("\u9a6c"), _T("\u8f66"), _T("\u70ae"), _T("\u5352")
    };
    const TCHAR* label = pieceChars[m_type];
    outtextxy(x - textwidth(label) / 2, y - textheight(label) / 2 - 1, label);
}
