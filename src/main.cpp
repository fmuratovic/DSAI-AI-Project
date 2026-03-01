//////// src/main.cpp
//#include <iostream>
//#include <string>
//#include <chrono>
//#include "ChessBoard.h"
//////
//int main() {
//    ChessBoard cb;
//    cb.printBitboards();
//////
//////    //auto moves = cb.getPawnMoves(6, 0, true);
//////    //cb.makeMove(moves[0]);
//////
//////    //cb.printBoard();
//////
//////
//////
//////
//////
//////    /* bool isWhiteTurn = true;
//////     int depth = 1;
//////
//////     auto start = std::chrono::high_resolution_clock::now();
//////
//////     Move bestMove;
//////
//////     while (depth <= 4) {
//////         std::cout << "Moves available: " << cb.getAllMoves(isWhiteTurn).size() << std::endl;
//////         bestMove = cb.findBestMove(depth, isWhiteTurn);
//////         std::cout << "Depth " << depth << " Best Move: (" << bestMove.fromRow << "," << bestMove.fromCol << ") -> ("
//////             << bestMove.toRow << "," << bestMove.toCol << ")\n";
//////         
//////         auto moves = cb.getLegalMoves(isWhiteTurn);
//////         std::cout << (isWhiteTurn ? "White" : "Black")
//////             << " legal moves: " << moves.size() << "\n";
//////
//////
//////         isWhiteTurn = !isWhiteTurn;
//////
//////         depth++;
//////
//////     }
//////
//////	 
//////		 std::cout << "Move from (" << bestMove.fromRow << "," << bestMove.fromCol << ") to ("
//////			 << bestMove.toRow << "," << bestMove.toCol << ")\n";
//////	 
//////
//////     auto end = std::chrono::high_resolution_clock::now(); // end timer
//////     std::chrono::duration<double> duration = end - start;
//////     std::cout << "Time taken: " << duration.count() << " seconds" << std::endl;*/
//////
//////    /*ChessBoard cb;
//////    bool isWhiteTurn = true;
//////    int moveCount = 0;
//////    const int MAX_MOVES = 200; // safety stop
//////
//////    while (moveCount < MAX_MOVES) {
//////
//////        cb.printBoard();
//////
//////        auto moves = cb.getAllMoves(isWhiteTurn);
//////        if (moves.empty()) {
//////            std::cout << (isWhiteTurn ? "White" : "Black")
//////                << " has no legal moves.\n";
//////            break;
//////        }
//////
//////        Move bestMove = cb.findBestMove(4, isWhiteTurn); // depth 4
//////
//////        char captured;
//////        cb.makeMove(bestMove, captured);
//////
//////        std::cout << (isWhiteTurn ? "White" : "Black")
//////            << " plays (" << bestMove.fromRow << "," << bestMove.fromCol
//////            << ") -> (" << bestMove.toRow << "," << bestMove.toCol << ")\n\n";
//////
//////        isWhiteTurn = !isWhiteTurn;
//////        moveCount++;
//////    }*/
//////
//     bool isWhiteTurnBB = true;
//     int depthBB = 1;
//
//     auto startBB = std::chrono::high_resolution_clock::now();
//
//     MoveBB bestMovebb;
//
//     while (depthBB <= 10) {
//         auto allMovesBB = cb.getAllMovesBB(isWhiteTurnBB);
//         std::cout << "Moves available (BB): " << allMovesBB.size() << std::endl;
//
//         bestMovebb = cb.findBestMoveBB(depthBB, isWhiteTurnBB); // You will need a findBestMoveBB
//
//         std::cout << "Depth " << depthBB
//             << " Best Move: from " << (int)bestMovebb.from << " to " << (int)bestMovebb.to << "\n";
//
//         // Optional: print legal moves count
//         auto legalMovesBB = cb.getLegalMovesBB(isWhiteTurnBB);
//         std::cout << (isWhiteTurnBB ? "White" : "Black")
//             << " legal moves (BB): " << legalMovesBB.size() << "\n";
//
//         isWhiteTurnBB = !isWhiteTurnBB;
//         depthBB++;
//     }
//
//     std::cout << "Best move chosen: from " << (int)bestMovebb.from
//         << " to " << (int)bestMovebb.to << "\n";
//
//     auto endBB = std::chrono::high_resolution_clock::now();
//     std::chrono::duration<double> durationBB = endBB - startBB;
//     std::cout << "Time taken (BB): " << durationBB.count() << " seconds" << std::endl;
//
//
//     
//    return 0;
//}
//
//
//
#include "Application.h"
#include "StartWindow.h"
#include <gui/WinMain.h>

int main(int argc, const char* argv[])
{
    std::cerr << "main() called\n";
    Application app(argc, argv);
    app.init("BA");
    return app.run();
}


