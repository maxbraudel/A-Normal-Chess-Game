# A Normal Chess Game — Plan de Développement Complet

---

## Table des matières

- [A Normal Chess Game — Plan de Développement Complet](#a-normal-chess-game--plan-de-développement-complet)
  - [Table des matières](#table-des-matières)
  - [Partie I — Architecture du projet](#partie-i--architecture-du-projet)
    - [1. Stack technique](#1-stack-technique)
    - [2. Organisation du projet (arborescence)](#2-organisation-du-projet-arborescence)
    - [3. Principes architecturaux](#3-principes-architecturaux)
      - [3.1 Séparation stricte des responsabilités](#31-séparation-stricte-des-responsabilités)
      - [3.2 Flux de données](#32-flux-de-données)
      - [3.3 Le pattern Commande](#33-le-pattern-commande)
      - [3.4 Neutralité du code entre les deux royaumes](#34-neutralité-du-code-entre-les-deux-royaumes)
      - [3.5 Données vs logique](#35-données-vs-logique)
    - [4. Architecture des modules (vue d'ensemble)](#4-architecture-des-modules-vue-densemble)
    - [5. Détail de chaque module](#5-détail-de-chaque-module)
      - [5.1 Core — Le cœur du moteur](#51-core--le-cœur-du-moteur)
      - [5.2 Config — Paramétrage global](#52-config--paramétrage-global)
      - [5.3 Board — Carte et terrain](#53-board--carte-et-terrain)
      - [5.4 Units — Pièces d'échecs](#54-units--pièces-déchecs)
      - [5.5 Buildings — Bâtiments](#55-buildings--bâtiments)
      - [5.6 Systems — Règles du jeu](#56-systems--règles-du-jeu)
        - [TurnSystem](#turnsystem)
        - [CombatSystem](#combatsystem)
        - [EconomySystem](#economysystem)
        - [XPSystem](#xpsystem)
        - [BuildSystem](#buildsystem)
        - [ProductionSystem](#productionsystem)
        - [MarriageSystem](#marriagesystem)
        - [CheckSystem](#checksystem)
        - [EventLog](#eventlog)
      - [5.7 Kingdom — Royaumes](#57-kingdom--royaumes)
      - [5.8 AI — Intelligence artificielle](#58-ai--intelligence-artificielle)
      - [5.9 UI — Interface utilisateur (TGUI)](#59-ui--interface-utilisateur-tgui)
      - [5.10 Render — Rendu graphique (SFML)](#510-render--rendu-graphique-sfml)
      - [5.11 Input — Gestion des entrées](#511-input--gestion-des-entrées)
      - [5.12 Save — Sauvegarde et persistance](#512-save--sauvegarde-et-persistance)
      - [5.13 Assets — Gestion des ressources](#513-assets--gestion-des-ressources)
    - [6. Boucle de jeu principale](#6-boucle-de-jeu-principale)
    - [7. Diagramme de flux d'un tour](#7-diagramme-de-flux-dun-tour)
  - [Partie II — Spécifications techniques détaillées](#partie-ii--spécifications-techniques-détaillées)
    - [8. La carte — Génération procédurale circulaire](#8-la-carte--génération-procédurale-circulaire)
    - [9. Système de cellules et terrains](#9-système-de-cellules-et-terrains)
    - [10. Système de pièces (Units)](#10-système-de-pièces-units)
    - [11. Système de bâtiments](#11-système-de-bâtiments)
    - [12. Système d'expérience et d'upgrade](#12-système-dexpérience-et-dupgrade)
    - [13. Système économique](#13-système-économique)
    - [14. Système de tour par tour](#14-système-de-tour-par-tour)
    - [15. Système de formations](#15-système-de-formations)
    - [16. Système de check/checkmate](#16-système-de-checkcheckmate)
    - [17. Intelligence artificielle](#17-intelligence-artificielle)
    - [18. Interface utilisateur](#18-interface-utilisateur)
    - [19. Caméra et navigation](#19-caméra-et-navigation)
    - [20. Système de sauvegarde](#20-système-de-sauvegarde)
  - [Partie III — Plan d'implémentation par phases](#partie-iii--plan-dimplémentation-par-phases)
    - [Phase 0 — Setup du projet](#phase-0--setup-du-projet)
    - [Phase 1 — Fondations du moteur](#phase-1--fondations-du-moteur)
    - [Phase 2 — Carte et rendu](#phase-2--carte-et-rendu)
    - [Phase 3 — Pièces et déplacement](#phase-3--pièces-et-déplacement)
    - [Phase 4 — Système de tour par tour](#phase-4--système-de-tour-par-tour)
    - [Phase 5 — Bâtiments et construction](#phase-5--bâtiments-et-construction)
    - [Phase 6 — Économie et ressources](#phase-6--économie-et-ressources)
    - [Phase 7 — XP, upgrades et mariage](#phase-7--xp-upgrades-et-mariage)
    - [Phase 8 — Interface utilisateur TGUI](#phase-8--interface-utilisateur-tgui)
    - [Phase 9 — Intelligence artificielle](#phase-9--intelligence-artificielle)
    - [Phase 10 — Formations](#phase-10--formations)
    - [Phase 11 — Sauvegarde et persistance](#phase-11--sauvegarde-et-persistance)
    - [Phase 12 — Polish et équilibrage](#phase-12--polish-et-équilibrage)
  - [Partie IV — Tableau des constantes de jeu](#partie-iv--tableau-des-constantes-de-jeu)
    - [Carte](#carte)
    - [Économie](#économie)
    - [Production](#production)
    - [Combat et PV](#combat-et-pv)
    - [Expérience](#expérience)
    - [Bâtiments (dimensions)](#bâtiments-dimensions)
    - [Limites par tour](#limites-par-tour)
  - [Partie V — Bonnes pratiques et conventions](#partie-v--bonnes-pratiques-et-conventions)
    - [Code C++](#code-c)
    - [Architecture](#architecture)
    - [Performance](#performance)
    - [Maintenabilité](#maintenabilité)

---

## Partie I — Architecture du projet

### 1. Stack technique

| Composant | Technologie | Rôle |
|-----------|-------------|------|
| **Langage** | C++ (standard C++17 minimum) | Langage principal du moteur et de toute la logique |
| **Rendu 2D** | SFML 2.6+ | Fenêtre, rendu des sprites, gestion des événements système |
| **UI joueur** | TGUI 1.x (pour SFML) | Widgets d'interface : panneaux, boutons, labels, listes |
| **UI debug** | Dear ImGui (optionnel) | Outils de développeur : inspecteur, tweaking de variables |
| **Build** | CMake 3.20+ | Système de build cross-platform |
| **Sérialisation** | JSON (nlohmann/json ou similaire) | Fichiers de sauvegarde et fichiers de configuration |

---

### 2. Organisation du projet (arborescence)

```
A Normal Chess Game/
│
├── CMakeLists.txt                    # Build principal
├── README.md
│
├── assets/                           # Ressources du jeu (runtime)
│   ├── textures/
│   │   ├── cells/                    # Textures de terrain (16×16 px)
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
│   │   │   ├── white/                # Sprites blancs (pawn, bishop, knight, rook, queen, king)
│   │   │   └── black/                # Sprites noirs
│   │   └── ui/
│   │       ├── crossed_swords.png
│   │       ├── shield_black.png
│   │       └── shield_white.png
│   ├── fonts/
│   └── config/
│       ├── game_params.json          # Constantes de gameplay (voir Partie IV)
│       └── ai_params.json            # Paramétrage de l'IA
│
├── src/                              # Code source
│   ├── main.cpp                      # Point d'entrée
│   │
│   ├── Core/
│   │   ├── Game.hpp / Game.cpp                 # Classe Game (machine à états)
│   │   ├── GameState.hpp                       # Enum des états (Menu, Playing, Paused, GameOver)
│   │   └── GameClock.hpp / GameClock.cpp       # Gestion du delta time
│   │
│   ├── Config/
│   │   ├── GameConfig.hpp / GameConfig.cpp     # Chargement et accès aux paramètres de jeu
│   │   └── AIConfig.hpp / AIConfig.cpp         # Chargement et accès aux paramètres IA
│   │
│   ├── Board/
│   │   ├── Cell.hpp / Cell.cpp                 # Une cellule de la carte
│   │   ├── CellType.hpp                        # Enum des types de cellule
│   │   ├── Board.hpp / Board.cpp               # La carte entière (grille 2D + masque circulaire)
│   │   └── BoardGenerator.hpp / BoardGenerator.cpp  # Génération procédurale
│   │
│   ├── Units/
│   │   ├── PieceType.hpp                       # Enum (Pawn, Knight, Bishop, Rook, Queen, King)
│   │   ├── Piece.hpp / Piece.cpp               # Entité pièce (identité, XP, type, position)
│   │   ├── PieceFactory.hpp / PieceFactory.cpp # Création de pièces
│   │   └── MovementRules.hpp / MovementRules.cpp  # Calcul des mouvements légaux
│   │
│   ├── Buildings/
│   │   ├── BuildingType.hpp                    # Enum (Barracks, WoodWall, StoneWall, Arena, Church, Mine, Farm)
│   │   ├── Building.hpp / Building.cpp         # Entité bâtiment (type, cases, PV par case, propriétaire)
│   │   └── BuildingFactory.hpp / BuildingFactory.cpp  # Création de bâtiments
│   │
│   ├── Kingdom/
│   │   ├── Kingdom.hpp / Kingdom.cpp           # Royaume (écus, liste pièces, liste bâtiments, roi)
│   │   └── KingdomId.hpp                       # Enum (White, Black)
│   │
│   ├── Systems/
│   │   ├── TurnSystem.hpp / TurnSystem.cpp           # Gestion des tours (actions, consommation)
│   │   ├── CombatSystem.hpp / CombatSystem.cpp       # Résolution des combats (pièce mange pièce/bloc)
│   │   ├── EconomySystem.hpp / EconomySystem.cpp     # Revenus des mines/champs, dépenses
│   │   ├── XPSystem.hpp / XPSystem.cpp               # Attribution d'XP, upgrades
│   │   ├── BuildSystem.hpp / BuildSystem.cpp         # Construction de bâtiments, validation
│   │   ├── ProductionSystem.hpp / ProductionSystem.cpp  # Fabrication en caserne
│   │   ├── MarriageSystem.hpp / MarriageSystem.cpp   # Système de mariage à l'église
│   │   ├── FormationSystem.hpp / FormationSystem.cpp # Gestion des formations
│   │   ├── CheckSystem.hpp / CheckSystem.cpp         # Détection échec / échec et mat
│   │   └── EventLog.hpp / EventLog.cpp               # Journal des événements
│   │
│   ├── AI/
│   │   ├── AIController.hpp / AIController.cpp       # Contrôleur principal de l'IA
│   │   ├── AIEvaluator.hpp / AIEvaluator.cpp         # Évaluation de la situation du plateau
│   │   ├── AIStrategyMove.hpp / AIStrategyMove.cpp   # Stratégie de déplacement / attaque
│   │   ├── AIStrategyBuild.hpp / AIStrategyBuild.cpp # Stratégie de construction
│   │   ├── AIStrategyEcon.hpp / AIStrategyEcon.cpp   # Stratégie économique (farm, production)
│   │   └── AIStrategySpecial.hpp / AIStrategySpecial.cpp  # Mariage, arène, upgrades
│   │
│   ├── UI/
│   │   ├── UIManager.hpp / UIManager.cpp             # Orchestrateur de l'UI (TGUI)
│   │   ├── MainMenuUI.hpp / MainMenuUI.cpp           # Écran du menu principal
│   │   ├── HUD.hpp / HUD.cpp                         # HUD en jeu (tour, écus, boutons)
│   │   ├── PiecePanel.hpp / PiecePanel.cpp           # Panneau latéral pièce sélectionnée
│   │   ├── BuildingPanel.hpp / BuildingPanel.cpp     # Panneau latéral bâtiment sélectionné
│   │   ├── BarracksPanel.hpp / BarracksPanel.cpp     # Interface de la caserne
│   │   ├── BuildToolPanel.hpp / BuildToolPanel.cpp   # Panneau outil construction
│   │   ├── EventLogPanel.hpp / EventLogPanel.cpp     # Panneau journal d'événements
│   │   └── ToolBar.hpp / ToolBar.cpp                 # Barre d'outils (souris, marteau, journal)
│   │
│   ├── Render/
│   │   ├── Renderer.hpp / Renderer.cpp               # Rendu principal (carte, pièces, bâtiments)
│   │   ├── Camera.hpp / Camera.cpp                   # Vue SFML (zoom, pan, centrage)
│   │   ├── SpriteSheet.hpp / SpriteSheet.cpp         # (Optionnel) Atlas de sprites
│   │   └── OverlayRenderer.hpp / OverlayRenderer.cpp # Overlays (cases accessibles, preview, drapeaux)
│   │
│   ├── Input/
│   │   ├── InputHandler.hpp / InputHandler.cpp       # Dispatching des événements SFML
│   │   └── ToolState.hpp                             # Enum (Select, Build, Journal)
│   │
│   ├── Assets/
│   │   └── AssetManager.hpp / AssetManager.cpp       # Chargement centralisé des textures/fonts
│   │
│   └── Save/
│       ├── SaveManager.hpp / SaveManager.cpp         # Sérialisation / désérialisation de l'état
│       └── SaveData.hpp                              # Structure de données de sauvegarde
│
├── saves/                            # Répertoire des sauvegardes (runtime)
│
├── docs/                             # Documentation
│   ├── BRAINSTORMING_RAPPORT.md
│   ├── QUESTIONS_IMPLEMENTATION.md
│   ├── REPONSES_IMPLEMENTATION.md
│   ├── choix techniques internet.md
│   └── PLAN_DEVELOPPEMENT.md         # Ce fichier
│
└── tests/                            # Tests unitaires (optionnel mais recommandé)
    ├── test_movement.cpp
    ├── test_combat.cpp
    ├── test_economy.cpp
    ├── test_xp.cpp
    ├── test_check.cpp
    └── test_board_gen.cpp
```

---

### 3. Principes architecturaux

#### 3.1 Séparation stricte des responsabilités

Le code est organisé en **couches indépendantes** qui ne se connaissent que par des interfaces minimales :

| Couche | Connaît | Ne connaît PAS |
|--------|---------|-----------------|
| **Core** | Toutes les couches (orchestre) | — |
| **Board** | Cell, CellType | Units, Buildings, UI, Render |
| **Units** | PieceType, position | Board (directement), UI, Render |
| **Buildings** | BuildingType, position, PV | Units, UI, Render |
| **Kingdom** | Units, Buildings, écus | UI, Render |
| **Systems** | Board, Units, Buildings, Kingdom, Config | UI, Render |
| **AI** | Systems (via la même API que le joueur) | UI, Render |
| **UI** | Données à afficher (lecture seule) | Logique de jeu (ne modifie rien directement) |
| **Render** | Board, Units, Buildings (lecture seule) | Logique de jeu |
| **Input** | Events SFML → commandes | Logique de jeu (transmet des commandes) |

#### 3.2 Flux de données

```
Entrées joueur (souris/clavier)
       │
       ▼
  InputHandler  ──→  Commande (enum/struct)
       │
       ▼
  TurnSystem  ──→  valide la commande
       │              ──→  enregistre l'action dans le tour en cours
       │
       ▼
  [Bouton "Jouer" pressé]
       │
       ▼
  TurnSystem::commit()  ──→  exécute toutes les actions du tour
       │                      ──→  CombatSystem, EconomySystem, XPSystem, etc.
       │
       ▼
  AIController::playTurn()  ──→  même API que le joueur
       │
       ▼
  Renderer::draw()  ──→  lit l'état du Board, Units, Buildings
       │
       ▼
  UIManager::update()  ──→  met à jour les panneaux TGUI
```

#### 3.3 Le pattern Commande

Chaque action du joueur ou de l'IA est encodée comme une **commande** (move, build, produce, upgrade, marry, formGroup, breakGroup, resetTurn). Cela permet :

- De les **stocker** avant commit (preview en transparence).
- De les **annuler** (bouton Reset = vider la file de commandes).
- De les **rejouer** (base pour un futur système de replay).
- De faire jouer l'**IA avec la même API** (elle produit des commandes).

```cpp
struct TurnCommand {
    enum Type { Move, Build, Produce, Upgrade, Marry, FormGroup, BreakGroup };
    Type type;
    // Données spécifiques selon le type (union ou variant)
};
```

#### 3.4 Neutralité du code entre les deux royaumes

Le code de jeu est **100% neutre** : la même logique sert pour le joueur blanc et l'IA noire. Un `Kingdom` a un `KingdomId` (White ou Black), et les `Systems` ne font jamais de if/else basé sur "est-ce le joueur ou l'IA". La seule différence est la **source des commandes** : InputHandler pour le joueur, AIController pour l'IA.

#### 3.5 Données vs logique

- Les **données** (état du jeu) sont dans Board, Units, Buildings, Kingdom.
- La **logique** (règles du jeu) est dans Systems.
- L'**affichage** est dans Render + UI.
- Les **entrées** sont dans Input.

Aucun système de rendu ne modifie l'état du jeu. Aucun système de jeu ne dessine quoi que ce soit.

---

### 4. Architecture des modules (vue d'ensemble)

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
   │ Handler  │    │  (logique)   │    │  + UI     │
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

### 5. Détail de chaque module

#### 5.1 Core — Le cœur du moteur

**Fichiers :** `Game.hpp/.cpp`, `GameState.hpp`, `GameClock.hpp/.cpp`

**Responsabilités :**

- Machine à états globale du jeu (Menu → Playing → Paused → GameOver).
- Instanciation de tous les sous-systèmes.
- Boucle de jeu principale : `handleInput()` → `update()` → `render()`.
- Gestion du delta time pour des animations futures.

**Classe `Game` :**

```cpp
class Game {
public:
    Game();
    void run();     // Boucle principale

private:
    void handleInput();
    void update();
    void render();

    sf::RenderWindow    m_window;
    tgui::Gui           m_gui;
    GameState           m_state;

    // Sous-systèmes (ownership)
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

**`GameState` :**

```cpp
enum class GameState {
    MainMenu,       // Menu principal (Nouvelle partie / Continuer)
    NewGameMenu,    // Menu de création de partie (nommer la partie)
    LoadGameMenu,   // Menu de chargement de sauvegarde
    Playing,        // En jeu
    Paused,         // Pause (Échap)
    GameOver        // Fin de partie (échec et mat)
};
```

---

#### 5.2 Config — Paramétrage global

**Fichiers :** `GameConfig.hpp/.cpp`, `AIConfig.hpp/.cpp`

**Responsabilités :**

- Charger les constantes de gameplay depuis `config/game_params.json`.
- Charger les paramètres IA depuis `config/ai_params.json`.
- Exposer des getters typés pour chaque paramètre.
- Permettre au développeur de modifier les valeurs sans recompiler.

**Contenu de `game_params.json` :**

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

**Principe :** Toute valeur numérique du jeu est lue depuis ce fichier. Aucune constante magique dans le code source. Le fichier `game_params.json` est la **source de vérité** pour l'équilibrage.

---

#### 5.3 Board — Carte et terrain

**Fichiers :** `Cell.hpp/.cpp`, `CellType.hpp`, `Board.hpp/.cpp`, `BoardGenerator.hpp/.cpp`

**`CellType` :**

```cpp
enum class CellType {
    Void,           // Hors de la carte (en dehors du cercle)
    Grass,          // Herbe (traversable, par défaut)
    Dirt,           // Terre (traversable, décoratif)
    Water           // Eau (infranchissable)
};
```

**`Cell` :**

```cpp
struct Cell {
    CellType        type;             // Type de terrain
    Building*       building;         // Pointeur vers le bâtiment (nullptr si aucun)
    Piece*          piece;            // Pointeur vers la pièce présente (nullptr si aucune)
    bool            isInCircle;       // Fait partie de la carte jouable
    sf::Vector2i    position;         // Coordonnées (col, row)
};
```

**`Board` :**

```cpp
class Board {
public:
    Board();
    void generate(const GameConfig& config);    // Génération procédurale

    Cell& getCell(int x, int y);
    const Cell& getCell(int x, int y) const;
    bool isInBounds(int x, int y) const;
    bool isTraversable(int x, int y, KingdomId mover) const;
    int  getRadius() const;
    int  getDiameter() const;

    // Itération sur les cellules valides
    std::vector<sf::Vector2i> getAllValidCells() const;

private:
    int m_radius;
    int m_diameter;     // = 2 * radius
    std::vector<std::vector<Cell>> m_grid;   // m_diameter × m_diameter
};
```

**`BoardGenerator` — Algorithme de génération :**

1. Créer une grille carrée de `2*radius × 2*radius` cellules, toutes à `CellType::Void`.
2. Appliquer le **masque circulaire** : pour chaque cellule `(x, y)`, calculer la distance au centre `(radius, radius)`. Si `distance <= radius`, la cellule est dans le cercle et devient `CellType::Grass`, sinon elle reste `Void`.
3. **Pixéliser le cercle** : la forme n'a pas besoin d'être lissée. Le cercle est naturellement pixélisé par la grille.
4. Placer des zones de **terre** (décoratif) aléatoirement par blobs/bruit.
5. Placer des zones d'**eau** (bloquantes) aléatoirement, en s'assurant qu'elles ne bloquent pas totalement le passage.
6. Placer les **bâtiments publics** : 1 église, 2 mines, 3 champs. Respecter la distance minimum de 10 blocs entre bâtiments publics. Les placer de préférence vers le centre/moitié de la carte (pas dans les zones de spawn).
7. Placer les **positions initiales** des deux pions (joueur dans les 25% gauche, IA dans les 25% droite).

---

#### 5.4 Units — Pièces d'échecs

**Fichiers :** `PieceType.hpp`, `Piece.hpp/.cpp`, `PieceFactory.hpp/.cpp`, `MovementRules.hpp/.cpp`

**`PieceType` :**

```cpp
enum class PieceType {
    Pawn,       // Niveau 0
    Knight,     // Niveau 1
    Bishop,     // Niveau 1
    Rook,       // Niveau 2
    Queen,      // Niveau 3 (non fabricable)
    King        // Niveau 4 (unique, non fabricable)
};
```

**`Piece` :**

```cpp
class Piece {
public:
    int             id;             // Identifiant unique (permanent)
    PieceType       type;           // Type actuel (peut changer lors d'une upgrade)
    KingdomId       kingdom;        // Propriétaire
    sf::Vector2i    position;       // Position sur la grille
    int             xp;             // XP accumulée (sans plafond)
    int             formationId;    // -1 si pas dans une formation

    bool canUpgradeTo(PieceType target, const GameConfig& config) const;
    int  getLevel() const;  // 0=Pawn, 1=Knight/Bishop, 2=Rook, 3=Queen, 4=King
};
```

**`MovementRules` :**

Ce module calcule toutes les cases accessibles pour une pièce donnée, en prenant en compte :

- Le type de pièce et ses règles de déplacement classiques des échecs.
- La **portée maximale globale** (8 cases) pour le fou, la tour, et la reine.
- Le blocage par les obstacles : eau, murs (alliés et ennemis).
- Le passage au travers des bâtiments (église, mines, champs, casernes).
- Le cavalier qui peut sauter par-dessus les pièces et les murs **alliés** (mais pas les murs ennemis).
- Les pièces alliées qui bloquent les cases.
- Les pièces ennemies qui sont des cibles d'attaque.

**Règles de déplacement par type :**

| Type | Déplacement | Particularités |
|------|-------------|----------------|
| **Pion** | 1 case en haut, bas, gauche, droite (pas de diagonale) | Pas de 2 cases au 1er coup, pas de prise en passant |
| **Cavalier** | Mouvement en L classique (fixe) | Saute par-dessus pièces et murs alliés, PAS murs ennemis |
| **Fou** | Diagonale, max 8 cases | Bloqué par murs et eau |
| **Tour** | Lignes droites (H/V), max 8 cases | Bloqué par murs et eau |
| **Reine** | Diagonale + lignes droites, max 8 cases | Bloqué par murs et eau |
| **Roi** | 1 case dans toutes directions (y compris diagonales) | Pas de roque |

```cpp
class MovementRules {
public:
    // Retourne toutes les cases accessibles pour une pièce
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

    // Helper : parcours en ligne droite (pour fou, tour, reine)
    static std::vector<sf::Vector2i> traceDirection(
        const Piece& piece, const Board& board,
        int dx, int dy, int maxRange
    );
};
```

---

#### 5.5 Buildings — Bâtiments

**Fichiers :** `BuildingType.hpp`, `Building.hpp/.cpp`, `BuildingFactory.hpp/.cpp`

**`BuildingType` :**

```cpp
enum class BuildingType {
    // Publics (indestructibles, générés)
    Church,
    Mine,
    Farm,
    // Privés (constructibles, destructibles)
    Barracks,
    WoodWall,
    StoneWall,
    Arena
};
```

**`Building` :**

```cpp
class Building {
public:
    int                         id;
    BuildingType                type;
    KingdomId                   owner;      // Neutral pour les publics
    sf::Vector2i                origin;     // Coin supérieur gauche
    int                         width;
    int                         height;
    std::vector<int>            cellHP;     // PV par case (taille = width * height)

    // Caserne uniquement
    bool                        isProducing;
    PieceType                   producingType;
    int                         turnsRemaining;

    bool isPublic() const;
    bool isDestroyed() const;                   // Toutes les cases à 0 PV
    bool isCellDamaged(int localX, int localY) const;
    int  getCellHP(int localX, int localY) const;
    void damageCellAt(int localX, int localY);  // -1 PV
    std::vector<sf::Vector2i> getOccupiedCells() const;
    std::vector<sf::Vector2i> getAdjacentCells(const Board& board) const;
};
```

**Classification :**

| Type | Public ? | Destructible ? | Constructible ? | Bloquant ? |
|------|----------|----------------|-----------------|------------|
| Church | Oui | Non | Non | Non |
| Mine | Oui | Non | Non | Non |
| Farm | Oui | Non | Non | Non |
| Barracks | Non | Oui (1 PV/case) | Oui (roi) | Non |
| WoodWall | Non | Oui (1 PV) | Oui (roi) | **Oui** |
| StoneWall | Non | Oui (3 PV) | Oui (roi) | **Oui** |
| Arena | Non | Oui | Oui (roi) | Non |

---

#### 5.6 Systems — Règles du jeu

Chaque system est un module **stateless** ou presque. Il opère sur les données (Board, Units, Buildings, Kingdom) et applique les règles.

##### TurnSystem

Gère le **cycle de tour**. Responsable de :

- Enregistrer les commandes du joueur actif (move, build, produce, upgrade, marry).
- Valider chaque commande (est-ce légal ?).
- Stocker les commandes en attente (avant commit).
- Exécuter le commit : appliquer toutes les commandes, créditer les revenus, avancer les compteurs de production, attribuer l'XP d'arène.
- Alterner entre les royaumes (joueur → IA → joueur → ...).
- Permettre le Reset (vider les commandes en attente).

```cpp
class TurnSystem {
public:
    void setActiveKingdom(KingdomId id);
    KingdomId getActiveKingdom() const;
    int  getTurnNumber() const;

    bool queueCommand(const TurnCommand& cmd);   // Enregistre (retourne false si illégal)
    void resetPendingCommands();                  // Bouton "Reset"
    const std::vector<TurnCommand>& getPendingCommands() const;

    void commitTurn();                            // Bouton "Jouer" (exécute tout)

private:
    KingdomId                   m_activeKingdom;
    int                         m_turnNumber;
    std::vector<TurnCommand>    m_pendingCommands;

    // Flags pour limites par tour
    bool m_hasMoved;
    bool m_hasBuilt;
    bool m_hasProduced;
    bool m_hasMarried;
};
```

##### CombatSystem

Résout les combats quand une pièce se déplace sur une case ennemie (pièce ou bloc).

```cpp
class CombatSystem {
public:
    struct CombatResult {
        bool    occurred;
        bool    targetWasPiece;     // true = pièce mangée, false = bloc détruit
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

Calcule et crédite les revenus à chaque fin de tour.

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
    // Pour chaque mine/champ : compter les cases occupées par le royaume
    // et seulement si l'ennemi N'EST PAS présent sur le bâtiment
};
```

##### XPSystem

Attribue de l'XP et gère les upgrades.

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

Valide et place les bâtiments sur la carte.

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
    // Vérifications :
    // 1. Le roi est adjacent à la zone d'emprise
    // 2. Toutes les cases de l'emprise sont libres (pas d'eau, pas d'autre bâtiment, pas de pièce)
    // 3. Le royaume a assez d'écus
    // 4. Le joueur n'a pas déjà construit ce tour

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

Gère la fabrication d'unités en caserne.

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

Gère la création de la reine via le rituel à l'église.

```cpp
class MarriageSystem {
public:
    // Vérifie si le mariage est possible :
    // 1. Le roi, un fou et un pion du même royaume sont sur des cases de l'église
    // 2. Le roi et le pion sont sur des cases adjacentes
    // 3. Il n'y a pas déjà une reine dans le royaume
    // 4. L'église n'est pas contestée (pas de pièce ennemie)
    static bool canMarry(
        const Kingdom& kingdom,
        const Board& board,
        const Building& church
    );

    // Transforme le pion en reine
    static void performMarriage(
        Kingdom& kingdom,
        const Board& board,
        const Building& church,
        EventLog& log
    );
};
```

##### CheckSystem

Détecte l'échec et l'échec et mat.

```cpp
class CheckSystem {
public:
    // Le roi du royaume donné est-il en échec ?
    static bool isInCheck(
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // Le roi du royaume donné est-il en échec et mat ?
    static bool isCheckmate(
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // Le roi peut-il se déplacer sur cette case sans être en échec ?
    static bool isSafeSquare(
        sf::Vector2i pos,
        KingdomId kingdomId,
        const Board& board,
        const GameConfig& config
    );

    // Toutes les cases contrôlées (menacées) par un royaume
    static std::set<sf::Vector2i> getThreatenedSquares(
        KingdomId attackerKingdom,
        const Board& board,
        const GameConfig& config
    );
};
```

**Détail de l'algorithme d'échec et mat :**

Pour un roi en échec, il est en **échec et mat** si et seulement si les **trois conditions** sont réunies :

1. Le roi ne peut **fuir** vers aucune case adjacente (toutes sont contrôlées par l'ennemi, occupées par des alliés, ou hors limites).
2. Aucune pièce alliée ne peut **capturer** la pièce qui met le roi en échec.
3. Aucune pièce alliée ne peut **s'interposer** entre le roi et la pièce menaçante (sauf si c'est un cavalier — on ne peut pas bloquer un cavalier).

**Note :** La notion de "pion initial" ajoute une règle supplémentaire. Tant que le roi est encore un pion (n'a aucun sujet), il peut être **tué directement** (pas besoin d'échec et mat). La partie se termine immédiatement.

##### EventLog

Journal de tous les événements de la partie.

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

#### 5.7 Kingdom — Royaumes

**Fichiers :** `Kingdom.hpp/.cpp`, `KingdomId.hpp`

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
    bool hasSubjects() const;     // Le roi a-t-il au moins un sujet ?
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

#### 5.8 AI — Intelligence artificielle

**Fichiers :** `AIController.hpp/.cpp`, `AIEvaluator.hpp/.cpp`, `AIStrategy*.hpp/.cpp`

L'IA est **symbolique** (rule-based), pas basée sur du machine learning. Elle suit un arbre de décision paramétrable.

**Architecture de l'IA :**

```
AIController::playTurn()
│
├── AIEvaluator::evaluate()          # Évaluer la situation (score Board)
│
├── AIStrategyEcon::decide()         # Décisions économiques (farm, production)
│   ├── Aller farmer si pas de revenus
│   ├── Construire une caserne si nécessaire
│   └── Lancer une production si caserne libre
│
├── AIStrategyBuild::decide()        # Décisions de construction
│   ├── Construire des murs défensifs
│   └── Construire une arène
│
├── AIStrategyMove::decide()         # Décisions de mouvement / attaque
│   ├── Attaquer une cible de valeur
│   ├── Défendre le roi
│   ├── Prendre une zone de farm
│   └── Avancer vers l'ennemi
│
├── AIStrategySpecial::decide()      # Actions spéciales
│   ├── Upgrade une pièce si possible
│   ├── Mariage si conditions remplies
│   └── Placer des pièces à l'arène
│
└── Produire les TurnCommands correspondantes
```

**Paramétrage via `ai_params.json` :**

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

Le développeur peut modifier `randomness` pour rendre l'IA moins déterministe, et les `weights` / `thresholds` pour modifier ses priorités. Cela permet d'ajuster le niveau d'intelligence sans modifier le code.

---

#### 5.9 UI — Interface utilisateur (TGUI)

**Fichiers :** `UIManager.hpp/.cpp`, `MainMenuUI.hpp/.cpp`, `HUD.hpp/.cpp`, `PiecePanel.hpp/.cpp`, `BuildingPanel.hpp/.cpp`, `BarracksPanel.hpp/.cpp`, `BuildToolPanel.hpp/.cpp`, `EventLogPanel.hpp/.cpp`, `ToolBar.hpp/.cpp`

**Responsabilités :**

- L'UI est **purement un affichage**. Elle ne modifie jamais l'état du jeu directement.
- Elle émet des **signaux** ou **callbacks** qui se traduisent en `TurnCommand` via l'InputHandler.
- Elle **lit** les données du jeu (lecture seule) pour mettre à jour les widgets.

**UIManager :**

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

**Layout de l'écran de jeu :**

```
┌─────────────────────────────────────────────────────────────────┐
│  Tour: 15   |   ♔ Blanc joue   |   Écus: 230   |  [Reset] [▶]│  ← HUD (haut)
├──────────────────────────────────────────┬──────────────────────┤
│                                          │                      │
│                                          │   Panneau latéral    │
│                                          │   (vide par défaut)  │
│              CARTE DE JEU                │                      │
│              (Renderer)                  │   - Info pièce       │
│                                          │   - Info bâtiment    │
│                                          │   - Caserne          │
│                                          │   - Construction     │
│                                          │   - Journal          │
│                                          │                      │
├──────────────────────────────────────────┴──────────────────────┤
│  [🖱️ Souris]  [🔨 Construction]  [📖 Journal]                  │  ← ToolBar (bas-gauche)
└─────────────────────────────────────────────────────────────────┘
```

---

#### 5.10 Render — Rendu graphique (SFML)

**Fichiers :** `Renderer.hpp/.cpp`, `Camera.hpp/.cpp`, `OverlayRenderer.hpp/.cpp`

**Responsabilités :**

- Dessiner la carte (cellules visibles dans le viewport).
- Dessiner les pièces.
- Dessiner les bâtiments.
- Dessiner les overlays (cases accessibles en vert, preview de construction, fantômes de déplacement, indicateurs de zone contestée, grisage des cases détruites).

**Renderer :**

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
        const TurnSystem& turnSystem   // pour les commandes en attente (previews)
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

**Optimisation du rendu (carte de 1024×1024 cellules) :**

La carte fait potentiellement `2*512 = 1024` cellules de diamètre. À 16×16 pixels, cela fait 16 384 × 16 384 pixels. Il est **impératif** de ne dessiner que les cellules visibles dans le viewport.

```cpp
// Calculer les bornes visibles
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

#### 5.11 Input — Gestion des entrées

**Fichiers :** `InputHandler.hpp/.cpp`, `ToolState.hpp`

**`ToolState` :**

```cpp
enum class ToolState {
    Select,     // Outil souris classique (par défaut)
    Build,      // Outil construction
    Journal     // Outil journal
};
```

**`InputHandler` — Dispatching des événements :**

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

    // Sélection
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

**Mapping des entrées :**

| Action | Entrée |
|--------|--------|
| Déplacer la caméra | Clic molette + drag, ou drag sur zone vide |
| Zoomer / dézoomer | Molette souris |
| Sélectionner une pièce / bâtiment | Clic gauche |
| Déplacer une pièce (attaquer) | Clic gauche sur pièce → clic gauche sur destination |
| Placer un bâtiment (mode construction) | Sélection dans le panneau → clic gauche sur la carte |
| Centrer la caméra sur le roi | Barre espace |
| Pause | Échap |

---

#### 5.12 Save — Sauvegarde et persistance

**Fichiers :** `SaveManager.hpp/.cpp`, `SaveData.hpp`

```cpp
struct SaveData {
    std::string     gameName;
    int             turnNumber;
    KingdomId       activeKingdom;

    // État de la carte
    struct CellData {
        CellType type;
        bool isInCircle;
    };
    std::vector<std::vector<CellData>> grid;
    int mapRadius;

    // Royaumes
    struct KingdomData {
        KingdomId id;
        int gold;
        std::vector<Piece> pieces;
        std::vector<Building> buildings;
    };
    KingdomData whiteKingdom;
    KingdomData blackKingdom;

    // Bâtiments publics
    std::vector<Building> publicBuildings;

    // Journal
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

**Format de sauvegarde :** JSON (via nlohmann/json). Un fichier par sauvegarde dans le dossier `saves/`. Le nom du fichier correspond au nom de la partie.

**Préparation replay :** Stocker en plus l'historique des `TurnCommand` dans la sauvegarde. Ce n'est pas utilisé pour le moment, mais simplifiera l'implémentation future d'un système de replay.

---

#### 5.13 Assets — Gestion des ressources

**Fichiers :** `AssetManager.hpp/.cpp`

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

Charge toutes les textures au démarrage. Aucune texture n'est chargée pendant le jeu.

---

### 6. Boucle de jeu principale

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
        m_gui.handleEvent(event);       // TGUI consomme d'abord

        if (event.type == sf::Event::Closed)
            m_window.close();

        if (m_state == GameState::Playing)
            m_input.handleEvent(event, m_camera, m_board, m_turnSystem, m_uiManager, m_config);
    }
}

void Game::update() {
    switch (m_state) {
        case GameState::Playing:
            // Vérifier si c'est le tour de l'IA
            if (m_turnSystem.getActiveKingdom() == KingdomId::Black) {
                m_ai.playTurn(m_board, m_blackKingdom, m_whiteKingdom,
                              m_turnSystem, m_config, m_eventLog);
                m_turnSystem.commitTurn();
                // Vérifier game over
                if (m_checkSystem.isCheckmate(KingdomId::White, m_board, m_config))
                    m_state = GameState::GameOver;
            }
            m_uiManager.update();
            break;
        // ...autres états...
    }
}

void Game::render() {
    m_window.clear(sf::Color(30, 30, 30));  // Fond sombre (hors carte)

    if (m_state == GameState::Playing || m_state == GameState::Paused) {
        m_renderer.draw(m_window, m_camera, m_board,
                        m_whiteKingdom, m_blackKingdom,
                        m_publicBuildings, m_turnSystem);
    }

    m_gui.draw();   // TGUI se dessine par-dessus
    m_window.display();
}
```

---

### 7. Diagramme de flux d'un tour

```
DÉBUT DU TOUR DU JOUEUR
│
├── Le joueur est libre de :
│   ├── Sélectionner une pièce → voir les cases accessibles (overlay vert)
│   ├── Déplacer une pièce (max 1) → affichage fantôme + flèche
│   ├── Sélectionner l'outil construction → ouvrir le panneau construction
│   │   └── Placer un bâtiment (max 1) → preview en overlay transparent
│   ├── Sélectionner une caserne → ouvrir le panneau caserne
│   │   └── Lancer une fabrication (max 1)
│   ├── Upgrade une pièce (si seuil d'XP atteint)
│   ├── Initier un mariage à l'église (max 1)
│   ├── Créer / défaire une formation
│   ├── Consulter le journal
│   └── Déplacer la caméra, zoomer, sélectionner des éléments
│
├── [Bouton "Reset"] → annuler TOUTES les actions en attente
│
└── [Bouton "Jouer"] → COMMIT DU TOUR
    │
    ├── 1. Exécuter le déplacement (résoudre le combat si applicable)
    │      ├── Pièce → pièce ennemie : combat → XP
    │      └── Pièce → bloc ennemi : destruction → XP
    │
    ├── 2. Exécuter la construction
    │
    ├── 3. Exécuter la fabrication (avancer les compteurs de casernes)
    │      └── Si compteur à 0 : spawn la pièce sur une case adjacente
    │
    ├── 4. Exécuter le mariage (pion → reine)
    │
    ├── 5. Exécuter les upgrades
    │
    ├── 6. Créditer les revenus (mines + champs)
    │
    ├── 7. Attribuer l'XP d'arène
    │
    ├── 8. Vérifier l'état du pion/roi ennemi (game over ?)
    │
    ├── 9. Vérifier échec / échec et mat
    │
    └── 10. Logger les événements → passer au tour de l'IA
         │
         ▼
    TOUR DE L'IA (instantané)
    │
    ├── AIController::playTurn() → produit des TurnCommands
    ├── TurnSystem::commitTurn() → même pipeline que le joueur
    ├── Vérifier game over (côté joueur)
    │
    └── Retour au TOUR DU JOUEUR
```

---

## Partie II — Spécifications techniques détaillées

### 8. La carte — Génération procédurale circulaire

**Forme :** Cercle de rayon 512 cellules. La grille interne est un carré de 1024×1024, avec un masque circulaire. Les cellules hors du cercle sont de type `Void` et ne sont jamais rendues ni accessibles.

**Algorithme de génération détaillé :**

```
FONCTION genererCarte(config):
    rayon = config.map.radius                           // 512
    diametre = rayon * 2                                // 1024
    centre = (rayon, rayon)                             // (512, 512)

    // ÉTAPE 1 : Initialiser la grille
    POUR chaque cellule (x, y) dans [0, diametre) × [0, diametre):
        distance = sqrt((x - centre.x)² + (y - centre.y)²)
        SI distance <= rayon:
            grille[x][y] = Cell(Grass, isInCircle=true)
        SINON:
            grille[x][y] = Cell(Void, isInCircle=false)

    // ÉTAPE 2 : Placer les zones de terre (décoratif)
    // Utiliser un bruit de Perlin simplifié ou des blobs aléatoires
    POUR i = 1 à NOMBRE_BLOBS_TERRE:
        centreBlob = position aléatoire dans le cercle
        rayonBlob = aléatoire(5, 20)
        POUR chaque cellule dans rayonBlob autour de centreBlob:
            SI cellule est dans le cercle ET cellule est Grass:
                cellule.type = Dirt

    // ÉTAPE 3 : Placer les zones d'eau
    // Petits lacs aléatoires. S'assurer de ne pas couper la carte en deux.
    POUR i = 1 à NOMBRE_LACS:
        centreLac = position aléatoire (pas dans les zones de spawn)
        rayonLac = aléatoire(3, 10)
        POUR chaque cellule dans rayonLac autour de centreLac:
            SI cellule est dans le cercle ET cellule est Grass ou Dirt:
                cellule.type = Water
        // Vérification de connectivité (pathfinding entre spawn joueur et spawn IA)
        // Si non connecté → annuler ce lac et recommencer

    // ÉTAPE 4 : Placer l'église (1)
    posEglise = position aléatoire proche du centre, pas dans les zones de spawn
    PLACER église (4×3) à posEglise

    // ÉTAPE 5 : Placer les mines (2)
    POUR i = 1 à 2:
        posMine = position aléatoire, distance >= 10 de tout bâtiment public existant
        PLACER mine (6×6) à posMine

    // ÉTAPE 6 : Placer les champs (3)
    POUR i = 1 à 3:
        posChamp = position aléatoire, distance >= 10 de tout bâtiment public existant
        PLACER champ (4×3) à posChamp

    // ÉTAPE 7 : Placer les pions initiaux
    posJoueur = position aléatoire dans les 25% gauche du cercle
    posIA = position aléatoire dans les 25% droite du cercle
    // S'assurer que les positions sont sur de l'herbe et accessibles

    RETOURNER grille, posJoueur, posIA
```

**Gestion des 25% gauche / 25% droite :**

Le cercle est centré en `(rayon, rayon)`. Les 25% gauche correspondent aux cellules dont `x < rayon - rayon/2` (soit `x < rayon * 0.5`). Les 25% droite : `x > rayon + rayon/2` (soit `x > rayon * 1.5`). Seules les cellules dans le cercle sont considérées.

---

### 9. Système de cellules et terrains

| Terrain | Texture | Traversable | Bloquant LdV | Notes |
|---------|---------|-------------|---------------|-------|
| Grass | `grass.png` | Oui | Non | Terrain par défaut |
| Dirt | `dirt.png` | Oui | Non | Purement décoratif |
| Water | `water.png` | **Non** | **Oui** | Infranchissable par toute pièce |
| Void | (pas rendu) | Non | — | Hors carte |

**Bâtiments sur les cellules :** Quand un bâtiment est placé, les cellules sous son emprise conservent leur terrain d'origine mais pointent vers le `Building`. Le rendu affiche la texture du bâtiment par-dessus. Les cellules de bâtiment sont traversables (sauf les murs).

---

### 10. Système de pièces (Units)

**Identité immuable :** Chaque pièce a un `id` unique attribué à la création et jamais modifié. Quand une pièce est upgradée, seul son `type` change. Sa position, son XP, son ID restent identiques.

**Cas du pion initial / roi :**

- Au début de la partie, chaque royaume a un pion unique.
- Ce pion **n'est pas encore roi**. Il se déplace comme un pion (4 directions, 1 case).
- Dès qu'il possède au moins **un sujet** (une 2e pièce dans le royaume), il **devient automatiquement roi** : son `type` passe de `Pawn` à `King`.
- Si le pion initial est tué **avant** de devenir roi → **game over immédiat**.
- Si le roi n'a plus de sujets (toutes les autres pièces sont mortes), il **reste roi** (ne redevient pas pion). Les règles d'échec et mat classiques s'appliquent.

**Transition des types :**

```
Pion (niveau 0)
├── → Cavalier (niveau 1)  [XP >= 100 + coût en écus]
├── → Fou (niveau 1)       [XP >= 100 + coût en écus]
│
Cavalier / Fou (niveau 1)
└── → Tour (niveau 2)      [XP >= 300 + coût en écus]

Tour (niveau 2) → (bloqué, pas d'upgrade possible)

Reine : obtenue uniquement par mariage (roi + fou + pion dans l'église)
Roi : transformation automatique du pion initial
```

---

### 11. Système de bâtiments

**Bâtiments publics :**

| Bâtiment | Taille | Nombre/carte | Fonctionnalité |
|----------|--------|-------------|----------------|
| Église | 4×3 | 1 | Lieu du mariage (création de la reine) |
| Mine | 6×6 | 2 | Revenu de 10 écus/tour par case occupée |
| Champ | 4×3 | 3 | Revenu de 5 écus/tour par case occupée |

**Règles des bâtiments publics :**

- **Indestructibles.**
- N'appartiennent à personne. Un royaume les **exploite** en y plaçant des pièces.
- Si des pièces des **deux royaumes** sont présentes sur le même bâtiment public → le bâtiment **ne fonctionne pour personne** (contesté, icône épées croisées).
- Si un seul royaume est présent → il exploite le bâtiment (icône bouclier de la couleur du royaume).
- Les pièces **traversent** les bâtiments publics sans problème.

**Bâtiments privés :**

| Bâtiment | Taille | Coût | PV/case | Constructible | Bloquant |
|----------|--------|------|---------|---------------|----------|
| Caserne | 4×2 | 50 écus | 1 | Oui | Non |
| Mur de bois | 1×1 | 20 écus | 1 | Oui | **Oui** |
| Mur de pierre | 1×1 | 40 écus | 3 | Oui | **Oui** |
| Arène | ~9 cases | À définir | À définir | Oui | Non |

**Construction :**

- Seul le **roi** construit.
- La construction doit se faire dans la **périphérie directe** du roi (cases adjacentes au roi touchant l'emprise du bâtiment).
- Maximum **1 construction par tour**.
- Le bâtiment est placé **entièrement** d'un coup (pas case par case).

**Destruction :**

- Les cases d'un bâtiment privé sont détruites individuellement.
- Une case détruite est **grisée** visuellement.
- Mur de pierre : grisage proportionnel aux PV perdus.
- Quand toutes les cases d'un bâtiment sont à 0 PV → le bâtiment **disparaît** entièrement.
- Les bâtiments ne se réparent pas.

**La caserne — production :**

- Une caserne ne produit qu'**une pièce à la fois**.
- Le joueur choisit le type de pièce.
- Le compteur de tours diminue de 1 à chaque commit.
- Quand le compteur atteint 0 → la pièce apparaît sur la **case adjacente libre la plus proche** de la caserne.
- Si la caserne est détruite pendant la production → production annulée, écus perdus.
- On ne peut pas annuler une production en cours.

---

### 12. Système d'expérience et d'upgrade

**Sources d'XP :**

| Action | XP gagnée |
|--------|-----------|
| Tuer un pion | 20 |
| Tuer un cavalier | 50 |
| Tuer un fou | 50 |
| Tuer une tour | 100 |
| Tuer une reine | 300 |
| Détruire un bloc (mur ou case de caserne) | 10 |
| Par tour passé sur une case d'arène | 10 |

**Seuils d'upgrade :**

| Transition | XP requise | Coût écus |
|------------|-----------|-----------|
| Pion → Cavalier ou Fou | ≥ 100 | À définir |
| Cavalier/Fou → Tour | ≥ 300 | À définir |

**Règles :**

- L'XP **ne se reset pas** après une upgrade. Elle continue de s'accumuler sans plafond.
- L'upgrade **n'est pas obligatoire** quand le seuil est atteint. Le joueur choisit quand upgrader.
- La reine et le roi accumulent de l'XP mais elle n'a aucune utilité actuelle.
- L'upgrade coûte des **écus en plus de l'XP** requis.

---

### 13. Système économique

**Revenus :**

- **Mine** : 10 écus/tour par case occupée par une pièce du royaume.
- **Champ** : 5 écus/tour par case occupée.
- **Condition** : le bâtiment public ne doit pas être contesté (pas de pièce ennemie dessus).
- Les revenus sont crédités à la **consommation du tour** (bouton "Jouer").

**Dépenses :**

- Construction de bâtiments.
- Recrutement de pièces (en caserne).
- Upgrades.

**Pas de maintenance** : aucun coût récurrent pour les bâtiments ou les pièces.

**Cycle économique du début de partie :**

```
Tour 1+  : Pion initial se déplace vers une mine ou un champ.
Tour N   : Pion est sur une case de mine → +10 écus/tour.
Tour N+5 : 50 écus accumulés → construire une caserne (le pion/roi doit être adjacent).
Tour N+5 : Lancer la fabrication d'un pion (2 tours).
Tour N+7 : Pion produit (le pion initial devient roi s'il ne l'était pas déjà).
...
```

---

### 14. Système de tour par tour

**Alternance stricte :** Joueur (White) → IA (Black) → Joueur → IA → ...

**Actions par tour (par royaume) :**

| Catégorie | Limite |
|-----------|--------|
| Déplacement (pièce ou formation) | 1 |
| Construction (bâtiment) | 1 |
| Fabrication (lancement de production) | 1 |
| Mariage | 1 |

Toutes les catégories **peuvent** être effectuées dans le **même tour**. Elles ne s'excluent pas.

**Pas de limite de temps :** Le joueur peut réfléchir aussi longtemps qu'il veut.

**Pas d'ordre de phases :** Tout se résout **simultanément** à la consommation du tour.

**Preview avant commit :**

- Le déplacement est affiché en **transparence** (fantôme de la pièce à la destination + flèche depuis la position d'origine).
- La construction est affichée en **overlay transparent** à l'emplacement prévu.
- Le joueur voit l'état futur avant de confirmer.

**Bouton Reset :** Annule **toutes** les actions en attente. Revient à l'état du début du tour. Pas d'annulation individuelle.

---

### 15. Système de formations

**Création :**

- Manuelle. Quand des pièces adjacentes sont sélectionnées, un bouton "Créer formation" apparaît.
- Les pièces doivent être **adjacentes** (côte à côte, sans case vide entre elles).

**Comportement :**

- La formation se comporte comme **une seule pièce**.
- Le déplacement est une **translation** : toutes les pièces bougent du même vecteur.
- La disposition relative des pièces est conservée.
- Sélectionner une pièce d'une formation sélectionne **toute** la formation.
- Le déplacement de la formation suit les règles de déplacement du **type de pièce** composant la formation.

**Dissolution :** Le même bouton permet de **défaire** la formation.

**Questions ouvertes à trancher lors de l'implémentation :**

- Formations mixtes (types différents) : quel pattern de déplacement ?
- Si le mouvement amène une pièce de la formation sur un obstacle ?
- Nombre maximum de pièces par formation ?
- Adjacence 4 directions ou 8 directions ?

---

### 16. Système de check/checkmate

**Échec :**

- Un roi est en **échec** si au moins une pièce ennemie peut l'atteindre via un mouvement légal.
- Quand le roi est en échec, le joueur doit **obligatoirement** le sortir d'échec (déplacer le roi, capturer la pièce menaçante, ou interposer une pièce).

**Échec et mat :**

- Le roi est en échec et mat si :
  1. Il est en échec.
  2. Il ne peut fuir vers aucune case sûre.
  3. Aucune pièce alliée ne peut capturer l'attaquant.
  4. Aucune pièce alliée ne peut bloquer l'attaque (sauf cavalier, pas bloquable).

**Cas spécial du pion initial :**

- Tant que le pion initial n'est pas devenu roi (pas de sujets), il peut être **tué directement** → game over immédiat. Pas besoin d'échec/mat.

**Implémentation :**

```cpp
bool CheckSystem::isCheckmate(KingdomId id, const Board& board, const GameConfig& config) {
    Piece* king = getKing(id, board);
    if (!king) return true;  // Pion initial tué = game over

    // Si pas en échec, pas de checkmate
    if (!isInCheck(id, board, config)) return false;

    // Essayer tous les coups possibles de toutes les pièces du royaume
    // Si au moins un coup retire le roi de l'échec → pas checkmate
    for (const Piece& piece : getKingdomPieces(id, board)) {
        auto moves = MovementRules::getValidMoves(piece, board, config);
        for (const auto& move : moves) {
            // Simuler le mouvement
            Board simBoard = board;  // Copie
            applyMove(simBoard, piece.id, move);
            if (!isInCheck(id, simBoard, config)) {
                return false;  // Au moins un coup sauve le roi
            }
        }
    }
    return true;  // Aucun coup ne sauve le roi
}
```

---

### 17. Intelligence artificielle

**Type :** IA symbolique (rule-based), pas de machine learning.

**Principes :**

- L'IA a **exactement les mêmes pouvoirs** que le joueur. Même API, mêmes contraintes.
- Pas de triche, pas d'avantage caché.
- L'IA voit tout (pas de brouillard de guerre).
- L'IA calcule son tour **instantanément** du point de vue du joueur.

**Arbre de décision simplifié :**

```
SI aucun revenu ET pas sur une zone de farm:
    → Déplacer le roi/pion vers la mine/champ la plus proche

SI assez d'écus ET pas de caserne:
    → Construire une caserne (adjacent au roi)

SI caserne libre ET assez d'écus:
    → Lancer une production

SI une pièce peut atteindre une cible de haute valeur (reine > tour > fou/cavalier > pion):
    → Attaque prioritaire

SI le roi est en échec:
    → Mouvement de survie obligatoire

SI conditions de mariage remplies:
    → Effectuer le mariage

SI une pièce atteint le seuil d'upgrade:
    → Upgrade

SI des pièces sont inactives:
    → Les déplacer vers des positions stratégiques (farm, défense, attaque)

SINON:
    → Mouvement positionnel (avancer vers le centre / vers l'ennemi)
```

**Évaluation de la position :**

L'AIEvaluator attribue un **score numérique** à la position actuelle en pondérant :

- Nombre et type des pièces (matériel).
- Revenus économiques (nombre de cases de farm occupées).
- Sécurité du roi.
- Contrôle territorial (occuper le centre / les bâtiments publics).
- Bâtiments possédés.
- Menaces imminentes (pièces en danger).

L'IA choisit les actions qui **maximisent son score** ou **minimisent le score de l'ennemi**.

---

### 18. Interface utilisateur

**Menu principal :**

```
┌─────────────────────────────────────┐
│                                     │
│       A  NORMAL  CHESS  GAME        │
│                                     │
│       ┌───────────────────┐         │
│       │  Nouvelle partie  │         │
│       └───────────────────┘         │
│       ┌───────────────────┐         │
│       │    Continuer      │         │
│       └───────────────────┘         │
│                                     │
└─────────────────────────────────────┘
```

- **Nouvelle partie :** Ouvre un sous-menu pour nommer la partie, puis la lance.
- **Continuer :** Ouvre un menu listant les sauvegardes existantes.

**HUD (bande supérieure en jeu) :**

| Élément | Position | Détail |
|---------|----------|--------|
| Numéro de tour | Gauche | "Tour 15" |
| Indicateur du joueur actif | Centre-gauche | Icône roi + "Blanc joue" |
| Nombre d'écus | Centre | "♦ 230" |
| Bouton Reset | Droite | Réinitialise les actions du tour |
| Bouton Jouer | Extrême droite | Consomme le tour |

**Barre d'outils (bas-gauche) :**

| Outil | Icône | Comportement |
|-------|-------|--------------|
| Souris | 🖱️ | Mode par défaut. Sélectionne, déplace la caméra |
| Construction | 🔨 | Ouvre le panneau construction. Permet de placer des bâtiments |
| Journal | 📖 | Ouvre le panneau journal (historique des événements) |

**Panneau latéral (droite) :**

Le panneau latéral est **contextuel**. Il affiche le contenu correspondant à la sélection ou à l'outil actif :

- **Pièce sélectionnée :** Type, XP, portée, possibilité d'upgrade, bouton upgrade.
- **Bâtiment sélectionné :** Type, PV par case, état.
- **Caserne sélectionnée :** Choix de la troupe, état de production, tours restants, bouton fabriquer.
- **Outil construction actif :** Liste des bâtiments constructibles + coût, sélection, preview sur la carte.
- **Journal :** Deux onglets : événements alliés, événements ennemis. Historique complet de la partie.

**Overlays visuels sur la carte :**

| Overlay | Quand | Visuel |
|---------|-------|--------|
| Cases accessibles | Pièce sélectionnée | Overlay vert semi-transparent |
| Preview de déplacement | Déplacement planifié (avant commit) | Fantôme transparent de la pièce + flèche |
| Preview de construction | Bâtiment prêt à être placé (mode construction) | Overlay transparent du bâtiment |
| Cases détruites | Case de bâtiment à 0 PV | Grisage de la case |
| Mur endommagé | Mur de pierre < 3 PV | Grisage progressif proportionnel |
| Indicateur de zone | Au-dessus des bâtiments publics | Bouclier blanc / noir / épées croisées |

---

### 19. Caméra et navigation

**Type :** Vue 100% top-down (2D, pas d'isométrique).

**Contrôles :**

| Action | Entrée |
|--------|--------|
| Déplacer la caméra | Clic molette + drag / drag sur zone vide |
| Zoomer | Molette haut |
| Dézoomer | Molette bas |
| Centrer sur le roi | Barre espace |

**Implémentation technique :**

La caméra est une `sf::View` SFML avec des transformations de zoom et de déplacement.

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

**Pas de brouillard de guerre :** Toute la carte est visible en permanence.

---

### 20. Système de sauvegarde

**Mécanismes :**

- **Manuel :** Le joueur décide quand sauvegarder.
- **Emplacements multiples :** Plusieurs parties, chacune avec ses sauvegardes.
- **Stockage :** JSON dans le dossier `saves/`.

**Quand le joueur quitte :**

- Option 1 : Quitter sans sauvegarder.
- Option 2 : Quitter en sauvegardant.
- Option 3 : Sauvegarder sans quitter.

**Données sérialisées :**

- Nom de la partie.
- Numéro de tour, royaume actif.
- Grille complète (rayon, types de cellules, masque circulaire).
- État de chaque royaume (or, pièces, bâtiments privés avec PV).
- Bâtiments publics.
- Historique du journal d'événements.
- File des `TurnCommand` historiques (base pour future fonction replay).

---

## Partie III — Plan d'implémentation par phases

### Phase 0 — Setup du projet

**Objectif :** Avoir un projet C++ compilable avec SFML et TGUI.

| # | Tâche | Détail |
|---|-------|--------|
| 0.1 | Installer SFML 2.6+ | Télécharger ou via vcpkg/conan |
| 0.2 | Installer TGUI 1.x | Compatible SFML 2.6 |
| 0.3 | Créer le `CMakeLists.txt` principal | Trouver SFML, TGUI, configurer les flags C++17 |
| 0.4 | Créer `main.cpp` minimal | Ouvrir une fenêtre SFML, afficher un fond vide |
| 0.5 | Vérifier la compilation | Fenêtre s'ouvre et se ferme proprement |
| 0.6 | Créer l'arborescence de dossiers | `src/Core`, `src/Board`, etc. + `assets/`, `saves/`, `docs/` |
| 0.7 | Ajouter nlohmann/json | Pour le chargement de config et les sauvegardes |

**Livrable :** Un projet qui compile, ouvre une fenêtre SFML vide, et inclut TGUI.

---

### Phase 1 — Fondations du moteur

**Objectif :** Game loop fonctionnelle avec machine à états.

| # | Tâche | Détail |
|---|-------|--------|
| 1.1 | Implémenter `Game` | Constructeur, `run()`, `handleInput()`, `update()`, `render()` |
| 1.2 | Implémenter `GameState` | Enum + transitions (MainMenu → Playing → Paused → GameOver) |
| 1.3 | Implémenter `GameClock` | Delta time via `sf::Clock` |
| 1.4 | Implémenter `AssetManager` | Chargement de toutes les textures et fonts |
| 1.5 | Implémenter `GameConfig` | Chargement du JSON `game_params.json`, getters typés |
| 1.6 | Créer `game_params.json` | Toutes les constantes du jeu (voir Partie IV) |

**Livrable :** Boucle de jeu vide qui tourne. Les assets sont chargés. Les constantes sont accessibles.

---

### Phase 2 — Carte et rendu

**Objectif :** Afficher la carte circulaire avec les terrains et la caméra.

| # | Tâche | Détail |
|---|-------|--------|
| 2.1 | Implémenter `CellType` et `Cell` | Structures de données de base |
| 2.2 | Implémenter `Board` | Grille 2D, masque circulaire, accesseurs |
| 2.3 | Implémenter `BoardGenerator` | Génération procédurale complète (cercle, terre, eau, bâtiments publics, spawns) |
| 2.4 | Implémenter `Camera` | `sf::View`, zoom (molette), pan (clic molette + drag) |
| 2.5 | Implémenter `Renderer::drawBoard()` | Dessiner les cellules visibles dans le viewport (frustum culling) |
| 2.6 | Intégrer dans `Game` | Générer la carte → l'afficher → naviguer avec la caméra |
| 2.7 | Tester les performances | S'assurer que le rendu est fluide sur une carte 1024×1024 |

**Livrable :** La carte circulaire s'affiche. On peut naviguer librement (pan/zoom). Les terrains (herbe, terre, eau) sont visibles. Les emplacements des bâtiments publics sont marqués.

---

### Phase 3 — Pièces et déplacement

**Objectif :** Placer les pièces, les sélectionner et les déplacer selon les règles des échecs.

| # | Tâche | Détail |
|---|-------|--------|
| 3.1 | Implémenter `PieceType` et `Piece` | Types, identité, XP, position |
| 3.2 | Implémenter `Kingdom` | Structure de données de base (or, pièces, bâtiments) |
| 3.3 | Implémenter `KingdomId` | Enum White / Black |
| 3.4 | Placer les pions initiaux | Un pion blanc et un pion noir aux positions générées |
| 3.5 | Implémenter `Renderer::drawPieces()` | Dessiner les sprites des pièces sur la carte |
| 3.6 | Implémenter `InputHandler` mode Select | Clic gauche → sélectionner une pièce → overlay vert des cases accessibles |
| 3.7 | Implémenter `MovementRules` | Calcul des mouvements légaux pour chaque type de pièce |
| 3.7.1 | — Pion | 4 directions cardinales, 1 case |
| 3.7.2 | — Cavalier | Mouvement en L, saut murs alliés |
| 3.7.3 | — Fou | Diagonale, max 8 cases, bloqué par murs/eau |
| 3.7.4 | — Tour | Lignes droites, max 8 cases, bloqué par murs/eau |
| 3.7.5 | — Reine | Diagonale + lignes droites, max 8 cases |
| 3.7.6 | — Roi | 1 case toutes directions |
| 3.8 | Implémenter `OverlayRenderer` | Afficher les cases accessibles en vert semi-transparent |
| 3.9 | Implémenter le clic de destination | Clic gauche sur une case verte → enregistrer la commande Move |
| 3.10 | Afficher le fantôme | Preview du déplacement (pièce transparente + flèche) avant commit |

**Livrable :** On peut sélectionner une pièce, voir ses cases accessibles, et planifier un déplacement (fantôme visible).

---

### Phase 4 — Système de tour par tour

**Objectif :** Cycle joueur → commit → IA → commit fonctionnel.

| # | Tâche | Détail |
|---|-------|--------|
| 4.1 | Implémenter `TurnCommand` | Structure (type, données selon le type) |
| 4.2 | Implémenter `TurnSystem` | File de commandes, validation, commit |
| 4.3 | Implémenter le bouton "Jouer" | TGUI : bouton qui déclenche `commitTurn()` |
| 4.4 | Implémenter le bouton "Reset" | Vider les commandes en attente |
| 4.5 | Implémenter le commit | Exécution séquentielle des actions : move, build, produce, marry, upgrade |
| 4.6 | Implémenter l'alternance | White → Black → White → ... |
| 4.7 | Stub IA | L'IA ne fait rien pour le moment (passe son tour) |
| 4.8 | Implémenter `CombatSystem` | Résolution des combats lors du commit (déplacement sur ennemi) |
| 4.9 | Implémenter l'XP de kill | Attribuer l'XP à la pièce qui mange |
| 4.10 | Transition pion → roi | Quand le pion initial a un sujet, il devient king |
| 4.11 | Game over (pion initial tué) | Détecter et passer en GameOver |

**Livrable :** On peut jouer des tours complets. Le déplacement fonctionne. Le combat résout la capture. L'alternance joueur/IA est en place (IA passive).

---

### Phase 5 — Bâtiments et construction

**Objectif :** Construire des bâtiments (caserne, murs, arène) et interagir avec les bâtiments publics.

| # | Tâche | Détail |
|---|-------|--------|
| 5.1 | Implémenter `BuildingType` et `Building` | Types, PV par case, propriétaire |
| 5.2 | Implémenter `BuildingFactory` | Création de bâtiments avec dimensions et PV depuis la config |
| 5.3 | Afficher les bâtiments publics | Dessiner les textures église, mines, champs sur la carte |
| 5.4 | Implémenter `BuildSystem` | Validation (adjacence roi, budget, cases libres) + placement |
| 5.5 | Implémenter l'outil Construction | Mode Build dans InputHandler, panneau latéral avec liste des bâtiments |
| 5.6 | Implémenter la preview de construction | Overlay transparent du bâtiment sur la carte avant le clic |
| 5.7 | Implémenter la validation de placement | Cases libres, pas d'eau, roi adjacent |
| 5.8 | Implémenter la destruction de bâtiments | Pièce attaque un bloc → damageCellAt() → grisage → disparition |
| 5.9 | Implémenter les murs (blocage) | Murs bloquent la ligne de vue et le passage (sauf cavalier allié) |
| 5.10 | Intégrer les murs dans `MovementRules` | Mettre à jour le calcul de mouvements pour prendre en compte les murs |

**Livrable :** On peut construire des casernes, murs (bois/pierre), arènes. Les murs bloquent le passage et la ligne de vue. Les bâtiments peuvent être détruits case par case.

---

### Phase 6 — Économie et ressources

**Objectif :** Les mines et champs rapportent des écus. Les dépenses sont déduites.

| # | Tâche | Détail |
|---|-------|--------|
| 6.1 | Implémenter `EconomySystem` | Calcul des revenus à chaque commit |
| 6.2 | Implémenter la logique de contestation | Si deux royaumes sur un bâtiment public → 0 revenu |
| 6.3 | Afficher les indicateurs de zone | Bouclier blanc/noir/épées croisées au-dessus des bâtiments publics |
| 6.4 | Implémenter les dépenses | Déduire les écus à la construction et à la fabrication |
| 6.5 | Afficher les écus dans le HUD | Compteur dynamique |
| 6.6 | Implémenter `ProductionSystem` | Fabrication en caserne : début, avancement, spawn |
| 6.7 | Implémenter le panneau caserne | Sélection du type de pièce, état de production, tours restants |
| 6.8 | Spawn des pièces produites | Apparition sur case adjacente libre après compteur à 0 |

**Livrable :** Le cycle économique complet fonctionne : farmer → construire → recruter. Les indicateurs de zone sont visibles.

---

### Phase 7 — XP, upgrades et mariage

**Objectif :** Système d'XP fonctionnel, upgrades, et création de la reine.

| # | Tâche | Détail |
|---|-------|--------|
| 7.1 | Implémenter `XPSystem` complet | Attribution d'XP (kill, bloc, arène) |
| 7.2 | Implémenter l'upgrade | Vérification seuil + coût → changement de type |
| 7.3 | Implémenter le panneau d'upgrade | Dans PiecePanel, bouton "Upgrade en cavalier/fou/tour" quand seuil atteint |
| 7.4 | Implémenter l'XP d'arène | Attribution automatique à chaque commit pour les pièces sur l'arène |
| 7.5 | Implémenter `MarriageSystem` | Vérification (roi + fou + pion dans l'église, adjacence roi-pion, pas de reine existante) |
| 7.6 | Implémenter le mariage | Transformation du pion en reine à la consommation du tour |
| 7.7 | Intégrer dans le commit du tour | XP d'arène + production + mariage dans le pipeline de commit |

**Livrable :** L'XP s'accumule. Les pièces peuvent être upgradées. La reine peut être créée via le mariage à l'église.

---

### Phase 8 — Interface utilisateur TGUI

**Objectif :** UI complète et fonctionnelle.

| # | Tâche | Détail |
|---|-------|--------|
| 8.1 | Implémenter `MainMenuUI` | Écran : Nouvelle partie + Continuer |
| 8.2 | Implémenter le menu Nouvelle partie | Champ de texte pour nommer la partie + bouton Lancer |
| 8.3 | Implémenter le menu Charger partie | Liste des sauvegardes + bouton Charger |
| 8.4 | Implémenter `HUD` complet | Tour, joueur actif, écus, Reset, Jouer |
| 8.5 | Implémenter `ToolBar` | 3 outils : souris, construction, journal |
| 8.6 | Implémenter `PiecePanel` | XP, type, portée, bouton upgrade |
| 8.7 | Implémenter `BuildingPanel` | Type, PV, état |
| 8.8 | Implémenter `BarracksPanel` | Choix pièce, production en cours, tours restants |
| 8.9 | Implémenter `BuildToolPanel` | Liste bâtiments constructibles + coût |
| 8.10 | Implémenter `EventLogPanel` | Deux onglets (allié / ennemi), historique complet |
| 8.11 | Implémenter le menu pause (Échap) | Overlay sombre + options (reprendre, sauvegarder, quitter) |
| 8.12 | Implémenter l'écran Game Over | Message de victoire ou défaite |

**Livrable :** Menu principal, HUD, tous les panneaux latéraux, barre d'outils, pause, game over.

---

### Phase 9 — Intelligence artificielle

**Objectif :** IA fonctionnelle qui joue stratégiquement.

| # | Tâche | Détail |
|---|-------|--------|
| 9.1 | Implémenter `AIConfig` | Chargement de `ai_params.json` |
| 9.2 | Implémenter `AIEvaluator` | Scoring de la position (matériel, économie, sécurité roi, territoire) |
| 9.3 | Implémenter `AIStrategyEcon` | Farm → caserne → production |
| 9.4 | Implémenter `AIStrategyMove` | Attaque, défense, positionnement |
| 9.5 | Implémenter `AIStrategyBuild` | Construction murs, casernes, arènes |
| 9.6 | Implémenter `AIStrategySpecial` | Upgrades, mariage, arène |
| 9.7 | Implémenter `AIController` | Orchestration : évaluer → décider → produire des TurnCommands |
| 9.8 | Intégrer l'IA dans la boucle | Après le commit du joueur, l'IA joue instantanément |
| 9.9 | Tester l'IA | Vérifier que l'IA fait des choix logiques et respecte les règles |
| 9.10 | Affiner les poids | Ajuster `ai_params.json` itérativement |

**Livrable :** L'IA joue des tours complets. Elle farm, construit, recrute, attaque, et fait des upgrades/mariages.

---

### Phase 10 — Formations

**Objectif :** Système de formations fonctionnel.

| # | Tâche | Détail |
|---|-------|--------|
| 10.1 | Implémenter `FormationSystem` | Création, dissolution, détection d'adjacence |
| 10.2 | Implémenter la sélection de formation | Clic sur une pièce en formation → sélection de toute la formation |
| 10.3 | Implémenter le déplacement de formation | Translation du groupe entier |
| 10.4 | Afficher les formations | Overlay visuel reliant les pièces d'une même formation |
| 10.5 | Bouton créer/défaire formation | Dans le PiecePanel quand la pièce est adjacente à d'autres |

**Livrable :** Les formations fonctionnent : création, déplacement groupé, dissolution.

---

### Phase 11 — Sauvegarde et persistance

**Objectif :** Sauvegarder et charger des parties.

| # | Tâche | Détail |
|---|-------|--------|
| 11.1 | Implémenter `SaveData` | Structure complète de l'état du jeu |
| 11.2 | Implémenter la sérialisation JSON | Toutes les données → JSON (nlohmann/json) |
| 11.3 | Implémenter la désérialisation | JSON → reconstruction de l'état du jeu |
| 11.4 | Implémenter `SaveManager` | `save()`, `load()`, `listSaves()`, `deleteSave()` |
| 11.5 | Intégrer les sauvegardes dans l'UI | Bouton sauvegarder (pause), menu charger (menu principal) |
| 11.6 | Stocker les TurnCommands historiques | Base pour futur replay |
| 11.7 | Tester la persistance | Sauvegarder → quitter → recharger → état identique |

**Livrable :** Le joueur peut sauvegarder, quitter, recharger ses parties.

---

### Phase 12 — Polish et équilibrage

**Objectif :** Peaufiner l'expérience et équilibrer le gameplay.

| # | Tâche | Détail |
|---|-------|--------|
| 12.1 | Tester le cycle complet | Partie du début à l'échec et mat |
| 12.2 | Ajuster les constantes | Modifier `game_params.json` (coûts, revenus, XP) |
| 12.3 | Ajuster l'IA | Modifier `ai_params.json` (poids, seuils, aléa) |
| 12.4 | Vérifier l'échec/mat | Tester tous les cas limites |
| 12.5 | Vérifier les edge cases | Pion initial tué, caserne détruite en production, zones contestées, etc. |
| 12.6 | Optimiser le rendu | Profiler et optimiser si nécessaire |
| 12.7 | Corriger les bugs | Fix des problèmes identifiés pendant les tests |
| 12.8 | Vérifier le système de sauvegarde | Aucune corruption de données |
| 12.9 | Définir les valeurs manquantes | Coûts recrutement par type, coût upgrade, coût arène, taille arène |

**Livrable :** Jeu complet, stable, équilibré.

---

## Partie IV — Tableau des constantes de jeu

Toutes les valeurs sont stockées dans `config/game_params.json` et accessibles via `GameConfig`.

### Carte

| Paramètre | Valeur | Notes |
|-----------|--------|-------|
| Rayon de la carte (cases) | 512 | Paramétrable |
| Taille d'une cellule (px) | 16×16 | |
| Nombre de mines | 2 | |
| Nombre de champs | 3 | |
| Distance min. entre bâtiments publics | 10 blocs | |
| Zone spawn joueur | 25% gauche | |
| Zone spawn IA | 25% droite | |

### Économie

| Paramètre | Valeur | Notes |
|-----------|--------|-------|
| Écus de départ | 0 | |
| Mine : revenu/tour/case | 10 écus | |
| Champ : revenu/tour/case | 5 écus | |
| Coût caserne | 50 écus | |
| Coût mur bois | 20 écus | |
| Coût mur pierre | 40 écus | |
| Coût arène | **À définir** | |
| Coût recrutement pion | **À définir** | |
| Coût recrutement cavalier | **À définir** | |
| Coût recrutement fou | **À définir** | |
| Coût recrutement tour | **À définir** | |
| Coût upgrade pion→cav./fou | **À définir** | |
| Coût upgrade cav./fou→tour | **À définir** | |

### Production

| Paramètre | Valeur |
|-----------|--------|
| Pion (niv. 0) | 2 tours |
| Cavalier (niv. 1) | 4 tours |
| Fou (niv. 1) | 4 tours |
| Tour (niv. 2) | 6 tours |

### Combat et PV

| Paramètre | Valeur |
|-----------|--------|
| Portée max. globale (fou/tour/reine) | 8 cases |
| PV mur bois | 1 |
| PV mur pierre | 3 |
| PV case caserne | 1 |

### Expérience

| Paramètre | Valeur |
|-----------|--------|
| XP kill pion | 20 |
| XP kill cavalier | 50 |
| XP kill fou | 50 |
| XP kill tour | 100 |
| XP kill reine | 300 |
| XP destruction bloc | 10 |
| XP arène par tour par pièce | 10 |
| Seuil upgrade pion → cav./fou | 100 XP |
| Seuil upgrade cav./fou → tour | 300 XP |

### Bâtiments (dimensions)

| Bâtiment | Largeur | Hauteur |
|----------|---------|---------|
| Caserne | 4 | 2 |
| Église | 4 | 3 |
| Mine | 6 | 6 |
| Champ | 4 | 3 |
| Arène | ~9 cases | **À préciser** |
| Mur bois | 1 | 1 |
| Mur pierre | 1 | 1 |

### Limites par tour

| Action | Limite |
|--------|--------|
| Déplacement | 1 par tour |
| Construction | 1 par tour |
| Fabrication (lancement) | 1 par tour |
| Mariage | 1 par tour |

---

## Partie V — Bonnes pratiques et conventions

### Code C++

- **Standard :** C++17 minimum.
- **Nommage :** PascalCase pour les classes/enums, camelCase pour les fonctions/variables, UPPER_SNAKE_CASE pour les constantes préprocesseur.
- **Préfixe membres :** `m_` pour les membres de classe privés.
- **Pas de magic numbers :** Toute valeur numérique passe par `GameConfig`.
- **RAII :** Utiliser des constructeurs et destructeurs pour la gestion des ressources.
- **Smart pointers :** Utiliser `std::unique_ptr` et `std::shared_ptr` quand l'ownership n'est pas trivial.
- **const correctness :** Marquer `const` tout ce qui ne modifie pas l'état.
- **Pas de variables globales.** L'état global passe par Game et ses sous-systèmes.

### Architecture

- **Aucun system de Render/UI ne modifie l'état du jeu.** Lecture seule.
- **Aucun system de logique ne dessine quoi que ce soit.**
- **L'IA utilise exactement la même API que le joueur.** Elle produit des `TurnCommand`.
- **Les Systems sont stateless** : ils reçoivent des références aux données et les modifient, mais ne stockent pas d'état interne significatif.
- **Tester les Systems indépendamment.** Créer des tests unitaires pour MovementRules, CombatSystem, CheckSystem, etc.

### Performance

- **Frustum culling obligatoire :** Ne dessiner que les cellules visibles dans le viewport.
- **Chargement unique des assets :** Toutes les textures sont chargées au démarrage et partagées via `AssetManager`.
- **Pas de réallocation par frame :** Préallouer les structures de données. Réutiliser les buffers.
- **Profiler avant d'optimiser :** Le bottleneck sera dans la logique (IA, pathfinding), pas dans le GPU.

### Maintenabilité

- **Un fichier par classe** (header + source).
- **Compilation séparée :** Headers légers (forward declarations), implementation dans les .cpp.
- **Configuration externalisée :** Toutes les constantes dans des fichiers JSON éditables sans recompilation.
- **Journal d'événements :** Utiliser `EventLog` pour tracer tout ce qui se passe (utile pour le debug ET pour le joueur).

---

*Ce plan est le document de référence pour l'implémentation de A Normal Chess Game. Il sera mis à jour à chaque décision architecturale ou changement de priorité.*
