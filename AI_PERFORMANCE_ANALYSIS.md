# AI Performance Analysis — Deep Dive

## Executive Summary

The AI freezes the game for seconds because **a single AI turn triggers millions of redundant board scans and move generations**. The root cause is a depth-3 minimax search that calls `evaluatePosition()` thousands of times, and `evaluatePosition()` itself calls `getThreatenedSquares()` 3 times — each of which scans all 10,000 board cells and generates every enemy move from scratch. Nothing is cached. Nothing is incremental.

---

## 1. IDENTIFIED BOTTLENECKS (Ranked by Severity)

### CRITICAL #1: `getThreatenedSquares()` — O(10,000) per call, called thousands of times

```
getThreatenedSquares() {
    for y in 0..100:          // 10,000 cells scanned
        for x in 0..100:
            if cell has piece:
                getValidMoves()   // up to 64 moves per queen (8 dirs × 8 range)
    return std::set<>         // O(log n) per insert into ordered set
}
```

**Where it's called (per AI turn):**
| Caller | Calls to getThreatenedSquares | Context |
|--------|-------------------------------|---------|
| `evaluatePosition()` | **3× per call** (enemy threats, our defense, isInCheck) | Called at every leaf of minimax |
| `minimax()` intermediate nodes | 1× via `isInCheck()` for checkmate/stalemate detection | Called at every non-leaf node |
| `AIBrain::determinePhase()` | 1× to check king zone threats | Called once per turn |
| `isSafeSquare()` | 1× per call | Used in fallback movement |
| `isMoveSafe()` | Indirect (generates all enemy moves per piece) | Called per candidate move in Econ/Move |

**Estimated total with depth-3 minimax, ~50 moves per side:**
- Alpha-beta best case: ~2,000–5,000 leaf evaluations
- Each leaf: 3 × getThreatenedSquares = 3 × 10,000 cells = 30,000 cell reads
- **Total: ~60–150 MILLION cell reads from this function alone**

---

### CRITICAL #2: `isInCheck()` scans entire board to find king

```cpp
// Current: scans 10,000 cells to find the king
for (int y = 0; y < diameter; ++y)
    for (int x = 0; x < diameter; ++x)
        if (cell.piece && cell.piece->kingdom == kingdomId && cell.piece->type == PieceType::King)
```

But `Kingdom::getKing()` already exists and iterates only the ~5-10 pieces in the kingdom! This turns an O(5) operation into O(10,000) — a **2,000× slowdown per call**.

Same problem in `isCheckmate()`: scans the board twice (once for king, once to collect all pieces) when `kingdom.pieces` is right there.

---

### CRITICAL #3: `evaluatePosition()` does 3 full threat computations + mobility for all pieces

Inside one call to `evaluatePosition()`:
1. Material loop: O(pieces) ✅ fine
2. King safety loop: O(pieces) ✅ fine
3. **Mobility: `getValidMoves()` for ALL self pieces** → ~5 × O(64) = 320 operations
4. **Mobility: `getValidMoves()` for ALL enemy pieces** → ~5 × O(64) = 320 operations
5. **`isInCheck()` → `getThreatenedSquares()` → 10,000 cell scan** + all enemy moves
6. **`getThreatenedSquares(enemy)` for threat detection** → 10,000 cell scan + all enemy moves
7. **`getThreatenedSquares(self)` for defense check** → 10,000 cell scan + all self moves

**One `evaluatePosition()` call ≈ 30,000+ cell reads + ~200 move generations.**
Multiplied by 3,000 leaf nodes = **~100M operations**.

---

### CRITICAL #4: `isMoveSafe()` is O(enemies × moves) per check, called in hot loops

```cpp
isMoveSafe() {
    for each enemy piece:                    // ~5 pieces
        getValidMoves(enemy_piece)           // ~20 moves each
        for each move: if move == destination → unsafe
}
```

Called from `AIStrategyMove` and `AIStrategyEcon` for each candidate move:
- ~5 pieces × ~10 valid moves each = ~50 calls
- Each call: 5 enemies × 20 moves = 100 move checks
- **Total: ~5,000 move generations just for safety checks**

---

### CRITICAL #5: `findCheckmateIn()` — Iterative deepening + isCheckmate

```cpp
findCheckmateIn(maxDepth=2) {
    for depth 1..2:
        generate ALL moves  (~50)
        for each move:
            applyMove()
            isCheckmate()    // scans 10,000 cells × 3 + tests ALL moves
            if depth > 1: minimax(depth-1) on top of that
            undoMove()
}
```

`isCheckmate()` itself:
- Scans 10,000 cells for king
- Calls `isInCheck()` (10,000 cells + getThreatenedSquares)
- Scans 10,000 cells for all pieces
- For each piece × each move: simulate + `isInCheck()` again

