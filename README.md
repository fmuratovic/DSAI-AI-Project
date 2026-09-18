# DSAI-AI-Project — Chess

Course project for **Artificial Intelligence**.

**Author:** Faris Muratović (19958)
**Instructor:** Prof. Dr. Izudin Džafić, Faculty of Electrical Engineering, University of Sarajevo

A complete chess application built with the natID framework: a bitboard engine
with fully legal move generation verified by perft, an alpha–beta opponent with a
transposition table and time-budgeted difficulty levels, and a drag-and-drop
graphical interface.

## Main features

- twelve-bitboard board representation with incremental Zobrist hashing;
- fully legal move generation (castling, en passant, promotion, pins) — perft
  matches the published counts for the start position, Kiwipete, position 3 and 4;
- negamax with alpha–beta pruning, quiescence search, transposition table,
  MVV–LVA, killer and history move ordering, iterative deepening, mate-distance scoring;
- root-parallel final iteration on all cores;
- four difficulty levels defined as time budgets per move (150 ms – 2.2 s);
- evaluation: material, tapered piece-square tables, bishop pair, pawn structure,
  mobility, rook files, king shelter;
- draw detection: stalemate, fifty-move rule, threefold repetition, insufficient material;
- GUI: human vs human or vs bot, drag-and-drop with legal-move highlighting,
  promotion picker, undo/redo, captured-piece panels, Bosnian/English interface;
- search runs on a worker thread so the interface never freezes.

## Repository structure

```
DSAI-AI-Project/
├── Docs/
│   ├── Images/
│   ├── Chess_Report.tex
│   ├── Chess_Report.pdf
│   └── IEEEtran.cls
└── Implementation/
    ├── CMakeLists.txt
    ├── chess.cmake
    ├── README.md
    ├── assets/      piece images
    ├── res/         main.xml resources, translations, app icon
    └── src/         engine (ChessBoard*, MoveBB, BB*.cpp) and GUI (Connection.h, *Window.h, mainView.h)
```

Build instructions and implementation details are in [Implementation/README.md](Implementation/README.md).
