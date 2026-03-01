#include <iostream>
#include <vector>
#include "ChessBoard.h"

std::vector<Move> ChessBoard::getPawnMoves(int row, int col, bool isWhite) {
    std::vector<Move> moves;
    int dir = isWhite ? -1 : 1;

    if (row + dir >= 0 && row + dir <= 7) {

        // Single move forward
        if (board[row + dir][col] == EMPTY) {
            moves.push_back(Move(row, col, row + dir, col));
        }

        // Double move from starting rank
        if ((isWhite && row == 6) || (!isWhite && row == 1)) {
            if (board[row + dir][col] == EMPTY && board[row + 2 * dir][col] == EMPTY) {
                moves.push_back(Move(row, col, row + 2 * dir, col));
            }
        }

        // Capture left
        if (col > 0 && isEnemy(board[row + dir][col - 1], isWhite)) {
            moves.push_back(Move(row, col, row + dir, col - 1));
        }

        // Capture right
        if (col < 7 && isEnemy(board[row + dir][col + 1], isWhite)) {
            moves.push_back(Move(row, col, row + dir, col + 1));
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getKnightMoves(int row, int col, bool isWhite) {

    std::vector<Move> moves;
    static constexpr int dr[8] = { 1,  1, -1, -1,  2,  2, -2, -2 }; // direction of rows
    static constexpr int dc[8] = { 2, -2,  2, -2,  1, -1,  1, -1 }; // direction of columns

    // Generating all possible knight moves
    for (int i = 0; i < 8; i++) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        if (nextRow < 0 || nextRow > 7 || nextCol < 0 || nextCol > 7) {
            continue; // out of bounds
        }
        if (board[nextRow][nextCol] == EMPTY || isEnemy(board[nextRow][nextCol], isWhite)) {
            moves.push_back(Move(row, col, nextRow, nextCol));
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getBishopMoves(int row, int col, bool isWhite) {

    std::vector<Move> moves;
    static constexpr int dr[4] = { 1, 1, -1, -1 }; // direction of rows
    static constexpr int dc[4] = { 1, -1, 1, -1 }; // direction of columns

    for (int i = 0; i < 4; i++) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        while (nextRow >= 0 && nextRow <= 7 && nextCol >= 0 && nextCol <= 7) {
            if (board[nextRow][nextCol] == EMPTY) {
                moves.push_back(Move(row, col, nextRow, nextCol));
            }
            else {
                if (isEnemy(board[nextRow][nextCol], isWhite)) {
                    moves.push_back(Move(row, col, nextRow, nextCol));
                }
                break; // cannot move past another piece
            }
            nextRow += dr[i];
            nextCol += dc[i];
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getRookMoves(int row, int col, bool isWhite) {

    std::vector<Move> moves;
    static constexpr int dr[4] = { 0, 0, 1, -1 }; // direction of rows
    static constexpr int dc[4] = { 1, -1, 0, 0 }; // direction of columns

    for (int i = 0; i < 4; i++) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        while (nextRow >= 0 && nextRow <= 7 && nextCol >= 0 && nextCol <= 7) {
            if (board[nextRow][nextCol] == EMPTY) {
                moves.push_back(Move(row, col, nextRow, nextCol));
            }
            else {
                if (isEnemy(board[nextRow][nextCol], isWhite)) {
                    moves.push_back(Move(row, col, nextRow, nextCol));
                }
                break; // cannot move past another piece
            }
            nextRow += dr[i];
            nextCol += dc[i];
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getQueenMoves(int row, int col, bool isWhite) {

    std::vector<Move> moves;
    static constexpr int dr[8] = { 0, 0, 1, -1, 1, 1, -1, -1 }; // direction of rows
    static constexpr int dc[8] = { 1, -1, 0, 0, 1, -1, 1, -1 }; // direction of columns

    for (int i = 0; i < 8; i++) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        while (nextRow >= 0 && nextRow <= 7 && nextCol >= 0 && nextCol <= 7) {
            if (board[nextRow][nextCol] == EMPTY) {
                moves.push_back(Move(row, col, nextRow, nextCol));
            }
            else {
                if (isEnemy(board[nextRow][nextCol], isWhite)) {
                    moves.push_back(Move(row, col, nextRow, nextCol));
                }
                break; // cannot move past another piece
            }
            nextRow += dr[i];
            nextCol += dc[i];
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getKingMoves(int row, int col, bool isWhite) {

    std::vector<Move> moves;
    static constexpr int dr[8] = { 0, 0, 1, -1, 1, 1, -1, -1 }; // direction of rows
    static constexpr int dc[8] = { 1, -1, 0, 0, 1, -1, 1, -1 }; // direction of columns

    for (int i = 0; i < 8; i++) {
        int nextRow = row + dr[i];
        int nextCol = col + dc[i];

        if (nextRow < 0 || nextRow > 7 || nextCol < 0 || nextCol > 7) {
            continue; // out of bounds
        }

        if (board[nextRow][nextCol] == EMPTY || isEnemy(board[nextRow][nextCol], isWhite)) {
            moves.push_back(Move(row, col, nextRow, nextCol));
        }
    }
    return moves;
}

std::vector<Move> ChessBoard::getAllMoves(bool isWhite) {

    std::vector<Move> allMoves;

    for (int row = 0; row < 8; row++) { // loop through all squares to get all possible moves
        for (int col = 0; col < 8; col++) {
            char piece = board[row][col];

            if (piece == EMPTY) continue;

            if (isWhite && piece >= 'a' && piece <= 'z') continue;
            if (!isWhite && piece >= 'A' && piece <= 'Z') continue;

            std::vector<Move> moves;

            switch (tolower(piece)) {
            case 'p': moves = getPawnMoves(row, col, isWhite); break;
            case 'n': moves = getKnightMoves(row, col, isWhite); break;
            case 'b': moves = getBishopMoves(row, col, isWhite); break;
            case 'r': moves = getRookMoves(row, col, isWhite); break;
            case 'q': moves = getQueenMoves(row, col, isWhite); break;
            case 'k': moves = getKingMoves(row, col, isWhite); break;
            }
            allMoves.insert(allMoves.end(), moves.begin(), moves.end());
        }
    }
    return allMoves;
}

std::vector<Move> ChessBoard::getLegalMoves(bool isWhite) {
    std::vector<Move> legalMoves;
    auto moves = getAllMoves(isWhite);

    for (auto& m : moves) {
        char captured;
        makeMove(m, captured);

        if (!isKingInCheck(isWhite)) {
            legalMoves.push_back(m);
        }

        undoMove(m, captured);
    }
    return legalMoves;
}