**For 50 moves at depth 2: ~50 × (30,000+) = 1.5M cell reads minimum.**

---

### HIGH #6: Minimax branching factor explosion

With ~50 legal moves per side at depth 3:
- Worst case: 50³ = 125,000 nodes
- Best-case alpha-beta: 50^(3/2) ≈ 353 nodes (but requires perfect move ordering)
- Realistic with mediocre ordering: **~5,000–20,000 nodes**

Each node triggers move generation + evaluation → catastrophic when combined with the above.

---

### MEDIUM #7: `std::set<sf::Vector2i>` for threatened squares

`std::set` has O(log n) insertion and O(log n) lookup. With ~200 threatened squares, each operation costs ~8 comparisons.

A flat `bool[100][100]` array or `std::unordered_set` would be O(1).

---

### MEDIUM #8: Board scans for resource cells

`findResourceCells()` and `findFreeResourceCells()` both scan all 10,000 cells to find mines/farms. These could be cached once per turn.

---

### LOW #9: `std::vector` allocations in hot path

Every call to `getValidMoves()` and `generateMoves()` allocates a new `std::vector`. In the minimax inner loop, this means thousands of heap allocations per turn.

---

## 2. CALL FLOW FOR A SINGLE AI TURN (MID-GAME)

```
AIController::playTurn()
├─ AIBrain::update()
│  └─ determinePhase()
│     ├─ isInCheck() → getThreatenedSquares() → 10,000 cells     ← 1 full scan
│     └─ getThreatenedSquares() for king zone                     ← 1 full scan
│
├─ AIStrategySpecial::decide()                                     ← lightweight, OK
│
├─ AIStrategyEcon::decide()
│  ├─ findFreeResourceCells()                                      ← 10,000 cell scan
│  ├─ For each piece × each move: isMoveSafe()                    ← ~50 × (5 × 20) calls
│  └─ canBuild() loops                                             ← OK
│
├─ AIStrategyMove::decide()
│  ├─ findCheckmateIn(depth=2)                                    ← HEAVY (see #5)
│  │  ├─ generateMoves() → ~50 moves
│  │  ├─ For each: isCheckmate() → 3 × getThreatenedSquares      ← 50 × 30,000
│  │  └─ minimax(depth=1) per move                                ← 50 × 50 leaf evals
│  │
│  ├─ findBestMove(depth=3)                                       ← HEAVIEST
│  │  ├─ generateMoves() → 50 moves
│  │  └─ minimax(depth=2)
│  │     ├─ generateMoves() → 50 moves per node
│  │     └─ minimax(depth=1)
│  │        ├─ generateMoves() → 50 moves per node
│  │        └─ evaluatePosition() at each leaf                    ← 3,000+ calls
│  │           ├─ getValidMoves() × all pieces × 2 sides          ← 30,000+ ops
│  │           ├─ isInCheck() → getThreatenedSquares()             ← 10,000 cells
│  │           └─ getThreatenedSquares() × 2                       ← 20,000 cells
│  │
│  └─ isMoveSafe() per candidate                                  ← ~50 calls
│
├─ AIStrategyBuild::decide()                                       ← lightweight, OK
│
└─ AIStrategyEcon::decide() (production retry)                     ← lightweight, OK
```

**Estimated total per turn: ~100–300 MILLION cell reads + ~500,000 move generations**

---

## 3. MODERN GAME INDUSTRY SOLUTIONS

### TIER 1: Immediate Fixes (eliminate 95% of lag)

#### A. REMOVE MINIMAX — Use Heuristic Scoring Instead
The game industry standard for strategy/tactics games is **NOT** minimax. It's **utility-based scoring**. Games like Civilization, Total War, Fire Emblem all use weighted heuristic evaluation of each candidate move, NOT tree search.

**Replace `findBestMove(depth=3)` with:**
```
For each move:
    score  = capture value (if capturing)
    score += positional bonus (closer to enemy king, center control)
    score -= danger penalty (is destination attacked?)
    score += check bonus (does this give check?)
Pick highest score.
```
This is O(moves × enemies) instead of O(moves^depth × evaluatePosition).

**Depth-1 with smart heuristics plays just as well** as depth-3 with naive evaluation for this type of game.

#### B. Fix CheckSystem to NOT Scan the Board
```cpp
// BEFORE (O(10,000)):
for (y..diameter) for (x..diameter) if king...

// AFTER (O(1)):
const Piece* king = kingdom.getKing();
```

Pass kingdoms to `isInCheck()`/`isCheckmate()` directly instead of only passing `KingdomId`.

#### C. Cache Threatened Squares Once Per Decision
```cpp
// Compute ONCE at the start of playTurn():
auto enemyThreats = CheckSystem::getThreatenedSquares(enemy.id, board, config);
auto selfThreats  = CheckSystem::getThreatenedSquares(self.id, board, config);
```
Pass these into all strategy modules. Never recompute during the same turn.

