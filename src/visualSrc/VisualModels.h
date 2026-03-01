//// VisualModels.h
//#pragma once
//#include <gui/Image.h> // For piece images
//#include <td/Point.h>  // For screen coordinates
//#include "MoveBB.h"    // For Piece enum
//#include "ChessBoard.h" // For reference to the logical board
//
//// Constants for drawing a chess square (optional)
//const gui::CoordType SQUARE_SIZE = 80.0; // pixels
//
//// Visual model for a chess piece
//struct VisualPiece {
//    // 1. Pointer back to logical chess piece (optional)
//    const ChessBoard* logicalBoard; // So you can query state if needed
//    int square;                     // 0-63 square index
//    Piece piece;                     // Which piece type (enum)
//
//    // 2. GUI representation
//    gui::Image* image;              // Pointer to the sprite or image
//
//    // 3. Current and target position on screen (for animation)
//    gui::Point pos;
//    gui::Point targetPos;
//
//    // 4. Motion flag
//    bool isMoving = false;
//
//    // Constructor
//    VisualPiece(const ChessBoard* board, Piece p, int sq, gui::Image* img, gui::Point startPos)
//        : logicalBoard(board), square(sq), piece(p), image(img), pos(startPos), targetPos(startPos) {
//    }
//};
