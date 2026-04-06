# A Normal Chess Game — Complete Development Plan

---

## Table of Contents

- [A Normal Chess Game — Complete Development Plan](#a-normal-chess-game--complete-development-plan)
  - [Table of Contents](#table-of-contents)
  - [Part I — Project Architecture](#part-i--project-architecture)
    - [1. Technical Stack](#1-technical-stack)
    - [2. Project Organization (Directory Structure)](#2-project-organization-directory-structure)
    - [3. Architectural Principles](#3-architectural-principles)
      - [3.1 Strict Separation of Responsibilities](#31-strict-separation-of-responsibilities)
      - [3.2 Data Flow](#32-data-flow)
      - [3.3 Command Pattern](#33-command-pattern)
      - [3.4 Code Neutrality Between the Two Kingdoms](#34-code-neutrality-between-the-two-kingdoms)
      - [3.5 Data vs Logic](#35-data-vs-logic)
    - [4. Module Architecture (Overview)](#4-module-architecture-overview)
    - [5. Detail of Each Module](#5-detail-of-each-module)
      - [5.1 Core — The Heart of the Engine](#51-core--the-heart-of-the-engine)
      - [5.2 Config — Global Settings](#52-config--global-settings)
      - [5.3 Board — Map and Terrain](#53-board--map-and-terrain)
      - [5.4 Units — Chess Pieces](#54-units--chess-pieces)
      - [5.5 Buildings — Buildings](#55-buildings--buildings)
      - [5.6 Systems — Game Rules](#56-systems--game-rules)
        - [TurnSystem](#turnsystem)
        - [CombatSystem](#combatsystem)
        - [EconomySystem](#economysystem)
        - [XPSystem](#xpsystem)
        - [BuildSystem](#buildsystem)
        - [ProductionSystem](#productionsystem)
        - [MarriageSystem](#marriagesystem)
        - [CheckSystem](#checksystem)
        - [EventLog](#eventlog)
      - [5.7 Kingdom — Kingdoms](#57-kingdom--kingdoms)
      - [5.8 AI — Artificial Intelligence](#58-ai--artificial-intelligence)
      - [5.9 UI — User Interface (TGUI)](#59-ui--user-interface-tgui)
      - [5.10 Render — Graphics Rendering (SFML)](#510-render--graphics-rendering-sfml)
      - [5.11 Input — Input Management](#511-input--input-management)
      - [5.12 Save — Saving and Persistence](#512-save--saving-and-persistence)
      - [5.13 Assets — Resource Management](#513-assets--resource-management)
    - [6. Main Game Loop](#6-main-game-loop)
    - [7. Turn Flow Diagram](#7-turn-flow-diagram)
  - [Part II — Detailed Technical Specifications](#part-ii--detailed-technical-specifications)
    - [8. The Map — Circular Procedural Generation](#8-the-map--circular-procedural-generation)
    - [9. Cell and Terrain System](#9-cell-and-terrain-system)
    - [10. Pieces (Units) System](#10-pieces-units-system)
    - [11. Buildings System](#11-buildings-system)
    - [12. Experience and Upgrade System](#12-experience-and-upgrade-system)
    - [13. Economic System](#13-economic-system)
    - [14. Turn-Based System](#14-turn-based-system)
    - [15. Formations System](#15-formations-system)
    - [16. Check/Checkmate System](#16-checkcheckmate-system)
    - [17. Artificial Intelligence](#17-artificial-intelligence)
    - [18. User Interface](#18-user-interface)
    - [19. Camera and Navigation](#19-camera-and-navigation)
    - [20. Save System](#20-save-system)
  - [Part III — Implementation Plan by Phases](#part-iii--implementation-plan-by-phases)
    - [Phase 0 — Project Setup](#phase-0--project-setup)
    - [Phase 1 — Engine Foundations](#phase-1--engine-foundations)
    - [Phase 2 — Map and Rendering](#phase-2--map-and-rendering)
    - [Phase 3 — Pieces and Movement](#phase-3--pieces-and-movement)
    - [Phase 4 — Turn-Based System](#phase-4--turn-based-system)
    - [Phase 5 — Buildings and Construction](#phase-5--buildings-and-construction)
    - [Phase 6 — Economy and Resources](#phase-6--economy-and-resources)
    - [Phase 7 — XP, Upgrades and Marriage](#phase-7--xp-upgrades-and-marriage)
    - [Phase 8 — TGUI User Interface](#phase-8--tgui-user-interface)
    - [Phase 9 — Artificial Intelligence](#phase-9--artificial-intelligence)
    - [Phase 10 — Formations](#phase-10--formations)
    - [Phase 11 — Saving and Persistence](#phase-11--saving-and-persistence)
    - [Phase 12 — Polish and Balancing](#phase-12--polish-and-balancing)
  - [Part IV — Game Constants Table](#part-iv--game-constants-table)
    - [Map](#map)
    - [Economy](#economy)
    - [Production](#production)
    - [Combat and HP](#combat-and-hp)
    - [Experience](#experience)
    - [Buildings (Dimensions)](#buildings-dimensions)
    - [Limits per Turn](#limits-per-turn)
  - [Part V — Best Practices and Conventions](#part-v--best-practices-and-conventions)
    - [C++ Code](#c-code)
    - [Architecture](#architecture)
    - [Performance](#performance)
    - [Maintainability](#maintainability)

---

## Part I — Project Architecture

### 1. Technical Stack

| Component       | Technology                  | Role |
|-----------------|-----------------------------|------|
| **Language**    | C++ (C++17 minimum)         | Main language for the engine and all logic |
| **2D Rendering**| SFML 2.6+                   | Window, sprite rendering, system event management |
| **Player UI**   | TGUI 1.x (for SFML)         | Interface widgets: panels, buttons, labels, lists |
| **Debug UI**    | Dear ImGui (optional)       | Developer tools: inspector, variable tweaking |
| **Build**       | CMake 3.20+                 | Cross-platform build system |
| **Serialization** | JSON (nlohmann/json or similar) | Save files and configuration files |

---

### 2. Project Organization (Directory Structure)

```
A Normal Chess Game/
│
├── CMakeLists.txt                    # Main build file
├── README.md
│
├── assets/                           # Game resources (runtime)
│   ├── textures/
│   │   ├── cells/                    # Terrain textures (16×16 px)
│   │   │   ├── grass.png
│   │   │   ├── dirt.png
│   │   │   ├── water.png
│   │   │   ├── church.png
│   │   │   ├── mine.png
│   │   │   ├── farm.png
│   │   │   ├── barrak.png
│   │   │   ├── wall_wood.png
│   │   │   ├── wall_stone.png
│   │   │   └── bridge.png
│   │   ├── pieces/
│   │   │   ├── white/                # White sprites (pawn, bishop, knight, rook, queen, king)
│   │   │   └── black/                # Black sprites
│   │   └── ui/
│   │       ├── crossed_swords.png
│   │       ├── shield_black.png
│   │       └── shield_white.png
│   ├── fonts/
│   └── config/
│       ├── game_params.json          # Gameplay constants (see Part IV)
│       └── ai_params.json            # AI settings
│
├── src/                              # Source code
│   ├── main.cpp                      # Entry point
│   │
│   ├── Core/
│   │   ├── Game.hpp / Game.cpp                 # Game class (state machine)
│   │   ├── GameState.hpp                       # Enum of states (Menu, Playing, Paused, GameOver)
│   │   └── GameClock.hpp / GameClock.cpp       # Delta time management
│   │
│   ├── Config/
│   │   ├── GameConfig.hpp / GameConfig.cpp     # Loading and access to game parameters
│   │   └── AIConfig.hpp / AIConfig.cpp         # Loading and access to AI parameters
│   │
│   ├── Board/
│   │   ├── Cell.hpp / Cell.cpp                 # A map cell
│   │   ├── CellType.hpp                        # Enum of cell types
│   │   ├── Board.hpp / Board.cpp               # The entire map (2D grid + circular mask)
│   │   └── BoardGenerator.hpp / BoardGenerator.cpp  # Procedural generation
│   │
│   ├── Units/
│   │   ├── PieceType.hpp                       # Enum (Pawn, Knight, Bishop, Rook, Queen, King)
│   │   ├── Piece.hpp / Piece.cpp               # Piece entity (identity, XP, type, position)
│   │   ├── PieceFactory.hpp / PieceFactory.cpp # Piece creation
│   │   └── MovementRules.hpp / MovementRules.cpp  # Legal movement calculation
│   │
│   ├── Buildings/
│   │   ├── BuildingType.hpp                    # Enum (Barracks, WoodWall, StoneWall, Arena, Church, Mine, Farm)
│   │   ├── Building.hpp / Building.cpp         # Building entity (type, cells, HP per cell, owner)
│   │   └── BuildingFactory.hpp / BuildingFactory.cpp  # Building creation
│   │
│   ├── Kingdom/
│   │   ├── Kingdom.hpp / Kingdom.cpp           # Kingdom (gold, list of pieces, list of buildings, king)
│   │   └── KingdomId.hpp                       # Enum (White, Black)
│   │
│   ├── Systems/
│   │   ├── TurnSystem.hpp / TurnSystem.cpp           # Turn management (actions, consumption)
│   │   ├── CombatSystem.hpp / CombatSystem.cpp       # Combat resolution (piece eats piece/block)
│   │   ├── EconomySystem.hpp / EconomySystem.cpp     # Income from mines/farms, expenses
│   │   ├── XPSystem.hpp / XPSystem.cpp               # XP allocation, upgrades
│   │   ├── BuildSystem.hpp / BuildSystem.cpp         # Building construction, validation
│   │   ├── ProductionSystem.hpp / ProductionSystem.cpp  # Barracks production
│   │   ├── MarriageSystem.hpp / MarriageSystem.cpp   # Church marriage system
│   │   ├── FormationSystem.hpp / FormationSystem.cpp # Formation management
│   │   ├── CheckSystem.hpp / CheckSystem.cpp         # Check / checkmate detection
│   │   └── EventLog.hpp / EventLog.cpp               # Event log
│   │
│   ├── AI/
│   │   ├── AIController.hpp / AIController.cpp       # Main AI controller
│   │   ├── AIEvaluator.hpp / AIEvaluator.cpp         # Board situation evaluation
│   │   ├── AIStrategyMove.hpp / AIStrategyMove.cpp   # Movement / attack strategy
│   │   ├── AIStrategyBuild.hpp / AIStrategyBuild.cpp # Construction strategy
│   │   ├── AIStrategyEcon.hpp / AIStrategyEcon.cpp   # Economic strategy (farm, production)
│   │   └── AIStrategySpecial.hpp / AIStrategySpecial.cpp  # Marriage, arena, upgrades
│   │
│   ├── UI/
│   │   ├── UIManager.hpp / UIManager.cpp             # UI orchestrator (TGUI)
│   │   ├── MainMenuUI.hpp / MainMenuUI.cpp           # Main menu screen
│   │   ├── HUD.hpp / HUD.cpp                         # In-game HUD (turn, gold, buttons)
│   │   ├── PiecePanel.hpp / PiecePanel.cpp           # Selected piece side panel
│   │   ├── BuildingPanel.hpp / BuildingPanel.cpp     # Selected building side panel
│   │   ├── BarracksPanel.hpp / BarracksPanel.cpp     # Barracks interface
│   │   ├── BuildToolPanel.hpp / BuildToolPanel.cpp   # Construction tool panel
│   │   ├── EventLogPanel.hpp / EventLogPanel.cpp     # Event log panel
│   │   └── ToolBar.hpp / ToolBar.cpp                 # Toolbar (mouse, hammer, log)
│   │
│   ├── Render/
│   │   ├── Renderer.hpp / Renderer.cpp               # Main rendering (map, pieces, buildings)
│   │   ├── Camera.hpp / Camera.cpp                   # SFML view (zoom, pan, centering)
│   │   ├── SpriteSheet.hpp / SpriteSheet.cpp         # (Optional) Sprite atlas
│   │   └── OverlayRenderer.hpp / OverlayRenderer.cpp # Overlays (reachable cells, preview, flags)
│   │
│   ├── Input/
│   │   ├── InputHandler.hpp / InputHandler.cpp       # SFML event dispatching
│   │   └── ToolState.hpp                             # Enum (Select, Build, Journal)
│   │
│   ├── Assets/
│   │   └── AssetManager.hpp / AssetManager.cpp       # Centralized texture/font loading
│   │
│   └── Save/
│       ├── SaveManager.hpp / SaveManager.cpp         # Serialization / deserialization of state
│       └── SaveData.hpp                              # Save data structure
│
├── saves/                            # Save directory (runtime)
│
├── docs/                             # Documentation
│   ├── BRAINSTORMING_RAPPORT.md
│   ├── QUESTIONS_IMPLEMENTATION.md
│   ├── REPONSES_IMPLEMENTATION.md
│   ├── choix techniques internet.md
│   └── PLAN_DEVELOPPEMENT.md         # This file
│
└── tests/                            # Unit tests (optional but recommended)
    ├── test_movement.cpp
    ├── test_combat.cpp
    ├── test_economy.cpp
    ├── test_xp.cpp
    ├── test_check.cpp
    └── test_board_gen.cpp
```

---

### 3. Architectural Principles

#### 3.1 Strict Separation of Responsibilities

The code is organized into **independent layers** that only know each other through minimal interfaces:

| Layer      | Knows                          | Does NOT Know |
|------------|--------------------------------|---------------|
| **Core**   | All layers (orchestrator)      | — |
| **Board**  | Cell, CellType                 | Units, Buildings, UI, Render |
| **Units**  | PieceType, position            | Board (directly), UI, Render |
| **Buildings** | BuildingType, position, HP   | Units, UI, Render |
| **Kingdom**| Units, Buildings, gold         | UI, Render |
| **Systems**| Board, Units, Buildings, Kingdom, Config | UI, Render |
| **AI**     | Systems (via the same API as the player) | UI, Render |
| **UI**     | Data to display (read-only)    | Game logic (does not modify anything directly) |
| **Render** | Board, Units, Buildings (read-only) | Game logic |
| **Input**  | SFML Events → commands         | Game logic (forwards commands) |

#### 3.2 Data Flow

```
Player inputs (mouse/keyboard)
       │
       ▼
  InputHandler  ──→  Command (enum/struct)
       │
       ▼
  TurnSystem  ──→  validates the command
       │              ──→  records the action in the current turn
       │
       ▼
  [ "Play" button pressed ]
       │
       ▼
  TurnSystem::commit()  ──→  executes all actions of the turn
       │                      ──→  CombatSystem, EconomySystem, XPSystem, etc.
       │
       ▼
  AIController::playTurn()  ──→  same API as the player
       │
       ▼
  Renderer::draw()  ──→  reads the state of Board, Units, Buildings
       │
       ▼
  UIManager::update()  ──→  updates TGUI panels
```

#### 3.3 Command Pattern

Every player or AI action is encoded as a **command** (move, build, produce, upgrade, marry, formGroup, breakGroup, resetTurn). This allows:

- **Storing** them before commit (transparent preview).
- **Canceling** them (Reset button = clear command queue).
- **Replaying** them (base for a future replay system).
- Making the **AI use the exact same API** (it produces commands).

```cpp
struct TurnCommand {
    enum Type { Move, Build, Produce, Upgrade, Marry, FormGroup, BreakGroup };
    Type type;
    // Type-specific data (union or variant)
};
```

#### 3.4 Code Neutrality Between the Two Kingdoms

The game code is **100% neutral**: the same logic serves both the white player and the black AI. A `Kingdom` has a `KingdomId` (White or Black), and the `Systems` never use if/else based on "is it the player or the AI". The only difference is the **source of commands**: InputHandler for the player, AIController for the AI.

#### 3.5 Data vs Logic

- **Data** (game state) is in Board, Units, Buildings, Kingdom.
- **Logic** (game rules) is in Systems.
- **Display** is in Render + UI.
- **Inputs** are in Input.

No rendering system modifies the game state. No game system draws anything.

---

### 4. Module Architecture (Overview)

```
┌─────────────────────────────────────────────────────────────┐
│                         main.cpp                            │
│                     Game::run() loop                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
         ┌─────────────────┼──────────────────┐
         ▼                 ▼                  ▼
   ┌──────────┐    ┌──────────────┐    ┌───────────┐
   │  Input   │    │   Systems    │    │  Render   │
   │ Handler  │    │  (logic)     │    │  + UI     │
   └────┬─────┘    └──────┬───────┘    └─────┬─────┘
        │                 │                  │
        │         ┌───────┼───────┐          │
        │         ▼       ▼       ▼          │
        │    ┌───────┐ ┌──────┐ ┌────────┐   │
        │    │ Board │ │Units │ │Building│   │
        │    └───────┘ └──────┘ └────────┘   │
        │         │       │       │          │
        │         └───────┼───────┘          │
        │                 ▼                  │
        │          ┌──────────┐              │
        │          │ Kingdom  │              │
        │          │ (White)  │              │
        │          │ (Black)  │              │
        │          └──────────┘              │
        │                 ▲                  │
        │         ┌───────┘                  │
        │         │                          │
        │    ┌────┴────┐                     │
        │    │   AI    │                     │
        │    │Controller│                    │
        │    └─────────┘                     │
        │                                    │
        └────────────── Config ──────────────┘
```

---

### 5. Detail of Each Module

#### 5.1 Core — The Heart of the Engine

**Files:** `Game.hpp/.cpp`, `GameState.hpp`, `GameClock.hpp/.cpp`

**Responsibilities:**

- Global game state machine (Menu → Playing → Paused → GameOver).
- Instantiation of all sub-systems.
- Main game loop: `handleInput()` → `update()` → `render()`.
- Delta time management for future animations.

**`Game` class:**

```cpp
class Game {
public:
    Game();
    void run();     // Main loop

private:
    void handleInput();
    void update();
    void render();

    sf::RenderWindow    m_window;
    tgui::Gui           m_gui;
    GameState           m_state;

    // Sub-systems (ownership)
    AssetManager        m_assets;
    GameConfig          m_config;
    Board               m_board;
    Kingdom             m_whiteKingdom;
    Kingdom             m_blackKingdom;
    TurnSystem          m_turnSystem;
    CombatSystem        m_combatSystem;
    EconomySystem       m_economySystem;
    XPSystem            m_xpSystem;
    BuildSystem         m_buildSystem;
    ProductionSystem    m_productionSystem;
    MarriageSystem      m_marriageSystem;
    FormationSystem     m_formationSystem;
    CheckSystem         m_checkSystem;
    EventLog            m_eventLog;
    AIController        m_ai;
    Camera              m_camera;
    Renderer            m_renderer;
    InputHandler        m_input;
    UIManager           m_uiManager;
    SaveManager         m_saveManager;
};
```

**`GameState`:**

```cpp
enum class GameState {
    MainMenu,       // Main menu (New Game / Continue)
    NewGameMenu,    // New game creation menu (name the game)
    LoadGameMenu,   // Load save menu
    Playing,        // In game
    Paused,         // Pause (Esc)
    GameOver        // End of game (checkmate)
};
```

---

#### 5.2 Config — Global Settings

**Files:** `GameConfig.hpp/.cpp`, `AIConfig.hpp/.cpp`

**Responsibilities:**

- Load gameplay constants from `config/game_params.json`.
- Load AI parameters from `config/ai_params.json`.
- Expose typed getters for each parameter.
- Allow the developer to modify values without recompiling.

**Content of `game_params.json`:**

```json
{
    "map": {
        "radius": 512,
        "cell_size_px": 16,
        "num_mines": 2,
        "num_farms": 3,
        "min_public_building_distance": 10,
        "player_spawn_zone_percent": 25,
        "ai_spawn_zone_percent": 25
    },
    "economy": {
        "starting_gold": 0,
        "mine_income_per_cell_per_turn": 10,
        "farm_income_per_cell_per_turn": 5,
        "barracks_cost": 50,
        "wood_wall_cost": 20,
        "stone_wall_cost": 40,
        "arena_cost": -1
    },
    "production": {
        "pawn_turns": 2,
        "knight_turns": 4,
        "bishop_turns": 4,
        "rook_turns": 6
    },
    "xp": {
        "kill_pawn": 20,
        "kill_knight": 50,
        "kill_bishop": 50,
        "kill_rook": 100,
        "kill_queen": 300,
        "destroy_block": 10,
        "arena_per_turn": 10,
        "threshold_pawn_to_knight_or_bishop": 100,
        "threshold_to_rook": 300
    },
    "combat": {
        "wood_wall_hp": 1,
        "stone_wall_hp": 3,
        "barracks_cell_hp": 1,
        "global_max_range": 8
    },
    "buildings": {
        "barracks_width": 4,
        "barracks_height": 2,
        "church_width": 4,
        "church_height": 3,
        "mine_width": 6,
        "mine_height": 6,
        "farm_width": 4,
        "farm_height": 3,
        "arena_size": 9
    }
}
```

**Principle:** Every numeric value in the game is read from this file. No magic constants in the source code. The `game_params.json` file is the **single source of truth** for balancing.

---

#### 5.3 Board — Map and Terrain

**Files:** `Cell.hpp/.cpp`, `CellType.hpp`, `Board.hpp/.cpp`, `BoardGenerator.hpp/.cpp`

**`CellType`:**

```cpp
enum class CellType {
    Void,           // Outside the map (outside the circle)
    Grass,          // Grass (traversable, default)
    Dirt,           // Dirt (traversable, decorative)
    Water           // Water (impassable)
};
```

**`Cell`:**

```cpp
struct Cell {
    CellType        type;             // Terrain type
    Building*       building;         // Pointer to building (nullptr if none)
    Piece*          piece;            // Pointer to occupying piece (nullptr if none)
    bool            isInCircle;       // Part of the playable map
    sf::Vector2i    position;         // Coordinates (col, row)
};
```

**`Board`:**

```cpp
class Board {
public:
    Board();
    void generate(const GameConfig& config);    // Procedural generation

    Cell& getCell(int x, int y);
    const Cell& getCell(int x, int y) const;
    bool isInBounds(int x, int y) const;
    bool isTraversable(int x, int y, KingdomId mover) const;
    int  getRadius() const;
    int  getDiameter() const;

    // Iteration over valid cells
    std::vector<sf::Vector2i> getAllValidCells() const;

private:
    int m_radius;
    int m_diameter;     // = 2 * radius
    std::vector<std::vector<Cell>> m_grid;   // m_diameter × m_diameter
};
```

**`BoardGenerator` — Generation Algorithm:**

1. Create a square grid of `2*radius × 2*radius` cells, all set to `CellType::Void`.
2. Apply the **circular mask**: for each cell `(x, y)`, calculate distance to center `(radius, radius)`. If `distance <= radius`, the cell is inside the circle and becomes `CellType::Grass`, otherwise it remains `Void`.
3. **Pixelate the circle**: the shape does not need to be smoothed. The circle is naturally pixelated by the grid.
4. Place **dirt** zones (decorative) randomly using blobs/noise.
5. Place **water** zones (blocking) randomly, ensuring they do not completely block passage.
6. Place **public buildings**: 1 church, 2 mines, 3 farms. Respect a minimum distance of 10 cells between public buildings. Prefer placement toward the center/half of the map (not in spawn zones).
7. Place the **initial King positions** (player in the left 25%, AI in the right 25%).

---

#### 5.4 Units — Chess Pieces

**Files:** `PieceType.hpp`, `Piece.hpp/.cpp`, `PieceFactory.hpp/.cpp`, `MovementRules.hpp/.cpp`

**`PieceType`:**

```cpp
enum class PieceType {
    Pawn,       // Level 0
    Knight,     // Level 1
    Bishop,     // Level 1
    Rook,       // Level 2
    Queen,      // Level 3 (not producible)
    King        // Level 4 (unique, not producible)
};
```

**`Piece`:**

```cpp
class Piece {
public:
    int             id;             // Unique permanent identifier
    PieceType       type;           // Current type (can change on upgrade)
    KingdomId       kingdom;        // Owner
    sf::Vector2i    position;       // Position on the grid
    int             xp;             // Accumulated XP (no cap)
    int             formationId;    // -1 if not in a formation

    bool canUpgradeTo(PieceType target, const GameConfig& config) const;
    int  getLevel() const;  // 0=Pawn, 1=Knight/Bishop, 2=Rook, 3=Queen, 4=King
};
```

**`MovementRules`:**

This module calculates all reachable squares for a given piece, taking into account:

- The piece type and its classic chess movement rules.
- The **global maximum range** (8 squares) for bishop, rook, and queen.
- Blocking by obstacles: water, walls (friendly and enemy).
- Passing through buildings (church, mines, farms, barracks).
- The knight that can jump over friendly pieces and walls (but not enemy walls).
- Friendly pieces that block squares.
- Enemy pieces that are attack targets.

**Movement Rules by Type:**

| Type     | Movement                          | Particularities |
|----------|-----------------------------------|-----------------|
| **Pawn** | 1 square up, down, left, right (no diagonal) | No initial double move, no en passant |
| **Knight** | Classic L-shape (fixed)          | Jumps over friendly pieces and walls, NOT enemy walls |
| **Bishop** | Diagonal, max 8 squares          | Blocked by walls and water |
| **Rook**   | Straight lines (H/V), max 8 squares | Blocked by walls and water |
| **Queen**  | Diagonal + straight lines, max 8 squares | Blocked by walls and water |
| **King**   | 1 square in all directions (including diagonal) | No castling |

```cpp
class MovementRules {
public:
    // Returns all reachable squares for a piece
    static std::vector<sf::Vector2i> getValidMoves(
        const Piece& piece,
        const Board& board,
        const GameConfig& config
    );

private:
    static std::vector<sf::Vector2i> getPawnMoves(const Piece&, const Board&);
    static std::vector<sf::Vector2i> getKnightMoves(const Piece&, const Board&, const GameConfig&);
    static std::vector<sf::Vector2i> getBishopMoves(const Piece&, const Board&, const GameConfig&);
    static std::vector<sf::Vector2i> getRookMoves(const Piece&, const Board&, const GameConfig&);
    static std::vector<sf::Vector2i> getQueenMoves(const Piece&, const Board&, const GameConfig&);
    static std::vector<sf::Vector2i> getKingMoves(const Piece&, const Board&);

    // Helper: straight line tracing (for bishop, rook, queen)
    static std::vector<sf::Vector2i> traceDirection(
        const Piece& piece, const Board& board,
        int dx, int dy, int maxRange
    );
};
```

---

#### 5.5 Buildings — Buildings

**Files:** `BuildingType.hpp`, `Building.hpp/.cpp`, `BuildingFactory.hpp/.cpp`

**`BuildingType`:**

```cpp
enum class BuildingType {
    // Public (indestructible, generated)
    Church,
    Mine,
    Farm,
    // Private (constructible, destructible)
    Barracks,
    WoodWall,
    StoneWall,
    Arena
};
```

**`Building`:**

```cpp
class Building {
public:
    int                         id;
    BuildingType                type;
    KingdomId                   owner;      // Neutral for public buildings
    sf::Vector2i                origin;     // Top-left corner
    int                         width;
    int                         height;
    std::vector<int>            cellHP;     // HP per cell (size = width * height)

    // Barracks only
    bool                        isProducing;
    PieceType                   producingType;
    int                         turnsRemaining;

    bool isPublic() const;
    bool isDestroyed() const;                   // All cells at 0 HP
    bool isCellDamaged(int localX, int localY) const;
    int  getCellHP(int localX, int localY) const;
    void damageCellAt(int localX, int localY);  // -1 HP
    std::vector<sf::Vector2i> getOccupiedCells() const;
    std::vector<sf::Vector2i> getAdjacentCells(const Board& board) const;
};
```

**Classification:**

| Type       | Public? | Destructible? | Constructible? | Blocking? |
|------------|---------|---------------|----------------|-----------|
| Church     | Yes     | No            | No             | No        |
| Mine       | Yes     | No            | No             | No        |
| Farm       | Yes     | No            | No             | No        |
| Barracks   | No      | Yes (1 HP/cell) | Yes (king)     | No        |
| WoodWall   | No      | Yes (1 HP)    | Yes (king)     | **Yes**   |
| StoneWall  | No      | Yes (3 HP)    | Yes (king)     | **Yes**   |
| Arena      | No      | Yes           | Yes (king)     | No        |

---

#### 5.6 Systems — Game Rules

Each system is a **mostly stateless** module. It operates on the data (Board, Units, Buildings, Kingdom) and applies the rules.

##### TurnSystem

Manages the **turn cycle**. Responsible for:

- Recording the active player's commands (move, build, produce, upgrade, marry).
- Validating each command (is it legal?).
- Storing pending commands (before commit).
- Executing the commit: apply all commands, credit income, advance production counters, award arena XP.
- Alternating between kingdoms (player → AI → player → ...).
- Allowing Reset (clear pending commands).

```cpp
class TurnSystem {
public:
    void setActiveKingdom(KingdomId id);
    KingdomId getActiveKingdom() const;
    int  getTurnNumber() const;

    bool queueCommand(const TurnCommand& cmd);   // Records (returns false if illegal)
    void resetPendingCommands();                  // "Reset" button
    const std::vector<TurnCommand>& getPendingCommands() const;

    void commitTurn();                            // "Play" button (executes everything)

private:
    KingdomId                   m_activeKingdom;
    int                         m_turnNumber;
    std::vector<TurnCommand>    m_pendingCommands;

    // Flags for per-turn limits
    bool m_hasMoved;
    bool m_hasBuilt;
    bool m_hasProduced;
    bool m_hasMarried;
};
```

##### CombatSystem

Resolves combat when a piece moves onto an enemy square (piece or block).

```cpp
class CombatSystem {
public:
    struct CombatResult {
        bool    occurred;
        bool    targetWasPiece;     // true = piece eaten, false = block destroyed
        int     xpGained;
    };

    static CombatResult resolve(
        Piece& attacker,
        Board& board,
        sf::Vector2i target,
        Kingdom& attackerKingdom,
        Kingdom& defenderKingdom,
        const GameConfig& config,
        EventLog& log
    );
};
```

##### EconomySystem

Calculates and credits income at the end of each turn.

```cpp
class EconomySystem {
public:
    static void collectIncome(
        Kingdom& kingdom,
        const Board& board,
        const std::vector<Building>& publicBuildings,
        const GameConfig& config,
        EventLog& log
    );
    // For each mine/farm: count cells occupied by the kingdom
    // and only if the enemy is NOT present on the building
};
```

##### XPSystem

Awards XP and manages upgrades.

```cpp
class XPSystem {
public:
    static void grantKillXP(Piece& killer, PieceType victim, const GameConfig& config);
    static void grantBlockDestroyXP(Piece& destroyer, const GameConfig& config);
    static void grantArenaXP(Kingdom& kingdom, const Board& board,
                             const std::vector<Building>& arenas, const GameConfig& config);
    static bool canUpgrade(const Piece& piece, PieceType target, const GameConfig& config);
    static void upgrade(Piece& piece, PieceType target);
};
```

##### BuildSystem

Validates and places buildings on the map.

```cpp
class BuildSystem {
public:
    static bool canBuild(
        BuildingType type,
        sf::Vector2i origin,
        const Piece& king,
        const Board& board,
        const Kingdom& kingdom,
        const GameConfig& config
    );
    // Checks:
    // 1. The king is adjacent to the building's area
    // 2. All cells in the area are free (no water, no other building, no piece)
    // 3. The kingdom has enough gold
    // 4. The player has not already built this turn

    static Building place(
        BuildingType type,
        sf::Vector2i origin,
        KingdomId owner,
        Board& board,
        const GameConfig& config
    );
};
```

##### ProductionSystem

Manages unit production in barracks.

```cpp
class ProductionSystem {
public:
    static bool canStartProduction(
        const Building& barracks,
        PieceType type,
        const Kingdom& kingdom,
        const GameConfig& config
    );

    static void startProduction(Building& barracks, PieceType type, const GameConfig& config);
    static void advanceProduction(Building& barracks);
    static bool isProductionComplete(const Building& barracks);
    static sf::Vector2i findSpawnCell(const Building& barracks, const Board& board);
};
```

##### MarriageSystem

Manages queen creation via the church ritual.

```cpp
class MarriageSystem {
public:
    // Checks if marriage is possible:
    // 1. The king, a bishop and a pawn of the same kingdom are on church cells
    // 2. The king and pawn are on adjacent cells
    // 3. There is no queen already in the kingdom
    // 4. The church is not contested (no enemy piece)
    static bool canMarry(
        const Kingdom& kingdom,
        const Board& board,
        const Building& church
    );

    // Transforms the pawn into a queen
    static void performMarriage(
        Kingdom& kingdom,
        const Board& board,
        const Building& church,
        EventLog& log
    );
};
```

##### CheckSystem

Detects check and checkmate.

```cpp
class CheckSystem {
public:
    // Is the king of the given kingdom in check?
    static bool isInCheck(
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // Is the king of the given kingdom in checkmate?
    static bool isCheckmate(
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // Can the king move to this square without being in check?
    static bool isSafeSquare(
        sf::Vector2i pos,
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // All controlled (threatened) squares by a kingdom
    static std::set<sf::Vector2i> getThreatenedSquares(
        KingdomId attackerKingdom,
        const Board& board,
        const GameConfig& config
    );
};
```

**Checkmate Algorithm Detail:**

For a king in check, it is in **checkmate** if and only if the **three conditions** are met:

1. The king cannot **flee** to any adjacent square (all are controlled by the enemy, occupied by allies, or out of bounds).
2. No allied piece can **capture** the piece that is checking the king.
3. No allied piece can **interpose** between the king and the threatening piece (except if it is a knight — knights cannot be blocked).

**Note:** When the King has no subjects (all other pieces dead), classic checkmate rules still apply — the King must be checkmated, not just captured.

##### EventLog

Log of all game events.

```cpp
class EventLog {
public:
    struct Event {
        int         turnNumber;
        KingdomId   kingdom;
        std::string message;
    };

    void log(int turn, KingdomId kingdom, const std::string& msg);
    const std::vector<Event>& getEvents() const;
    std::vector<Event> getEventsForKingdom(KingdomId id) const;
};
```

---

#### 5.7 Kingdom — Kingdoms

**Files:** `Kingdom.hpp/.cpp`, `KingdomId.hpp`

```cpp
enum class KingdomId { White, Black };

class Kingdom {
public:
    KingdomId                   id;
    int                         gold;
    std::vector<Piece>          pieces;
    std::vector<Building>       buildings;

    Piece* getKing();
    const Piece* getKing() const;
    bool hasQueen() const;
    bool hasSubjects() const;     // Does the king have at least one subject?
    int  pieceCount() const;
    Piece* getPieceAt(sf::Vector2i pos);
    Building* getBuildingAt(sf::Vector2i pos);

    void addPiece(const Piece& piece);
    void removePiece(int pieceId);
    void addBuilding(const Building& building);
    void removeBuilding(int buildingId);
};
```

---

#### 5.8 AI — Artificial Intelligence

**Files:** `AIController.hpp/.cpp`, `AIEvaluator.hpp/.cpp`, `AIStrategy*.hpp/.cpp`

The AI is **symbolic** (rule-based), not machine-learning based. It follows a configurable decision tree.

**AI Architecture:**

```
AIController::playTurn()
│
├── AIEvaluator::evaluate()          # Evaluate board situation (score)
│
├── AIStrategyEcon::decide()         # Economic decisions (farm, production)
│   ├── Go farm if no income
│   ├── Build barracks if needed
│   └── Start production if barracks free
│
├── AIStrategyBuild::decide()        # Construction decisions
│   ├── Build defensive walls
│   └── Build arena
│
├── AIStrategyMove::decide()         # Movement / attack decisions
│   ├── Attack high-value target
│   ├── Defend the king
│   ├── Take a farm zone
│   └── Advance toward enemy
│
├── AIStrategySpecial::decide()      # Special actions
│   ├── Upgrade a piece if possible
│   ├── Marriage if conditions met
│   └── Place pieces in arena
│
└── Produce the corresponding TurnCommands
```

**Configuration via `ai_params.json`:**

```json
{
    "weights": {
        "farm_priority": 0.8,
        "attack_priority": 0.6,
        "defense_priority": 0.7,
        "build_priority": 0.5,
        "upgrade_priority": 0.4,
        "marriage_priority": 0.3
    },
    "thresholds": {
        "min_gold_before_attack": 100,
        "min_pieces_before_attack": 3,
        "wall_defense_radius": 5
    },
    "randomness": 0.1
}
```

The developer can modify `randomness` to make the AI less deterministic, and the `weights` / `thresholds` to change its priorities. This allows adjusting the difficulty without changing the code.

---

#### 5.9 UI — User Interface (TGUI)

**Files:** `UIManager.hpp/.cpp`, `MainMenuUI.hpp/.cpp`, `HUD.hpp/.cpp`, `PiecePanel.hpp/.cpp`, `BuildingPanel.hpp/.cpp`, `BarracksPanel.hpp/.cpp`, `BuildToolPanel.hpp/.cpp`, `EventLogPanel.hpp/.cpp`, `ToolBar.hpp/.cpp`

**Responsibilities:**

- The UI is **purely display**. It never directly modifies the game state.
- It emits **signals** or **callbacks** that translate into `TurnCommand` via the InputHandler.
- It **reads** game data (read-only) to update widgets.

**UIManager:**

```cpp
class UIManager {
public:
    void init(tgui::Gui& gui, const AssetManager& assets);
    void showMainMenu();
    void showHUD();
    void showPiecePanel(const Piece& piece, const GameConfig& config);
    void showBuildingPanel(const Building& building);
    void showBarracksPanel(const Building& barracks, const Kingdom& kingdom, const GameConfig& config);
    void showBuildToolPanel(const Kingdom& kingdom, const GameConfig& config);
    void showEventLogPanel(const EventLog& log);
    void hideAllPanels();
    void update();

private:
    MainMenuUI      m_mainMenu;
    HUD             m_hud;
    PiecePanel      m_piecePanel;
    BuildingPanel   m_buildingPanel;
    BarracksPanel   m_barracksPanel;
    BuildToolPanel  m_buildToolPanel;
    EventLogPanel   m_eventLogPanel;
    ToolBar         m_toolBar;
};
```

**In-game Screen Layout:**

```
┌─────────────────────────────────────────────────────────────────┐
│  Turn: 15   |   ♔ White's turn   |   Gold: 230   |  [Reset] [▶]│  ← HUD (top)
├──────────────────────────────────────────┬──────────────────────┤
│                                          │                      │
│                                          │   Side Panel         │
│                                          │   (empty by default) │
│              GAME MAP                    │                      │
│              (Renderer)                  │   - Piece info       │
│                                          │   - Building info    │
│                                          │   - Barracks         │
│                                          │   - Construction     │
│                                          │   - Log              │
│                                          │                      │
├──────────────────────────────────────────┴──────────────────────┤
│  [🖱️ Mouse]  [🔨 Build]  [📖 Log]                             │  ← ToolBar (bottom-left)
└─────────────────────────────────────────────────────────────────┘
```

---

#### 5.10 Render — Graphics Rendering (SFML)

**Files:** `Renderer.hpp/.cpp`, `Camera.hpp/.cpp`, `OverlayRenderer.hpp/.cpp`

**Responsibilities:**

- Draw the map (visible cells in the viewport).
- Draw pieces.
- Draw buildings.
- Draw overlays (reachable cells in green, construction preview, movement ghosts, contested zone indicators, graying of destroyed cells).

**Renderer:**

```cpp
class Renderer {
public:
    void init(const AssetManager& assets);

    void draw(
        sf::RenderWindow& window,
        const Camera& camera,
        const Board& board,
        const Kingdom& white,
        const Kingdom& black,
        const std::vector<Building>& publicBuildings,
        const TurnSystem& turnSystem   // for pending commands (previews)
    );

private:
    void drawBoard(sf::RenderWindow& window, const Camera& camera, const Board& board);
    void drawBuildings(sf::RenderWindow& window, const Camera& camera, /* ... */);
    void drawPieces(sf::RenderWindow& window, const Camera& camera, /* ... */);
    void drawOverlays(sf::RenderWindow& window, const Camera& camera, const TurnSystem& turnSystem);
    void drawZoneIndicators(sf::RenderWindow& window, const Camera& camera, /* ... */);

    const AssetManager* m_assets;
};
```

**Rendering Optimization (1024×1024 cell map):**

The map can be up to `2*512 = 1024` cells in diameter. At 16×16 pixels, this is 16,384 × 16,384 pixels. It is **mandatory** to draw only the cells visible in the viewport.

```cpp
// Calculate visible bounds
sf::FloatRect viewBounds = camera.getViewBounds();
int minCol = std::max(0, (int)(viewBounds.left / CELL_SIZE));
int maxCol = std::min(diameter - 1, (int)((viewBounds.left + viewBounds.width) / CELL_SIZE));
int minRow = std::max(0, (int)(viewBounds.top / CELL_SIZE));
int maxRow = std::min(diameter - 1, (int)((viewBounds.top + viewBounds.height) / CELL_SIZE));

for (int y = minRow; y <= maxRow; ++y) {
    for (int x = minCol; x <= maxCol; ++x) {
        if (!board.getCell(x, y).isInCircle) continue;
        // draw cell...
    }
}
```

---

#### 5.11 Input — Input Management

**Files:** `InputHandler.hpp/.cpp`, `ToolState.hpp`

**`ToolState`:**

```cpp
enum class ToolState {
    Select,     // Classic mouse tool (default)
    Build,      // Construction tool
    Journal     // Journal tool
};
```

**`InputHandler` — Event Dispatching:**

```cpp
class InputHandler {
public:
    void handleEvent(
        const sf::Event& event,
        Camera& camera,
        Board& board,
        TurnSystem& turnSystem,
        UIManager& uiManager,
        const GameConfig& config
    );

    ToolState getCurrentTool() const;
    void setTool(ToolState tool);

    // Selection
    Piece* getSelectedPiece() const;
    Building* getSelectedBuilding() const;

private:
    ToolState       m_currentTool = ToolState::Select;
    Piece*          m_selectedPiece = nullptr;
    Building*       m_selectedBuilding = nullptr;
    sf::Vector2i    m_buildPreviewOrigin;
    BuildingType    m_buildPreviewType;

    void handleSelectTool(const sf::Event& event, /* ... */);
    void handleBuildTool(const sf::Event& event, /* ... */);
    void handleCameraInput(const sf::Event& event, Camera& camera);
};
```

**Input Mapping:**

| Action                        | Input |
|-------------------------------|-------|
| Move camera                   | Middle click + drag, or drag on empty area |
| Zoom in/out                   | Mouse wheel |
| Select piece / building       | Left click |
| Move piece (attack)           | Left click on piece → left click on destination |
| Place building (build mode)   | Select in panel → left click on map |
| Center camera on king         | Spacebar |
| Pause                         | Esc |

---

#### 5.12 Save — Saving and Persistence

**Files:** `SaveManager.hpp/.cpp`, `SaveData.hpp`

```cpp
struct SaveData {
    std::string     gameName;
    int             turnNumber;
    KingdomId       activeKingdom;

    // Map state
    struct CellData {
        CellType type;
        bool isInCircle;
    };
    std::vector<std::vector<CellData>> grid;
    int mapRadius;

    // Kingdoms
    struct KingdomData {
        KingdomId id;
        int gold;
        std::vector<Piece> pieces;
        std::vector<Building> buildings;
    };
    KingdomData whiteKingdom;
    KingdomData blackKingdom;

    // Public buildings
    std::vector<Building> publicBuildings;

    // Log
    std::vector<EventLog::Event> events;
};

class SaveManager {
public:
    bool save(const std::string& filepath, const SaveData& data);
    bool load(const std::string& filepath, SaveData& outData);
    std::vector<std::string> listSaves(const std::string& savesDir);
    bool deleteSave(const std::string& filepath);
};
```

**Save Format:** JSON (via nlohmann/json). One file per save in the `saves/` folder. The filename corresponds to the game name.

**Replay Preparation:** Additionally store the history of `TurnCommand` in the save. Not used yet, but will simplify future replay system implementation.

---

#### 5.13 Assets — Resource Management

**Files:** `AssetManager.hpp/.cpp`

```cpp
class AssetManager {
public:
    void loadAll(const std::string& assetsDir);

    const sf::Texture& getCellTexture(CellType type) const;
    const sf::Texture& getBuildingTexture(BuildingType type) const;
    const sf::Texture& getPieceTexture(PieceType type, KingdomId kingdom) const;
    const sf::Texture& getUITexture(const std::string& name) const;
    const sf::Font&    getFont() const;

private:
    std::map<std::string, sf::Texture>  m_textures;
    sf::Font                            m_font;
};
```

Loads all textures at startup. No texture is loaded during the game.

---

### 6. Main Game Loop

```cpp
void Game::run() {
    while (m_window.isOpen()) {
        handleInput();
        update();
        render();
    }
}

void Game::handleInput() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        m_gui.handleEvent(event);       // TGUI consumes first

        if (event.type == sf::Event::Closed)
            m_window.close();

        if (m_state == GameState::Playing)
            m_input.handleEvent(event, m_camera, m_board, m_turnSystem, m_uiManager, m_config);
    }
}

void Game::update() {
    switch (m_state) {
        case GameState::Playing:
            // Check if it's the AI's turn
            if (m_turnSystem.getActiveKingdom() == KingdomId::Black) {
                m_ai.playTurn(m_board, m_blackKingdom, m_whiteKingdom,
                              m_turnSystem, m_config, m_eventLog);
                m_turnSystem.commitTurn();
                // Check game over
                if (m_checkSystem.isCheckmate(KingdomId::White, m_board, m_config))
                    m_state = GameState::GameOver;
            }
            m_uiManager.update();
            break;
        // ...other states...
    }
}

void Game::render() {
    m_window.clear(sf::Color(30, 30, 30));  // Dark background (outside map)

    if (m_state == GameState::Playing || m_state == GameState::Paused) {
        m_renderer.draw(m_window, m_camera, m_board,
                        m_whiteKingdom, m_blackKingdom,
                        m_publicBuildings, m_turnSystem);
    }

    m_gui.draw();   // TGUI draws on top
    m_window.display();
}
```

---

### 7. Turn Flow Diagram

```
START OF PLAYER'S TURN
│
├── The player is free to:
│   ├── Select a piece → see reachable squares (green overlay)
│   ├── Move a piece (max 1) → ghost display + arrow
│   ├── Select build tool → open construction panel
│   │   └── Place a building (max 1) → transparent overlay preview
│   ├── Select a barracks → open barracks panel
│   │   └── Start production (max 1)
│   ├── Upgrade a piece (if XP threshold reached)
│   ├── Initiate marriage at the church (max 1)
│   ├── Create / dissolve a formation
│   ├── Consult the log
│   └── Move camera, zoom, select elements
│
├── [Reset button] → cancel ALL pending actions
│
└── [Play button] → TURN COMMIT
    │
    ├── 1. Execute movement (resolve combat if applicable)
    │      ├── Piece → enemy piece: combat → XP
    │      └── Piece → enemy block: destruction → XP
    │
    ├── 2. Execute construction
    │
    ├── 3. Execute production (advance barracks counters)
    │      └── If counter reaches 0: spawn piece on adjacent free square
    │
    ├── 4. Execute marriage (pawn → queen)
    │
    ├── 5. Execute upgrades
    │
    ├── 6. Credit income (mines + farms)
    │
    ├── 7. Award arena XP
    │
    ├── 8. Check enemy pawn/king status (game over?)
    │
    ├── 9. Check check / checkmate
    │
    └── 10. Log events → pass to AI turn
         │
         ▼
    AI TURN (instant)
    │
    ├── AIController::playTurn() → produces TurnCommands
    ├── TurnSystem::commitTurn() → same pipeline as player
    ├── Check game over (player side)
    │
    └── Back to PLAYER TURN
```

---

## Part II — Detailed Technical Specifications

### 8. The Map — Circular Procedural Generation

**Shape:** Circle with radius 512 cells. The internal grid is a 1024×1024 square, with a circular mask. Cells outside the circle are of type `Void` and are never rendered or accessible.

**Detailed Generation Algorithm:**

```
FUNCTION generateMap(config):
    radius = config.map.radius                           // 512
    diameter = radius * 2                                // 1024
    center = (radius, radius)                            // (512, 512)

    // STEP 1: Initialize the grid
    FOR each cell (x, y) in [0, diameter) × [0, diameter):
        distance = sqrt((x - center.x)² + (y - center.y)²)
        IF distance <= radius:
            grid[x][y] = Cell(Grass, isInCircle=true)
        ELSE:
            grid[x][y] = Cell(Void, isInCircle=false)

    // STEP 2: Place dirt zones (decorative)
    // Use simplified Perlin noise or random blobs
    FOR i = 1 to NUM_DIRT_BLOBS:
        blobCenter = random position inside circle
        blobRadius = random(5, 20)
        FOR each cell in blobRadius around blobCenter:
            IF cell is inside circle AND cell is Grass:
                cell.type = Dirt

    // STEP 3: Place water zones
    // Small random lakes. Ensure they do not cut the map in half.
    FOR i = 1 to NUM_LAKES:
        lakeCenter = random position (not in spawn zones)
        lakeRadius = random(3, 10)
        FOR each cell in lakeRadius around lakeCenter:
            IF cell is inside circle AND cell is Grass or Dirt:
                cell.type = Water
        // Connectivity check (pathfinding between player spawn and AI spawn)
        // If not connected → cancel this lake and retry

    // STEP 4: Place the church (1)
    churchPos = random position near center, not in spawn zones
    PLACE church (4×3) at churchPos

    // STEP 5: Place mines (2)
    FOR i = 1 to 2:
        minePos = random position, distance >= 10 from any existing public building
        PLACE mine (6×6) at minePos

    // STEP 6: Place farms (3)
    FOR i = 1 to 3:
        farmPos = random position, distance >= 10 from any existing public building
        PLACE farm (4×3) at farmPos

    // STEP 7: Place initial pawns
    playerPos = random position in the left 25% of the circle
    aiPos = random position in the right 25% of the circle
    // Ensure positions are on grass and accessible

    RETURN grid, playerPos, aiPos
```

**Management of 25% left / 25% right:**

The circle is centered at `(radius, radius)`. The left 25% corresponds to cells where `x < radius - radius/2` (i.e. `x < radius * 0.5`). The right 25%: `x > radius + radius/2` (i.e. `x > radius * 1.5`). Only cells inside the circle are considered.

---

### 9. Cell and Terrain System

| Terrain | Texture       | Traversable | Blocks LoS | Notes |
|---------|---------------|-------------|------------|-------|
| Grass   | `grass.png`   | Yes         | No         | Default terrain |
| Dirt    | `dirt.png`    | Yes         | No         | Purely decorative |
| Water   | `water.png`  | **No**      | **Yes**    | Impassable by any piece |
| Void    | (not rendered)| No          | —          | Outside map |

**Buildings on cells:** When a building is placed, the cells under its footprint keep their original terrain but point to the `Building`. Rendering displays the building texture on top. Building cells are traversable (except walls).

---

### 10. Pieces (Units) System

**Immutable Identity:** Each piece has a unique `id` assigned at creation and never changed. When a piece is upgraded, only its `type` changes. Its position, XP, and ID remain identical.

**Initial King:**

- At the start of the game, each kingdom has one King.
- The King can recruit more pieces via a Barracks.
- If the King is killed → **checkmate / game over**.

**Type Transitions:**

```
Pawn (level 0)
├── → Knight (level 1)  [XP >= 100 + gold cost]
├── → Bishop (level 1)  [XP >= 100 + gold cost]
│
Knight / Bishop (level 1)
└── → Rook (level 2)    [XP >= 300 + gold cost]

Rook (level 2) → (blocked, no further upgrade)

Queen: obtained only via marriage (king + bishop + pawn in church)
King: automatic transformation of the initial pawn
```

---

### 11. Buildings System

**Public Buildings:**

| Building | Size   | Number per map | Functionality |
|----------|--------|----------------|---------------|
| Church   | 4×3    | 1              | Marriage location (queen creation) |
| Mine     | 6×6    | 2              | 10 gold/turn per occupied cell |
| Farm     | 4×3    | 3              | 5 gold/turn per occupied cell |

**Public Building Rules:**

- **Indestructible.**
- Belong to no one. A kingdom **exploits** them by placing pieces on them.
- If pieces from **both kingdoms** are present on the same public building → the building **produces nothing for anyone** (contested, crossed swords icon).
- If only one kingdom is present → it exploits the building (shield icon of the kingdom's color).
- Pieces **pass through** public buildings without issue.

**Private Buildings:**

| Building     | Size   | Cost     | HP/cell | Constructible | Blocking |
|--------------|--------|----------|---------|---------------|----------|
| Barracks     | 4×2    | 50 gold  | 1       | Yes (king)    | No       |
| Wood Wall    | 1×1    | 20 gold  | 1       | Yes (king)    | **Yes**  |
| Stone Wall   | 1×1    | 40 gold  | 3       | Yes (king)    | **Yes**  |
| Arena        | ~9 cells | To define | To define | Yes (king)  | No       |

**Construction:**

- Only the **king** can build.
- Construction must be done in the **direct periphery** of the king (squares adjacent to the king touching the building's footprint).
- Maximum **1 construction per turn**.
- The building is placed **entirely** at once (not cell by cell).

**Destruction:**

- Private building cells are destroyed individually.
- A destroyed cell is **grayed out** visually.
- Stone wall: graying proportional to lost HP.
- When all cells of a building are at 0 HP → the building **disappears** entirely.
- Buildings do not repair themselves.

**Barracks — Production:**

- A barracks produces only **one piece at a time**.
- The player chooses the piece type.
- The turn counter decreases by 1 on each commit.
- When the counter reaches 0 → the piece appears on the **nearest free adjacent square** to the barracks.
- If the barracks is destroyed during production → production canceled, gold lost.
- Ongoing production cannot be canceled.

---

### 12. Experience and Upgrade System

**XP Sources:**

| Action                        | XP Gained |
|-------------------------------|-----------|
| Kill a pawn                   | 20        |
| Kill a knight                 | 50        |
| Kill a bishop                 | 50        |
| Kill a rook                   | 100       |
| Kill a queen                  | 300       |
| Destroy a block (wall or barracks cell) | 10 |
| Per turn spent on an arena square | 10    |

**Upgrade Thresholds:**

| Transition               | XP Required | Gold Cost |
|--------------------------|-------------|-----------|
| Pawn → Knight or Bishop  | ≥ 100       | To define |
| Knight/Bishop → Rook     | ≥ 300       | To define |

**Rules:**

- XP **does not reset** after an upgrade. It continues to accumulate with no cap.
- Upgrade **is not mandatory** when the threshold is reached. The player chooses when to upgrade.
- The queen and king accumulate XP but it currently has no use.
- Upgrading costs **gold in addition to the required XP**.

---

### 13. Economic System

**Income:**

- **Mine**: 10 gold/turn per cell occupied by a kingdom piece.
- **Farm**: 5 gold/turn per cell occupied.
- **Condition**: the public building must not be contested (no enemy piece on it).
- Income is credited on **turn consumption** ("Play" button).

**Expenses:**

- Building construction.
- Unit recruitment (in barracks).
- Upgrades.

**No maintenance**: no recurring cost for buildings or pieces.

**Early Game Economic Cycle:**

```
Turn 1+  : King moves toward a mine or farm.
Turn N   : King is on a mine cell → +10 gold/turn.
Turn N+5 : 50 gold accumulated → build a barracks (King must be adjacent).
Turn N+5 : Start pawn production (2 turns).
Turn N+7 : Pawn produced.
...
```

---

### 14. Turn-Based System

**Strict Alternation:** Player (White) → AI (Black) → Player → AI → ...

**Actions per Turn (per kingdom):**

| Category              | Limit |
|-----------------------|-------|
| Movement (piece or formation) | 1     |
| Construction (building) | 1     |
| Production (start)    | 1     |
| Marriage              | 1     |

All categories **can** be performed in the **same turn**. They do not exclude each other.

**No time limit:** The player can think as long as they want.

**No phase order:** Everything is resolved **simultaneously** on turn consumption.

**Preview before Commit:**

- Movement is displayed in **transparency** (ghost of the piece at destination + arrow from origin).
- Construction is displayed in **transparent overlay** at the planned location.
- The player sees the future state before confirming.

**Reset Button:** Cancels **all** pending actions. Returns to the start-of-turn state. No individual undo.

---

### 15. Formations System

**Creation:**

- Manual. When adjacent pieces are selected, a "Create Formation" button appears.
- Pieces must be **adjacent** (side by side, no empty square between them).

**Behavior:**

- The formation behaves as **a single piece**.
- Movement is a **translation**: all pieces move by the same vector.
- Relative positioning of pieces is preserved.
- Selecting a piece in a formation selects **the entire formation**.
- Formation movement follows the movement rules of the **component piece type**.

**Dissolution:** The same button allows **dissolving** the formation.

**Open Questions to Decide During Implementation:**

- Mixed formations (different types): what movement pattern?
- If movement brings a formation piece onto an obstacle?
- Maximum number of pieces per formation?
- 4-direction or 8-direction adjacency?

---

### 16. Check/Checkmate System

**Check:**

- A king is in **check** if at least one enemy piece can reach it via a legal move.
- When the king is in check, the player must **obligatorily** get it out of check (move the king, capture the threatening piece, or interpose a piece).

**Checkmate:**

- The king is in checkmate if:
  1. It is in check.
  2. It cannot flee to any safe square.
  3. No allied piece can capture the attacker.
  4. No allied piece can block the attack (except knight, which cannot be blocked).

**Implementation:**

```cpp
bool CheckSystem::isCheckmate(KingdomId id, const Board& board, const GameConfig& config) {
    Piece* king = getKing(id, board);
    if (!king) return true;  // King dead = game over

    // If not in check, no checkmate
    if (!isInCheck(id, board, config)) return false;

    // Try all possible moves of all kingdom pieces
    // If at least one move removes the king from check → not checkmate
    for (const Piece& piece : getKingdomPieces(id, board)) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& move : moves) {
            // Simulate the move
            Board simBoard = board;  // Copy
            applyMove(simBoard, piece.id, move);
            if (!isInCheck(id, simBoard, config)) {
                return false;  // At least one move saves the king
            }
        }
    }
    return true;  // No move saves the king
}
```

---

### 17. Artificial Intelligence

**Type:** Symbolic AI (rule-based), no machine learning.

**Principles:**

- The AI has **exactly the same powers** as the player. Same API, same constraints.
- No cheating, no hidden advantage.
- The AI sees everything (no fog of war).
- The AI calculates its turn **instantly** from the player's point of view.

**Simplified Decision Tree:**

```
IF no income AND not on a farm zone:
    → Move the king/pawn toward the nearest mine/farm

IF enough gold AND no barracks:
    → Build a barracks (adjacent to king)

IF barracks free AND enough gold:
    → Start production

IF a piece can reach a high-value target (queen > rook > bishop/knight > pawn):
    → Priority attack

IF the king is in check:
    → Mandatory survival move

IF marriage conditions are met:
    → Perform marriage

IF a piece reaches the upgrade threshold:
    → Upgrade

IF pieces are idle:
    → Move them to strategic positions (farm, defense, attack)

ELSE:
    → Positional movement (advance toward center / toward enemy)
```

**Position Evaluation:**

The AIEvaluator assigns a **numeric score** to the current position by weighting:

- Number and type of pieces (material).
- Economic income (number of occupied farm cells).
- King safety.
- Territorial control (occupying center / public buildings).
- Owned buildings.
- Imminent threats (pieces in danger).

The AI chooses actions that **maximize its score** or **minimize the enemy's score**.

---

### 18. User Interface

**Main Menu:**

```
┌─────────────────────────────────────┐
│                                     │
│       A  NORMAL  CHESS  GAME        │
│                                     │
│       ┌───────────────────┐         │
│       │  New Game         │         │
│       └───────────────────┘         │
│       ┌───────────────────┐         │
│       │    Continue       │         │
│       └───────────────────┘         │
│                                     │
└─────────────────────────────────────┘
```

- **New Game:** Opens a sub-menu to name the game, then launches it.
- **Continue:** Opens a menu listing existing saves.

**HUD (top bar in game):**

| Element                  | Position       | Detail |
|--------------------------|----------------|--------|
| Turn number              | Left           | "Turn 15" |
| Active player indicator  | Center-left    | King icon + "White's turn" |
| Gold amount              | Center         | "♦ 230" |
| Reset button             | Right          | Resets turn actions |
| Play button              | Far right      | Consumes the turn |

**Toolbar (bottom-left):**

| Tool       | Icon | Behavior |
|------------|------|----------|
| Mouse      | 🖱️   | Default mode. Selects, moves camera |
| Build      | 🔨   | Opens construction panel. Allows placing buildings |
| Log        | 📖   | Opens event log panel (history) |

**Side Panel (right):**

The side panel is **contextual**. It displays content corresponding to the selection or active tool:

- **Selected piece:** Type, XP, range, upgrade possibility, upgrade button.
- **Selected building:** Type, HP per cell, status.
- **Selected barracks:** Troop choice, production status, turns remaining, produce button.
- **Active build tool:** List of constructible buildings + cost, selection, map preview.
- **Log:** Two tabs: allied events, enemy events. Full game history.

**Visual Overlays on the Map:**

| Overlay                  | When                        | Visual |
|--------------------------|-----------------------------|--------|
| Reachable squares        | Piece selected              | Semi-transparent green overlay |
| Movement preview         | Planned move (before commit)| Transparent piece ghost + arrow |
| Construction preview     | Building ready to place (build mode) | Transparent building overlay |
| Destroyed cells          | Building cell at 0 HP       | Cell graying |
| Damaged wall             | Stone wall < 3 HP           | Progressive graying proportional to damage |
| Zone indicator           | Above public buildings      | White / black shield / crossed swords |

---

### 19. Camera and Navigation

**Type:** 100% top-down view (2D, no isometric).

**Controls:**

| Action             | Input |
|--------------------|-------|
| Move camera        | Middle click + drag / drag on empty area |
| Zoom               | Mouse wheel up |
| Unzoom             | Mouse wheel down |
| Center on king     | Spacebar |

**Technical Implementation:**

The camera is an SFML `sf::View` with zoom and movement transformations.

```cpp
class Camera {
public:
    void init(sf::RenderWindow& window);

    void zoom(float factor);
    void pan(sf::Vector2f delta);
    void centerOn(sf::Vector2f worldPos);

    void applyTo(sf::RenderWindow& window) const;
    sf::FloatRect getViewBounds() const;
    sf::Vector2f screenToWorld(sf::Vector2i screenPos) const;
    sf::Vector2i worldToCell(sf::Vector2f worldPos) const;

private:
    sf::View    m_view;
    float       m_zoomLevel;
};
```

**No fog of war:** The entire map is permanently visible.

---

### 20. Save System

**Mechanisms:**

- **Manual:** The player decides when to save.
- **Multiple slots:** Multiple games, each with their own saves.
- **Storage:** JSON in the `saves/` folder.

**When the player quits:**

- Option 1: Quit without saving.
- Option 2: Quit while saving.
- Option 3: Save without quitting.

**Serialized Data:**

- Game name.
- Turn number, active kingdom.
- Complete grid (radius, cell types, circular mask).
- State of each kingdom (gold, pieces, private buildings with HP).
- Public buildings.
- Event log history.
- History of `TurnCommand` queue (base for future replay function).

---

## Part III — Implementation Plan by Phases

### Phase 0 — Project Setup

**Objective:** Have a compilable C++ project with SFML and TGUI.

| #   | Task                          | Detail |
|-----|-------------------------------|--------|
| 0.1 | Install SFML 2.6+             | Download or via vcpkg/conan |
| 0.2 | Install TGUI 1.x              | Compatible with SFML 2.6 |
| 0.3 | Create main `CMakeLists.txt`  | Find SFML, TGUI, configure C++17 flags |
| 0.4 | Create minimal `main.cpp`     | Open an SFML window, display empty background |
| 0.5 | Verify compilation            | Window opens and closes cleanly |
| 0.6 | Create folder structure       | `src/Core`, `src/Board`, etc. + `assets/`, `saves/`, `docs/` |
| 0.7 | Add nlohmann/json             | For config loading and saves |

**Deliverable:** A project that compiles, opens an empty SFML window, and includes TGUI.

---

### Phase 1 — Engine Foundations

**Objective:** Functional game loop with state machine.

| #   | Task                          | Detail |
|-----|-------------------------------|--------|
| 1.1 | Implement `Game`              | Constructor, `run()`, `handleInput()`, `update()`, `render()` |
| 1.2 | Implement `GameState`         | Enum + transitions (MainMenu → Playing → Paused → GameOver) |
| 1.3 | Implement `GameClock`         | Delta time via `sf::Clock` |
| 1.4 | Implement `AssetManager`      | Loading of all textures and fonts |
| 1.5 | Implement `GameConfig`        | Loading of `game_params.json`, typed getters |
| 1.6 | Create `game_params.json`     | All game constants (see Part IV) |

**Deliverable:** Empty running game loop. Assets are loaded. Constants are accessible.

---

### Phase 2 — Map and Rendering

**Objective:** Display the circular map with terrain and camera.

| #   | Task                          | Detail |
|-----|-------------------------------|--------|
| 2.1 | Implement `CellType` and `Cell` | Basic data structures |
| 2.2 | Implement `Board`             | 2D grid, circular mask, accessors |
| 2.3 | Implement `BoardGenerator`    | Complete procedural generation (circle, dirt, water, public buildings, spawns) |
| 2.4 | Implement `Camera`            | `sf::View`, zoom (wheel), pan (middle click + drag) |
| 2.5 | Implement `Renderer::drawBoard()` | Draw only visible cells in the viewport (frustum culling) |
| 2.6 | Integrate into `Game`         | Generate map → display it → navigate with camera |
| 2.7 | Test performance              | Ensure smooth rendering on 1024×1024 map |

**Deliverable:** The circular map is displayed. Free navigation (pan/zoom). Terrains (grass, dirt, water) are visible. Public building locations are marked.

---

### Phase 3 — Pieces and Movement

**Objective:** Place pieces, select them and move them according to chess rules.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 3.1  | Implement `PieceType` and `Piece` | Types, identity, XP, position |
| 3.2  | Implement `Kingdom`           | Basic data structure (gold, pieces, buildings) |
| 3.3  | Implement `KingdomId`         | Enum White / Black |
| 3.4  | Place initial Kings           | One white King and one black King at generated positions |
| 3.5  | Implement `Renderer::drawPieces()` | Draw piece sprites on the map |
| 3.6  | Implement `InputHandler` Select mode | Left click → select piece → green overlay of reachable squares |
| 3.7  | Implement `MovementRules`     | Legal movement calculation for each piece type |
| 3.7.1| — Pawn                        | 4 cardinal directions, 1 square |
| 3.7.2| — Knight                      | L-shape, jumps friendly walls |
| 3.7.3| — Bishop                      | Diagonal, max 8 squares, blocked by walls/water |
| 3.7.4| — Rook                        | Straight lines, max 8 squares, blocked by walls/water |
| 3.7.5| — Queen                       | Diagonal + straight lines, max 8 squares |
| 3.7.6| — King                        | 1 square all directions |
| 3.8  | Implement `OverlayRenderer`   | Display reachable squares in semi-transparent green |
| 3.9  | Implement destination click   | Left click on green square → record Move command |
| 3.10 | Display ghost                 | Movement preview (transparent piece + arrow) before commit |

**Deliverable:** Can select a piece, see its reachable squares, and plan a move (ghost visible).

---

### Phase 4 — Turn-Based System

**Objective:** Functional player → commit → AI → commit cycle.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 4.1  | Implement `TurnCommand`       | Structure (type, data depending on type) |
| 4.2  | Implement `TurnSystem`        | Command queue, validation, commit |
| 4.3  | Implement "Play" button       | TGUI button that triggers `commitTurn()` |
| 4.4  | Implement "Reset" button      | Clear pending commands |
| 4.5  | Implement commit              | Sequential execution of actions: move, build, produce, marry, upgrade |
| 4.6  | Implement alternation         | White → Black → White → ... |
| 4.7  | Stub AI                       | AI does nothing for now (passes turn) |
| 4.8  | Implement `CombatSystem`      | Combat resolution on commit (move onto enemy) |
| 4.9  | Implement kill XP             | Award XP to the eating piece |
| 4.10 | Game over (King killed/checkmated) | Detect and switch to GameOver |

**Deliverable:** Can play complete turns. Movement works. Combat resolves capture. Player/AI alternation is in place (AI passive).

---

### Phase 5 — Buildings and Construction

**Objective:** Build buildings (barracks, walls, arena) and interact with public buildings.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 5.1  | Implement `BuildingType` and `Building` | Types, HP per cell, owner |
| 5.2  | Implement `BuildingFactory`   | Building creation with dimensions and HP from config |
| 5.3  | Display public buildings      | Draw church, mine, farm textures on the map |
| 5.4  | Implement `BuildSystem`       | Validation (king adjacency, budget, free cells) + placement |
| 5.5  | Implement Build tool          | Build mode in InputHandler, side panel with building list |
| 5.6  | Implement construction preview| Transparent building overlay on map before click |
| 5.7  | Implement placement validation| Free cells, no water, king adjacent |
| 5.8  | Implement building destruction| Piece attacks block → damageCellAt() → graying → disappearance |
| 5.9  | Implement walls (blocking)    | Walls block line of sight and passage (except friendly knight) |
| 5.10 | Integrate walls in `MovementRules` | Update movement calculation to account for walls |

**Deliverable:** Can build barracks, walls (wood/stone), arenas. Walls block passage and line of sight. Buildings can be destroyed cell by cell.

---

### Phase 6 — Economy and Resources

**Objective:** Mines and farms generate gold. Expenses are deducted.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 6.1  | Implement `EconomySystem`     | Income calculation on each commit |
| 6.2  | Implement contestation logic  | If two kingdoms on a public building → 0 income |
| 6.3  | Display zone indicators       | White/black shield / crossed swords above public buildings |
| 6.4  | Implement expenses            | Deduct gold on construction and production |
| 6.5  | Display gold in HUD           | Dynamic counter |
| 6.6  | Implement `ProductionSystem`  | Barracks production: start, advance, spawn |
| 6.7  | Implement barracks panel      | Piece type selection, production status, turns remaining |
| 6.8  | Spawn produced pieces         | Appear on nearest free adjacent square after counter reaches 0 |

**Deliverable:** Complete economic cycle works: farm → build → recruit. Zone indicators are visible.

---

### Phase 7 — XP, Upgrades and Marriage

**Objective:** Functional XP system, upgrades, and queen creation.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 7.1  | Implement full `XPSystem`     | XP allocation (kill, block, arena) |
| 7.2  | Implement upgrade             | Threshold check + cost → type change |
| 7.3  | Implement upgrade panel       | In PiecePanel, "Upgrade to knight/bishop/rook" button when threshold reached |
| 7.4  | Implement arena XP            | Automatic award on each commit for pieces on arena |
| 7.5  | Implement `MarriageSystem`    | Validation (king + bishop + pawn in church, king-pawn adjacency, no existing queen) |
| 7.6  | Implement marriage            | Transform pawn into queen on turn consumption |
| 7.7  | Integrate into turn commit    | Arena XP + production + marriage in the commit pipeline |

**Deliverable:** XP accumulates. Pieces can be upgraded. Queen can be created via church marriage.

---

### Phase 8 — TGUI User Interface

**Objective:** Complete and functional UI.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 8.1  | Implement `MainMenuUI`        | Screen: New Game + Continue |
| 8.2  | Implement New Game menu       | Text field to name the game + Launch button |
| 8.3  | Implement Load Game menu      | List of saves + Load button |
| 8.4  | Implement full `HUD`          | Turn, active player, gold, Reset, Play |
| 8.5  | Implement `ToolBar`           | 3 tools: mouse, build, log |
| 8.6  | Implement `PiecePanel`        | XP, type, range, upgrade button |
| 8.7  | Implement `BuildingPanel`     | Type, HP, status |
| 8.8  | Implement `BarracksPanel`     | Piece choice, ongoing production, turns remaining |
| 8.9  | Implement `BuildToolPanel`    | List of constructible buildings + cost |
| 8.10 | Implement `EventLogPanel`     | Two tabs (ally / enemy), full history |
| 8.11 | Implement pause menu (Esc)    | Dark overlay + options (resume, save, quit) |
| 8.12 | Implement Game Over screen    | Victory or defeat message |

**Deliverable:** Main menu, HUD, all side panels, toolbar, pause, game over.

---

### Phase 9 — Artificial Intelligence

**Objective:** Functional AI that plays strategically.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 9.1  | Implement `AIConfig`          | Loading of `ai_params.json` |
| 9.2  | Implement `AIEvaluator`       | Position scoring (material, economy, king safety, territory) |
| 9.3  | Implement `AIStrategyEcon`    | Farm → barracks → production |
| 9.4  | Implement `AIStrategyMove`    | Attack, defense, positioning |
| 9.5  | Implement `AIStrategyBuild`   | Wall, barracks, arena construction |
| 9.6  | Implement `AIStrategySpecial` | Upgrades, marriage, arena |
| 9.7  | Implement `AIController`      | Orchestration: evaluate → decide → produce TurnCommands |
| 9.8  | Integrate AI into the loop    | After player commit, AI plays instantly |
| 9.9  | Test AI                       | Verify AI makes logical choices and respects rules |
| 9.10 | Refine weights                | Iteratively adjust `ai_params.json` |

**Deliverable:** AI plays complete turns. It farms, builds, recruits, attacks, and performs upgrades/marriages.

---

### Phase 10 — Formations

**Objective:** Functional formations system.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 10.1 | Implement `FormationSystem`   | Creation, dissolution, adjacency detection |
| 10.2 | Implement formation selection | Click on a piece in formation → select entire formation |
| 10.3 | Implement formation movement  | Translate the entire group |
| 10.4 | Display formations            | Visual overlay connecting pieces of the same formation |
| 10.5 | Create/dissolve formation button | In PiecePanel when piece is adjacent to others |

**Deliverable:** Formations work: creation, grouped movement, dissolution.

---

### Phase 11 — Saving and Persistence

**Objective:** Save and load games.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 11.1 | Implement `SaveData`          | Complete game state structure |
| 11.2 | Implement JSON serialization  | All data → JSON (nlohmann/json) |
| 11.3 | Implement deserialization     | JSON → game state reconstruction |
| 11.4 | Implement `SaveManager`       | `save()`, `load()`, `listSaves()`, `deleteSave()` |
| 11.5 | Integrate saves into UI       | Save button (pause), load menu (main menu) |
| 11.6 | Store historical TurnCommands | Base for future replay |
| 11.7 | Test persistence              | Save → quit → reload → identical state |

**Deliverable:** Player can save, quit, and reload games.

---

### Phase 12 — Polish and Balancing

**Objective:** Refine the experience and balance gameplay.

| #    | Task                          | Detail |
|------|-------------------------------|--------|
| 12.1 | Test complete cycle           | Game from start to checkmate |
| 12.2 | Adjust constants              | Modify `game_params.json` (costs, income, XP) |
| 12.3 | Adjust AI                     | Modify `ai_params.json` (weights, thresholds, randomness) |
| 12.4 | Verify check/mate             | Test all edge cases |
| 12.5 | Verify edge cases             | Initial pawn killed, barracks destroyed during production, contested zones, etc. |
| 12.6 | Optimize rendering            | Profile and optimize if necessary |
| 12.7 | Fix bugs                      | Fix issues identified during testing |
| 12.8 | Verify save system            | No data corruption |
| 12.9 | Define missing values         | Recruitment costs per type, upgrade costs, arena cost, arena size |

**Deliverable:** Complete, stable, balanced game.

---

## Part IV — Game Constants Table

All values are stored in `config/game_params.json` and accessible via `GameConfig`.

### Map

| Parameter                      | Value     | Notes |
|--------------------------------|-----------|-------|
| Map radius (cells)             | 512       | Configurable |
| Cell size (px)                 | 16×16     | |
| Number of mines                | 2         | |
| Number of farms                | 3         | |
| Min. distance between public buildings | 10 cells | |
| Player spawn zone              | 25% left  | |
| AI spawn zone                  | 25% right | |

### Economy

| Parameter                      | Value     | Notes |
|--------------------------------|-----------|-------|
| Starting gold                  | 0         | |
| Mine: income/turn/cell         | 10 gold   | |
| Farm: income/turn/cell         | 5 gold    | |
| Barracks cost                  | 50 gold   | |
| Wood wall cost                 | 20 gold   | |
| Stone wall cost                | 40 gold   | |
| Arena cost                     | **To define** | |
| Pawn recruitment cost          | **To define** | |
| Knight recruitment cost        | **To define** | |
| Bishop recruitment cost        | **To define** | |
| Rook recruitment cost          | **To define** | |
| Pawn→knight/bishop upgrade cost| **To define** | |
| Knight/bishop→rook upgrade cost| **To define** | |

### Production

| Parameter          | Value |
|--------------------|-------|
| Pawn (level 0)     | 2 turns |
| Knight (level 1)   | 4 turns |
| Bishop (level 1)   | 4 turns |
| Rook (level 2)     | 6 turns |

### Combat and HP

| Parameter                  | Value |
|----------------------------|-------|
| Global max range (bishop/rook/queen) | 8 squares |
| Wood wall HP               | 1     |
| Stone wall HP              | 3     |
| Barracks cell HP           | 1     |

### Experience

| Parameter                  | Value |
|----------------------------|-------|
| XP kill pawn               | 20    |
| XP kill knight             | 50    |
| XP kill bishop             | 50    |
| XP kill rook               | 100   |
| XP kill queen              | 300   |
| XP block destruction       | 10    |
| XP arena per turn per piece| 10    |
| Upgrade threshold pawn → knight/bishop | 100 XP |
| Upgrade threshold knight/bishop → rook | 300 XP |

### Buildings (Dimensions)

| Building     | Width | Height |
|--------------|-------|--------|
| Barracks     | 4     | 2      |
| Church       | 4     | 3      |
| Mine         | 6     | 6      |
| Farm         | 4     | 3      |
| Arena        | ~9 cells | **To specify** |
| Wood Wall    | 1     | 1      |
| Stone Wall   | 1     | 1      |

### Limits per Turn

| Action                    | Limit |
|---------------------------|-------|
| Movement                  | 1 per turn |
| Construction              | 1 per turn |
| Production (start)        | 1 per turn |
| Marriage                  | 1 per turn |

---

## Part V — Best Practices and Conventions

### C++ Code

- **Standard:** C++17 minimum.
- **Naming:** PascalCase for classes/enums, camelCase for functions/variables, UPPER_SNAKE_CASE for preprocessor constants.
- **Member prefix:** `m_` for private class members.
- **No magic numbers:** Every numeric value goes through `GameConfig`.
- **RAII:** Use constructors and destructors for resource management.
- **Smart pointers:** Use `std::unique_ptr` and `std::shared_ptr` when ownership is not trivial.
- **Const correctness:** Mark `const` everything that does not modify state.
- **No global variables.** Global state goes through Game and its sub-systems.

### Architecture

- **No Render/UI system modifies the game state.** Read-only.
- **No logic system draws anything.**
- **The AI uses exactly the same API as the player.** It produces `TurnCommand`.
- **Systems are stateless**: they receive references to data and modify them, but do not store significant internal state.
- **Test Systems independently.** Create unit tests for MovementRules, CombatSystem, CheckSystem, etc.

### Performance

- **Frustum culling mandatory:** Draw only cells visible in the viewport.
- **Single asset loading:** All textures are loaded at startup and shared via `AssetManager`.
- **No per-frame reallocation:** Pre-allocate data structures. Reuse buffers.
- **Profile before optimizing:** The bottleneck will be in logic (AI, pathfinding), not on the GPU.

### Maintainability

- **One file per class** (header + source).
- **Separate compilation:** Lightweight headers (forward declarations), implementation in .cpp files.
- **Externalized configuration:** All constants in editable JSON files without recompilation.
- **Event log:** Use `EventLog` to trace everything that happens (useful for debug AND for the player).