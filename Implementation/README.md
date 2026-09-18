# Implementation

## Layout

| Files | Role |
|---|---|
| `src/ChessBoard.h/.cpp`, `src/BBBasedMovement.cpp`, `src/BBKingCheckLogic.cpp`, `src/MoveBB.h` | engine: bitboards, move generation, make/undo, Zobrist hashing, transposition table, search, evaluation, FEN |
| `src/Connection.h` | game controller (`Chess`): history, undo/redo, draw rules, bot thread, board drawing, drag-and-drop, promotion |
| `src/StartWindow.h`, `src/ModeWindow.h`, `src/MainWindow.h`, `src/mainView.h`, `src/ToolBar.*`, `src/MenuBar.h`, `src/ViewSettings.h` | natGUI windows, toolbar, menu, settings popover |
| `res/main.xml`, `res/tr/`, `assets/` | image resources (referenced by id), translations, piece images |

The engine has no GUI dependency: `ChessBoard.cpp` + the two `BB*.cpp` files compile stand-alone,
which is how the perft and search benchmarks in the report were produced.

## Search in one paragraph

`findBestMoveBB_Timed` runs iterative deepening; each depth calls `negamaxBB`, which probes a
Zobrist-keyed transposition table, returns 0 for rule draws, hands over to `quiescenceBB`
at depth 0, scores mate as `-(MATE - ply)`, orders moves by MVV–LVA / promotion / killer /
history, and stores its result with an EXACT / LOWER / UPPER flag. The deepest iteration is
distributed over the root moves across `hardware_concurrency()` threads, each with a private
board copy and table. The next iteration is started only if its estimated cost fits the level's
time budget (Easy 150 ms, Normal 400 ms, Hard 1000 ms, Expert 2200 ms).

## Prerequisites

- CMake 3.18 or newer and a C++20 compiler (Visual Studio 2022, Apple Clang, GCC 12+);
- the natID SDK with `mainUtils` and `natGUI`;
- the SDK location is taken from `NATID_SDK_ROOT` (default `%USERPROFILE%\natID.SDK` / `~/natID.SDK`).

## Build

```bat
cmake -S Implementation -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

On macOS/Linux replace the generator with `-G Ninja` or omit it. Do not commit the `build/`
folder or `CMakeCache.txt`; both contain absolute paths of one machine.

## Playing

Start the application, choose *Human vs Human* or *Play vs Bot*, the bot's colour and a
difficulty, then drag pieces. Legal destinations are highlighted; captures are marked.
Undo retracts a full move pair against the bot. Language is changed in the settings popover.