#### D. Replace `std::set` with `bool[100][100]` grid for threats
```cpp
// BEFORE: std::set<sf::Vector2i> with O(log n) lookup
// AFTER: bool grid[100][100] with O(1) lookup
struct ThreatMap {
    bool threatened[100][100] = {};  // 10KB, fits in L1 cache
};
```

---

### TIER 2: Smart Optimizations (further 90% speedup on top of Tier 1)

#### E. Eliminate `isMoveSafe()` — Use the Cached Threat Map
```cpp
// BEFORE: For each candidate move, iterate all enemy pieces + their moves
bool isMoveSafe = engine.isMoveSafe(board, self, enemy, piece.id, dest, config);

// AFTER: Just look up the cached threat map
bool isMoveSafe = !enemyThreatMap[dest.y][dest.x];
```

#### F. Pre-compute Piece Move Lists Once
```cpp
// At the start of the turn:
std::unordered_map<int, std::vector<sf::Vector2i>> selfMoveCache;
for (auto& p : self.pieces)
    selfMoveCache[p.id] = MovementRules::getValidMoves(p, board, config);
```
Reuse throughout all strategy modules.

#### G. Lazy Checkmate Detection
Don't call `findCheckmateIn()` every turn. Only check for checkmate when:
- Material advantage > 2:1
- Enemy king has ≤ 2 escape squares
- We have pieces adjacent to enemy king

#### H. Resource Cell Cache
Maintain a list of resource cell positions. Only update when buildings change (rare). Don't scan 10,000 cells every turn.

---

### TIER 3: Architecture-Level Solutions (for future)

#### I. Async/Threaded AI
Run AI computation on a background thread. Show an "AI thinking..." animation. The game loop keeps rendering at 60fps.

```cpp
// Main thread:
std::future<std::vector<TurnCommand>> aiResult;
aiResult = std::async(std::launch::async, [&]() {
    return aiController.computeTurn(board, self, enemy, ...);
});

// Next frame: if (aiResult.ready()) applyCommands();
```

This is how ALL modern games handle AI think time — the player never sees a freeze.

#### J. Time-Budgeted Iterative Deepening
If keeping tree search:
```cpp
auto start = std::chrono::steady_clock::now();
for (int depth = 1; depth <= maxDepth; ++depth) {
    result = search(depth);
    if (elapsed > 30ms) break;  // Time budget reached
}
```
Guarantees the AI never takes more than 30ms regardless of board complexity.

#### K. Influence Maps (How Real RTS Games Do It)
Pre-compute per-cell scores:
```
attackInfluence[x][y]  = sum of (pieceValue / distance) for all enemy pieces
defenseInfluence[x][y] = sum of (pieceValue / distance) for all friendly pieces
```
Update incrementally. All AI decisions become simple lookups: "move toward cells where defenseInfluence > attackInfluence."

Used by: StarCraft 2 AI, Company of Heroes, Supreme Commander.

#### L. Monte Carlo Tree Search (MCTS)
Better than minimax for high-branching-factor games. Explores promising lines deeper while quickly pruning bad ones. Can be time-budgeted naturally.

Used by: AlphaGo, many modern board game AIs.

---

## 4. RECOMMENDED IMPLEMENTATION PLAN

### Phase 1: Kill the Lag (Immediately)
1. **Delete minimax entirely**. Replace with depth-1 scored move selection + heuristics
2. **Fix `CheckSystem`** to accept `Kingdom&` and use `kingdom.pieces`/`kingdom.getKing()` instead of board scans
3. **Cache threat maps** at start of AI turn, pass to all modules
4. **Replace `std::set<Vector2i>` with `bool[100][100]`** threat grid
5. **Cache piece move lists** once per turn

**Expected result: AI turn drops from 2-5 seconds to < 10ms**

### Phase 2: Polish (After Phase 1)
6. Async AI on background thread
7. Resource cell cache
8. Pre-computed influence maps for smarter positioning

### Phase 3: Advanced (Optional)
9. Time-budgeted search for endgame tactical precision
10. Transposition table for repeated positions
11. MCTS for complex situations

---

## 5. COMPLEXITY COMPARISON

| Approach | Estimated Operations Per Turn | Time (rough) |
|----------|-------------------------------|--------------|
| **Current** (minimax depth 3 + naive eval) | ~100-300M | **2-5 seconds** |
| Phase 1 (heuristic + cached threats) | ~50,000 | **< 1ms** |
| Phase 1 + influence maps | ~20,000 | **< 0.5ms** |
| Async minimax depth 2 + good eval | ~5M (off main thread) | **0ms perceived** |
