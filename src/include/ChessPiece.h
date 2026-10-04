#pragma once
#include <graphics.h>

// 棋子类型枚举
enum PieceType {
    PIECE_NONE = -1,
    PIECE_R_GENERAL,   // 红帅
    PIECE_R_ADVISOR,    // 红仕
    PIECE_R_ELEPHANT,   // 红相
    PIECE_R_HORSE,      // 红马
    PIECE_R_CHARIOT,    // 红车
    PIECE_R_CANNON,     // 红炮
    PIECE_R_SOLDIER,    // 红兵
    PIECE_B_GENERAL,    // 黑将
    PIECE_B_ADVISOR,    // 黑士
    PIECE_B_ELEPHANT,   // 黑象
    PIECE_B_HORSE,      // 黑马
    PIECE_B_CHARIOT,    // 黑车
    PIECE_B_CANNON,     // 黑炮
    PIECE_B_SOLDIER     // 黑卒
};

// 棋子颜色枚举
enum PieceColor {
    RED_P,
    BLACK_P
};

// 棋子类
class ChessPiece {
public:
    ChessPiece();
    ChessPiece(PieceType type, int x, int y);

    // 绘制棋子
    void Draw(int x, int y, bool isSelected = false) const;

    // 获取棋子类型
    PieceType GetType() const { return m_type; }

    // 获取棋子颜色
    PieceColor GetColor() const { return m_color; }

    // 获取棋子坐标
    int GetX() const { return m_x; }
    int GetY() const { return m_y; }

    // 设置棋子位置
    void SetPosition(int x, int y) { m_x = x; m_y = y; }

    // 判断是否是红子
    bool IsRed() const { return m_color == RED_P; }

    // 判断是否是黑子
    bool IsBlack() const { return m_color == BLACK_P; }

    void DrawGlow(int x, int y);

private:
    PieceType m_type;    // 棋子类型
    PieceColor m_color;  // 棋子颜色
    int m_x, m_y;        // 棋子坐标
    IMAGE* m_image;      // 棋子图片
};