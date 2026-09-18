 {
     {{BROOK, BKNIGHT, BBISHOP, BQUEEN, BKING, BBISHOP, BKNIGHT, BROOK}},
     { {BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN, BPAWN} },
     { {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} },
     { {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} },
     { {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} },
     { {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} },
     { {WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN, WPAWN} },
     { {WROOK, WKNIGHT, WBISHOP, WQUEEN, WKING, WBISHOP, WKNIGHT, WROOK} }
     } } {

         void ChessBoard::printBoard() const {
             for (const auto& row : board) {
                 for (char piece : row) {
                     std::cout << piece << ' ';
                 }
                 std::cout << '\n';
             }
         }

         void ChessBoard::makeMove(const Move& move, char& capturedPiece) {
             capturedPiece = board[move.toRow][move.toCol];

             board[move.toRow][move.toCol] = board[move.fromRow][move.fromCol];
             board[move.fromRow][move.fromCol] = EMPTY;

         }
         void ChessBoard::undoMove(const Move& move, char capturedPiece) {

             board[move.fromRow][move.fromCol] = board[move.toRow][move.toCol];
             board[move.toRow][move.toCol] = capturedPiece;

         }

         int ChessBoard::evaluateBoard() {
             int score = 0;
             for (int row = 0; row < 8; row++) {
                 for (int col = 0; col < 8; col++) {
                     char piece = board[row][col];
                     switch (piece) {
                     case WPAWN: score += 100; break;
                     case WKNIGHT: score += 320; break;
                     case WBISHOP: score += 330; break;
                     case WROOK: score += 500; break;
                     case WQUEEN: score += 900; break;
                     case WKING: score += 90000; break;
                     case BPAWN: score -= 100; break;
                     case BKNIGHT: score -= 320; break;
                     case BBISHOP: score -= 330; break;
                     case BROOK: score -= 500; break;
                     case BQUEEN: score -= 900; break;
                     case BKING: score -= 90000; break;
                     }
                 }
             }
             return score;
         }

         int ChessBoard::negamax(int depth, int alpha, int beta, bool isWhite) {
             if (depth == 0) {
                 return isWhite ? evaluateBoard() : -evaluateBoard();
             }

             std::vector<Move> moves = getLegalMoves(isWhite);

             int maxEval = -1000000;
             for (auto& move : moves) {
                 char captured;
                 makeMove(move, captured);
                 int score = -negamax(depth - 1, -beta, -alpha, !isWhite);
                 undoMove(move, captured);

                 if (score > maxEval) maxEval = score;
                 if (score > alpha) alpha = score;
                 if (alpha >= beta) break; // prune branch
             }
             return maxEval;
         }

         Move ChessBoard::findBestMove(int depth, bool isWhite) {
             int bestScore = -1000000;
             Move bestMove(0, 0, 0, 0);
             for (auto& move : getLegalMoves(isWhite)) {
                 char captured;
                 makeMove(move, captured);
                 int score = -negamax(depth - 1, -1000000, 1000000, !isWhite);
                 undoMove(move, captured);
                 if (score > bestScore) {
                     bestScore = score;
                     bestMove = move;
                 }
             }
             return bestMove;
         }


         Piece ChessBoard::charToPiece(char c) const {
             assert(
                 c == ' ' ||
                 c == 'P' || c == 'N' || c == 'B' || c == 'R' || c == 'Q' || c == 'K' ||
                 c == 'p' || c == 'n' || c == 'b' || c == 'r' || c == 'q' || c == 'k'
             );

             switch (c) {
             case 'P': return Piece::WPAWN;
             case 'N': return Piece::WKNIGHT;
             case 'B': return Piece::WBISHOP;
             case 'R': return Piece::WROOK;
             case 'Q': return Piece::WQUEEN;
             case 'K': return Piece::WKING;
             case 'p': return Piece::BPAWN;
             case 'n': return Piece::BKNIGHT;
             case 'b': return Piece::BBISHOP;
             case 'r': return Piece::BROOK;
             case 'q': return Piece::BQUEEN;
             case 'k': return Piece::BKING;
             case ' ': return Piece::EMPTY;
                 //default:
                     //assert(false && "Invalid char piece");
                     //return Piece::WPAWN; // never reached
             }
         }


         void ChessBoard::initBitboardsFromArray() {

             whitePawns = whiteKnights = whiteBishops = whiteRooks = whiteQueen = whiteKing = 0ULL;
             blackPawns = blackKnights = blackBishops = blackRooks = blackQueen = blackKing = 0ULL;

             for (int row = 0; row < 8; row++) {
                 for (int col = 0; col < 8; col++) {
                     int sq = row * 8 + col;
                     uint64_t mask = 1ULL << sq;
                     switch (board[row][col]) {
                     case 'P': whitePawns |= mask; break;
                     case 'N': whiteKnights |= mask; break;
                     case 'B': whiteBishops |= mask; break;
                     case 'R': whiteRooks |= mask; break;
                     case 'Q': whiteQueen |= mask; break;
                     case 'K': whiteKing |= mask; break;
                     case 'p': blackPawns |= mask; break;
                     case 'n': blackKnights |= mask; break;
                     case 'b': blackBishops |= mask; break;
                     case 'r': blackRooks |= mask; break;
                     case 'q': blackQueen |= mask; break;
                     case 'k': blackKing |= mask; break;
                     }
                 }
             }
         }


         // --- Move generation (array-based) ---
         std::vector<Move> getPawnMoves(int row, int col, bool IsWhite);
         std::vector<Move> getKnightMoves(int row, int col, bool isWhite);
         std::vector<Move> getBishopMoves(int row, int col, bool isWhite);
         std::vector<Move> getRookMoves(int row, int col, bool isWhite);
         std::vector<Move> getQueenMoves(int row, int col, bool isWhite);
         std::vector<Move> getKingMoves(int row, int col, bool isWhite);

         std::vector<Move> getAllMoves(bool IsWhite);
         std::vector<Move> getLegalMoves(bool isWhite);

         void makeMove(const Move& m, char& capturedPiece);
         void undoMove(const Move& m, char capturedPiece);

         bool isKingInCheck(bool isWhite);
         bool isKingInCheckBB(bool isWhite);

         int evaluateBoard();
         int negamax(int depth, int alpha, int beta, bool isWhite);
         Move findBestMove(int depth, bool isWhite);

         // --- Bitboard initialization ---
         void initBitboardsFromArray();

         void printBoard() const;

         Piece charToPiece(char c) const;

         // --- Helpers ---
         bool isEnemy(char piece, bool isWhite) const {
             if (piece == ' ') return false;
             return isWhite ? (piece >= 'a' && piece <= 'z') : (piece >= 'A' && piece <= 'Z');
         }

         bool pawnAttacksKing(int kingRow, int kingCol, bool isWhite) const;
         bool knightAttacksKing(int kingRow, int kingCol, bool isWhite) const;
         bool rookOrQueenAttacksKing(int kingRow, int kingCol, bool isWhite) const;
         bool bishopOrQueenAttacksKing(int kingRow, int kingCol, bool isWhite) const;
         bool kingAttacksKing(int kingRow, int kingCol, bool isWhite) const;

#include "ChessBoard.h"
#include "Move.h"
#include <iostream>


         void ChessBoard::printBoard() const {
             for (const auto& row : board) {
                 for (char piece : row) {
                     std::cout << piece << ' ';
                 }
                 std::cout << '\n';
             }
         }

         void ChessBoard::makeMove(const Move& move, char& capturedPiece) {
             capturedPiece = board[move.toRow][move.toCol];

             board[move.toRow][move.toCol] = board[move.fromRow][move.fromCol];
             board[move.fromRow][move.fromCol] = EMPTY;

         }
         void ChessBoard::undoMove(const Move& move, char capturedPiece) {

             board[move.fromRow][move.fromCol] = board[move.toRow][move.toCol];
             board[move.toRow][move.toCol] = capturedPiece;

         }

         int ChessBoard::evaluateBoard() {
             int score = 0;
             for (int row = 0; row < 8; row++) {
                 for (int col = 0; col < 8; col++) {
                     char piece = board[row][col];
                     switch (piece) {
                     case WPAWN: score += 100; break;
                     case WKNIGHT: score += 320; break;
                     case WBISHOP: score += 330; break;
                     case WROOK: score += 500; break;
                     case WQUEEN: score += 900; break;
                     case WKING: score += 90000; break;
                     case BPAWN: score -= 100; break;
                     case BKNIGHT: score -= 320; break;
                     case BBISHOP: score -= 330; break;
                     case BROOK: score -= 500; break;
                     case BQUEEN: score -= 900; break;
                     case BKING: score -= 90000; break;
                     }
                 }
             }
             return score;
         }

         int ChessBoard::negamax(int depth, int alpha, int beta, bool isWhite) {
             if (depth == 0) {
                 return isWhite ? evaluateBoard() : -evaluateBoard();
             }

             std::vector<Move> moves = getLegalMoves(isWhite);

             int maxEval = -1000000;
             for (auto& move : moves) {
                 char captured;
                 makeMove(move, captured);
                 int score = -negamax(depth - 1, -beta, -alpha, !isWhite);
                 undoMove(move, captured);

                 if (score > maxEval) maxEval = score;
                 if (score > alpha) alpha = score;
                 if (alpha >= beta) break; // prune branch
             }
             return maxEval;
         }

         Move ChessBoard::findBestMove(int depth, bool isWhite) {
             int bestScore = -1000000;
             Move bestMove(0, 0, 0, 0);
             for (auto& move : getLegalMoves(isWhite)) {
                 char captured;
                 makeMove(move, captured);
                 int score = -negamax(depth - 1, -1000000, 1000000, !isWhite);
                 undoMove(move, captured);
                 if (score > bestScore) {
                     bestScore = score;
                     bestMove = move;
                 }
             }
             return bestMove;
         }