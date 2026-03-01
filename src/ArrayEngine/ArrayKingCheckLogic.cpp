#include "ChessBoard.h"

bool ChessBoard::isKingInCheck(bool isWhite) {
    //if kingpiece is in move.torow.tocol than kings in check
    int kingRow = -1, kingCol = -1;
    char kingChar = isWhite ? WKING : BKING;

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if (board[row][col] == kingChar) {
                kingRow = row;
                kingCol = col;
                break;
            }
        }
        if (kingRow != -1) break; // king found
    }

    if (kingRow == -1) return false; // king not found, should not happen in a valid game

    if (pawnAttacksKing(kingRow, kingCol, isWhite)) return true;
    if (knightAttacksKing(kingRow, kingCol, isWhite)) return true;
    if (rookOrQueenAttacksKing(kingRow, kingCol, isWhite)) return true;
    if (bishopOrQueenAttacksKing(kingRow, kingCol, isWhite)) return true;
    if (kingAttacksKing(kingRow, kingCol, isWhite)) return true;

    return false;
}

bool ChessBoard::pawnAttacksKing(int kingRow, int kingCol, bool isWhite) const {
    int dir = isWhite ? -1 : 1;
    int pawnRow = kingRow - dir;

    if (pawnRow < 0 || pawnRow > 7) return false;

    // pawn attacks from left
    if (kingCol > 0 && isEnemy(board[pawnRow][kingCol - 1], isWhite)) {
        return true;
    }

    // pawn attacks from right
    if (kingCol < 7 && isEnemy(board[pawnRow][kingCol + 1], isWhite)) {
        return true;
    }

    return false;
}

bool ChessBoard::knightAttacksKing(int kingRow, int kingCol, bool isWhite) const {
    static constexpr int dr[8] = { 1,  1, -1, -1,  2,  2, -2, -2 }; // direction of rows
    static constexpr int dc[8] = { 2, -2,  2, -2,  1, -1,  1, -1 }; // direction of columns

    char enemyKnight = isWhite ? BKNIGHT : WKNIGHT;

    for (int i = 0; i < 8; i++) {
        int knightRow = kingRow + dr[i];
        int knightCol = kingCol + dc[i];

        if (knightRow < 0 || knightRow > 7 || knightCol < 0 || knightCol > 7) continue; // out of bounds

        if (board[knightRow][knightCol] == enemyKnight) {
            return true;
        }
    }
    return false;
}

bool ChessBoard::rookOrQueenAttacksKing(int kingRow, int kingCol, bool isWhite) const {
    static constexpr int dr[4] = { 0, 0, 1, -1 }; // direction of rows
    static constexpr int dc[4] = { 1, -1, 0, 0 }; // direction of columns

    char enemyRook = isWhite ? BROOK : WROOK;
    char enemyQueen = isWhite ? BQUEEN : WQUEEN;

    for (int i = 0; i < 4; i++) {
        int rookOrQueenRow = kingRow + dr[i];
        int rookOrQueenCol = kingCol + dc[i];

        while (rookOrQueenRow >= 0 && rookOrQueenRow <= 7 && rookOrQueenCol >= 0 && rookOrQueenCol <= 7) {
            if (board[rookOrQueenRow][rookOrQueenCol] == EMPTY) {
                rookOrQueenRow += dr[i];
                rookOrQueenCol += dc[i];
                continue;
            }


            if (board[rookOrQueenRow][rookOrQueenCol] == enemyQueen || board[rookOrQueenRow][rookOrQueenCol] == enemyRook) {
                return true;
            }
            break; // blocked by another piece
        }
    }
    return false;
}

bool ChessBoard::bishopOrQueenAttacksKing(int kingRow, int kingCol, bool isWhite) const {

    static constexpr int dr[4] = { 1, 1, -1, -1 }; // direction of rows
    static constexpr int dc[4] = { 1, -1, 1, -1 }; // direction of columns

    char enemyBishop = isWhite ? BBISHOP : WBISHOP;
    char enemyQueen = isWhite ? BQUEEN : WQUEEN;

    for (int i = 0; i < 4; i++) {
        int bishopOrQueenRow = kingRow + dr[i];
        int bishopOrQueenCol = kingCol + dc[i];

        while (bishopOrQueenRow >= 0 && bishopOrQueenRow <= 7 && bishopOrQueenCol >= 0 && bishopOrQueenCol <= 7) {
            if (board[bishopOrQueenRow][bishopOrQueenCol] == EMPTY) {
                bishopOrQueenRow += dr[i];
                bishopOrQueenCol += dc[i];
                continue;
            }

            if (board[bishopOrQueenRow][bishopOrQueenCol] == enemyQueen || board[bishopOrQueenRow][bishopOrQueenCol] == enemyBishop) {
                return true;
            }
            break; // blocked by another piece

        }
    }
    return false;
}

bool ChessBoard::kingAttacksKing(int kingRow, int kingCol, bool isWhite) const {
    static constexpr int dr[8] = { 0, 0, 1, -1, 1, 1, -1, -1 }; // direction of rows
    static constexpr int dc[8] = { 1, -1, 0, 0, 1, -1, 1, -1 }; // direction of columns

    char enemyKing = isWhite ? BKING : WKING;

    for (int i = 0; i < 8; i++) {
        int enemyRow = kingRow + dr[i];
        int enemyCol = kingCol + dc[i];

        if (enemyRow < 0 || enemyRow > 7 || enemyCol < 0 || enemyCol > 7) {
            continue; // out of bounds
        }

        if (board[enemyRow][enemyCol] == enemyKing) {
            return true;
        }
    }
    return false;
}