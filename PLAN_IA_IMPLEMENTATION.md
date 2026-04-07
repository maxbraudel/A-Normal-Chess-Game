# Plan d'implémentation de l'IA — A Normal Chess Game

---

## Table des matières

- [0. Diagnostic de l'IA actuelle](#0-diagnostic-de-lia-actuelle)
- [1. Architecture cible — Vue d'ensemble](#1-architecture-cible--vue-densemble)
- [2. Phase 1 — Forward Model (simulation rapide)](#2-phase-1--forward-model-simulation-rapide)
- [3. Phase 2 — Fonction d'évaluation refondée](#3-phase-2--fonction-dévaluation-refondée)
- [4. Phase 3 — Couche Stratégie (Utility AI)](#4-phase-3--couche-stratégie-utility-ai)
- [5. Phase 4 — Couche Tactique (MCTS simplifié)](#5-phase-4--couche-tactique-mcts-simplifié)
- [6. Phase 5 — Couche Exécution (action pipeline)](#6-phase-5--couche-exécution-action-pipeline)
- [7. Phase 6 — Module Checkmate Solver](#7-phase-6--module-checkmate-solver)
- [8. Phase 7 — Module Économie & Production](#8-phase-7--module-économie--production)
- [9. Phase 8 — Module Construction](#9-phase-8--module-construction)
- [10. Phase 9 — Module Mariage & Spécial](#10-phase-9--module-mariage--spécial)
- [11. Phase 10 — Anti-freeze et budget temps](#11-phase-10--anti-freeze-et-budget-temps)
- [12. Phase 11 — Tests, self-play et calibrage](#12-phase-11--tests-self-play-et-calibrage)
- [13. Résumé des fichiers à créer/modifier](#13-résumé-des-fichiers-à-créermodifier)
- [14. Paramètres et configuration](#14-paramètres-et-configuration)
- [Annexe A — Valeurs de référence du jeu](#annexe-a--valeurs-de-référence-du-jeu)
- [Annexe B — Pseudo-code des algorithmes clés](#annexe-b--pseudo-code-des-algorithmes-clés)

---

## 0. Diagnostic de l'IA actuelle

### Ce qui existe

L'IA actuelle est un système **heuristique pur en 4 modules** :

| Module | Rôle |
|---|---|
| `AIBrain` | Classifie la partie en 6 phases (EARLY_GAME → CRISIS) et fixe des poids de priorité |
| `AIStrategyMove` | 10 sections de règles if/else pour choisir un mouvement |
| `AIStrategyEcon` | Production d'unités + récolte de ressources |
| `AIStrategyBuild` | Placement de casernes et murs |
| `AIStrategySpecial` | Upgrades et mariages |
| `AITacticalEngine` | Scoring heuristique d'un mouvement (7 facteurs), pas de recherche arborescente |
| `AITurnContext` | Cache pré-calculé par tour (ThreatMap, mouvements légaux, ressources libres) |

### Problèmes critiques identifiés

| # | Problème | Impact | Cause racine |
|---|---|---|---|
| **P1** | **Freeze / lenteur** | Le jeu se bloque pendant le tour IA | `findCheckmateIn1()` est O(pièces² × coups) sans élagage. Avec 10+ pièces, chaque tour simule des milliers de positions |
| **P2** | **Incapacité à faire échec et mat** | L'IA encercle le roi adverse mais ne conclut pas | Pas de vrai solver de mat. La logique de siège "hold position" (Manhattan ≤ 4) bloque les pièces à distance au lieu de fermer les cases de fuite |
| **P3** | **Boucles infinies** | L'IA fait des allers-retours avec la même pièce | Détection de boucle sur 12 coups seulement, pénalité `-200 × n` insuffisante, pas de blacklist |
| **P4** | **Sous-exploitation des casernes** | L'IA ne lance des productions que dans 1 caserne | `AIStrategyEcon` produit 1 unité par tour même avec N casernes ; backlog trop conservateur (≥3 unités → stop) |
| **P5** | **Gestion des troupes médiocre** | Pièces empilées, pas de coordination | Scoring de mouvement purement local (1 coup d'avance), pas de planification multi-tour |
| **P6** | **Mariage rarement atteint** | La reine n'est presque jamais créée | Hard-lock à distance Manhattan > 8, abandon dès que la phase passe en AGGRESSION |
| **P7** | **Pas d'adaptation à l'adversaire** | Même stratégie quel que soit le jeu humain | Phases déterminées par des seuils fixes, pas de reconnaissance de la stratégie ennemie |
| **P8** | **IA 100% déterministe** | Prédictible après quelques parties | `randomness: 0.0`, aucune variation de poids |

---

## 1. Architecture cible — Vue d'ensemble

L'architecture recommandée est un **système hybride à 3 couches** avec un **forward model** central :

```
┌─────────────────────────────────────────────────────┐
│                    AIDirector                        │
│  (orchestrateur principal, remplace AIController)    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─────────────┐   ┌──────────────┐   ┌─────────┐ │
│  │ STRATÉGIE   │──▶│  TACTIQUE    │──▶│ ACTION  │ │
│  │ (Utility AI)│   │  (MCTS léger)│   │ (Queue) │ │
│  └──────┬──────┘   └──────┬───────┘   └────┬────┘ │
│         │                 │                 │      │
│         ▼                 ▼                 ▼      │
│  ┌─────────────────────────────────────────────┐   │
│  │           ForwardModel (simulation)         │   │
│  │  clone état → appliquer action → évaluer    │   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
│  ┌──────────────────────┐  ┌────────────────────┐  │
│  │   Evaluator          │  │   TimeBudget       │  │
│  │   (score d'état)     │  │   (garde-fou)      │  │
│  └──────────────────────┘  └────────────────────┘  │
│                                                     │
│  ┌──────────────────────────────────────────────┐  │
│  │   CheckmateSolver (détection mat en N)       │  │
│  └──────────────────────────────────────────────┘  │
│                                                     │
│  Modules spécialisés :                              │
│  ┌────────┐ ┌────────┐ ┌──────────┐ ┌──────────┐  │
│  │ Éco    │ │ Build  │ │ Marriage │ │ Movement │  │
│  │ Module │ │ Module │ │ Module   │ │ Module   │  │
│  └────────┘ └────────┘ └──────────┘ └──────────┘  │
└─────────────────────────────────────────────────────┘
```

### Principes directeurs

1. **Forward Model central** — Toute décision non triviale passe par une simulation d'état. L'IA "joue dans sa tête" avant d'agir.
2. **Utility AI pour la stratégie** — Chaque objectif possible (attaq, éco, mariage, défense…) reçoit un score dynamique. L'objectif avec le meilleur score est choisi.
3. **MCTS léger pour la tactique** — Monte Carlo Tree Search limité en temps (50-200ms) pour explorer les séquences d'actions les plus prometteuses dans l'objectif choisi.
4. **Budget temps strict** — Chaque computation est capped par un chrono. Jamais de freeze.
5. **Modules autonomes** — Chaque sous-domaine (éco, build, mariage, mouvement) est un module remplaçable qui expose une interface `suggestAction(context, objective) → Action + score`.

### Pourquoi pas Minimax ?

Le facteur de branchement du jeu est **trop élevé**. Avec N pièces × M cases possibles × (move + build + produce + marry), un tour peut avoir des centaines de combinaisons. Minimax à profondeur 3+ serait trop lent. MCTS s'adapte naturellement au budget temps disponible et gère bien les grands espaces d'actions.

### Pourquoi pas du Deep Learning ?

Le Deep RL (type AlphaZero) demande des millions de parties de self-play et un GPU. C'est impraticable pour un projet solo en C++. L'architecture Utility + MCTS + heuristiques donne un excellent niveau sans infrastructure lourde.

---

## 2. Phase 1 — Forward Model (simulation rapide)

### Objectif

Créer un **simulateur d'état léger** qui permet de :
- Cloner l'état du jeu en < 0.1ms
- Appliquer une action (mouvement, production, construction, mariage) sur le clone
- Évaluer l'état résultant via l'Evaluator

C'est la **brique fondamentale** sur laquelle tout le reste repose (MCTS, checkmate solver, etc.).

### Fichiers à créer

```
src/AI/ForwardModel.hpp
src/AI/ForwardModel.cpp
src/AI/GameSnapshot.hpp
src/AI/GameSnapshot.cpp
```

### Structure `GameSnapshot`

Un instantané léger de l'état du jeu, **sans graphiques, sans UI, sans historique complet** :

```cpp
struct SnapPiece {
    int id;
    PieceType type;
    KingdomId kingdom;
    Vec2i position;
    int xp;
};

struct SnapBuilding {
    int id;
    BuildingType type;
    KingdomId owner;       // Neutral si neutre
    Vec2i position;
    int width, height;
    std::vector<int> cellHP;
    bool isProducing;
    PieceType producingType;
    int turnsRemaining;
};

struct SnapKingdom {
    KingdomId id;
    int gold;
    std::vector<SnapPiece> pieces;
    std::vector<SnapBuilding> buildings;
};

struct GameSnapshot {
    int boardRadius;                          // 25
    std::vector<std::vector<CellType>> terrain; // Compressé : seulement le type de terrain
    SnapKingdom white;
    SnapKingdom black;
    KingdomId activeKingdom;
    int turnNumber;

    // --- Méthodes ---
    GameSnapshot clone() const;                // Deep copy
    SnapPiece* findPiece(int id);
    SnapPiece* findKing(KingdomId k);
    SnapBuilding* findBuilding(int id);
    void removePiece(int id);
    void removeBuilding(int id);
    bool isTraversable(Vec2i pos) const;
    bool isInBounds(Vec2i pos) const;
    SnapPiece* pieceAt(Vec2i pos) const;
    SnapBuilding* buildingAt(Vec2i pos) const;
};
```

**Optimisation mémoire :** Le terrain ne change quasi jamais. On peut partager un pointeur `shared_ptr<TerrainGrid>` entre tous les clones au lieu de le copier.

### Classe `ForwardModel`

```cpp
class ForwardModel {
public:
    // Créer un snapshot depuis l'état réel du jeu
    static GameSnapshot createSnapshot(const Board& board,
                                       const Kingdom& white,
                                       const Kingdom& black,
                                       int turnNumber);

    // Appliquer une action atomique
    static bool applyMove(GameSnapshot& s, int pieceId, Vec2i dest);
    static bool applyBuild(GameSnapshot& s, KingdomId k, BuildingType type, Vec2i pos);
    static bool applyProduce(GameSnapshot& s, int barracksId, PieceType type);
    static bool applyMarriage(GameSnapshot& s, KingdomId k, int pawnId);
    static bool applyUpgrade(GameSnapshot& s, int pieceId, PieceType toType);

    // Avancer d'un tour complet (income, production tick, spawns)
    static void advanceTurn(GameSnapshot& s, KingdomId k);

    // Requêtes tactiques
    static std::vector<Vec2i> getLegalMoves(const GameSnapshot& s, int pieceId);
    static bool isInCheck(const GameSnapshot& s, KingdomId k);
    static bool isCheckmate(const GameSnapshot& s, KingdomId k);
    static ThreatMap buildThreatMap(const GameSnapshot& s, KingdomId attacker);
};
```

### Règles de simulation à implémenter

Chaque méthode doit reproduire **exactement** les mêmes règles que les vrais systèmes du jeu :

| Action | Logique |
|---|---|
| `applyMove` | Déplacer pièce, capturer si ennemi présent (sauf roi), octroyer XP, dommage mur si attaque mur |
| `applyBuild` | Vérifier or ≥ coût, adjacence roi, espace libre → placer bâtiment, déduire or |
| `applyProduce` | Vérifier barracks libre + or ≥ coût → lancer production, déduire or |
| `applyMarriage` | Vérifier roi + fou + pion sur église, adjacence roi-pion, pas de reine existante → transformer pion en reine |
| `advanceTurn` | Décrémenter production, spawner unités complétées, collecter revenus (10g/mine, 5g/ferme si occupation exclusive), XP arène |

### Tests unitaires

- Cloner un état → vérifier que la modification du clone ne touche pas l'original
- Appliquer un mouvement de pion → vérifier nouvelle position
- Appliquer une capture → vérifier suppression
- Simuler 5 tours d'income → vérifier or correct
- Vérifier isCheckmate sur des positions connues

---

## 3. Phase 2 — Fonction d'évaluation refondée

### Objectif

Remplacer le scoring simpliste de `AITacticalEngine::scoreMove()` par une **fonction d'évaluation d'état globale** qui note une position complète.

### Fichier

```
src/AI/AIEvaluator.hpp  (refondre le fichier existant)
src/AI/AIEvaluator.cpp  (refondre le fichier existant)
```

### Score global : 7 composantes

```
Score(s, kingdom) =
    W_mat  × MaterialScore(s, kingdom)
  + W_eco  × EconomyScore(s, kingdom)
  + W_map  × MapControlScore(s, kingdom)
  + W_king × KingSafetyScore(s, kingdom)
  + W_dev  × DevelopmentScore(s, kingdom)
  + W_threat × ThreatScore(s, kingdom)
  + W_mate × CheckmateProximityScore(s, kingdom)
```

Le score est toujours calculé **du point de vue de l'IA** : positif = bon pour l'IA, négatif = bon pour le joueur.

### Détail de chaque composante

#### 3.1 MaterialScore — Valeur des pièces

```cpp
float MaterialScore(const GameSnapshot& s, KingdomId k) {
    float myMat = 0, enemyMat = 0;
    for (auto& p : myPieces)    myMat    += pieceValue(p.type);
    for (auto& p : enemyPieces) enemyMat += pieceValue(p.type);
    return myMat - enemyMat;
}
```

Valeurs :
| Pièce | Valeur |
|---|---|
| Pion | 100 |
| Cavalier | 320 |
| Fou | 330 |
| Tour | 500 |
| Reine | 900 |
| Roi | 10000 (pas de valeur d'échange, mais pénalité si menacé) |

#### 3.2 EconomyScore — Richesse et revenus

```cpp
float EconomyScore(const GameSnapshot& s, KingdomId k) {
    float goldScore    = myGold * 0.5f;
    float incomeScore  = countMyIncomePerTurn(s, k) * 5.0f;  // revenu récurrent vaut plus
    float enemyIncome  = countEnemyIncomePerTurn(s, k) * 5.0f;
    float barracksScore = countMyBarracks(s, k) * 30.0f;      // capacité de production
    return goldScore + incomeScore - enemyIncome + barracksScore;
}
```

Le revenu par tour se calcule en comptant les cases de mines (10g) et fermes (5g) occupées exclusivement.

#### 3.3 MapControlScore — Contrôle territorial

```cpp
float MapControlScore(const GameSnapshot& s, KingdomId k) {
    int myResourceCells   = countOccupiedResourceCells(s, k);
    int enemyResourceCells = countOccupiedResourceCells(s, enemyOf(k));
    int contestedCells    = countContestedResourceCells(s);
    float churchControl   = hasChurchControl(s, k) ? 50.0f : 0.0f;
    float arenaControl    = countArenaControl(s, k) * 20.0f;
    return (myResourceCells - enemyResourceCells) * 15.0f
         - contestedCells * 5.0f
         + churchControl
         + arenaControl;
}
```

#### 3.4 KingSafetyScore — Sécurité du roi

```cpp
float KingSafetyScore(const GameSnapshot& s, KingdomId k) {
    float score = 0;
    Vec2i myKing = findKing(s, k)->position;
    Vec2i enemyKing = findKing(s, enemyOf(k))->position;

    // Mon roi menacé = très mauvais
    if (isInCheck(s, k)) score -= 500;

    // Cases de fuite du roi
    int escapeSquares = countSafeAdjacentSquares(s, myKing, k);
    score += escapeSquares * 30.0f;

    // Pièces défensives proches de mon roi (Manhattan ≤ 3)
    int defenders = countDefendersNear(s, myKing, k, 3);
    score += defenders * 40.0f;

    // Roi ennemi menacé = bon
    if (isInCheck(s, enemyOf(k))) score += 400;

    // Cases de fuite du roi ennemi (moins = mieux)
    int enemyEscapes = countSafeAdjacentSquares(s, enemyKing, enemyOf(k));
    score -= enemyEscapes * 25.0f;    // moins il en a, mieux c'est pour nous

    return score;
}
```

#### 3.5 DevelopmentScore — Avancement et potentiel

```cpp
float DevelopmentScore(const GameSnapshot& s, KingdomId k) {
    float score = 0;
    // Production en cours = valeur future
    for (auto& b : myBuildings)
        if (b.isProducing)
            score += pieceValue(b.producingType) * 0.3f;

    // XP cumulée des pièces (potentiel d'upgrade)
    for (auto& p : myPieces)
        score += p.xp * 0.1f;

    // Avoir une reine = gros bonus
    if (hasQueen(s, k)) score += 200;

    return score;
}
```

#### 3.6 ThreatScore — Pression offensive

```cpp
float ThreatScore(const GameSnapshot& s, KingdomId k) {
    float score = 0;
    ThreatMap myThreats = buildThreatMap(s, k);

    // Pièces ennemies sous menace = bon
    for (auto& ep : enemyPieces)
        if (myThreats.isSet(ep.position.x, ep.position.y))
            score += pieceValue(ep.type) * 0.3f;

    // Mes pièces menacées = mauvais
    ThreatMap enemyThreats = buildThreatMap(s, enemyOf(k));
    for (auto& mp : myPieces)
        if (enemyThreats.isSet(mp.position.x, mp.position.y))
            score -= pieceValue(mp.type) * 0.4f;

    return score;
}
```

#### 3.7 CheckmateProximityScore — Proximité du mat

Le plus **critique** pour conclure les parties :

```cpp
float CheckmateProximityScore(const GameSnapshot& s, KingdomId k) {
    float score = 0;
    Vec2i eKing = findKing(s, enemyOf(k))->position;

    // Roi ennemi en échec = bon
    if (isInCheck(s, enemyOf(k))) score += 300;

    // Roi ennemi en échec et mat = victoire
    if (isCheckmate(s, enemyOf(k))) score += 100000;

    // Mon roi en échec et mat = défaite
    if (isCheckmate(s, k)) score -= 100000;

    // Nombre de cases de fuite bloquées du roi ennemi
    int maxEscapes = 8;
    int blockedEscapes = maxEscapes - countSafeAdjacentSquares(s, eKing, enemyOf(k));
    score += blockedEscapes * 40.0f;

    // Distance moyenne de mes pièces au roi ennemi
    float avgDist = averageManhattanDistance(s, k, eKing);
    score += std::max(0.0f, 100.0f - avgDist * 3.0f);

    // Pièces dans l'anneau d'assaut (Manhattan ≤ 3 du roi ennemi)
    int assaultPieces = countPiecesInRadius(s, k, eKing, 3);
    score += assaultPieces * 50.0f;

    return score;
}
```

### Pondérations par phase

Les poids W_xxx changent selon la phase stratégique :

| Phase | W_mat | W_eco | W_map | W_king | W_dev | W_threat | W_mate |
|---|---|---|---|---|---|---|---|
| EARLY_GAME | 1.0 | 3.0 | 2.0 | 1.0 | 2.0 | 0.5 | 0.3 |
| BUILD_UP | 1.5 | 2.0 | 1.5 | 1.0 | 1.5 | 1.0 | 0.5 |
| MID_GAME | 2.0 | 1.0 | 1.0 | 1.5 | 1.0 | 1.5 | 1.5 |
| AGGRESSION | 2.0 | 0.3 | 0.5 | 1.5 | 0.3 | 2.0 | 3.0 |
| ENDGAME | 1.5 | 0.2 | 0.3 | 2.0 | 0.2 | 2.0 | 4.0 |
| CRISIS | 0.5 | 0.1 | 0.1 | 5.0 | 0.1 | 0.5 | 0.5 |

---

## 4. Phase 3 — Couche Stratégie (Utility AI)

### Objectif

Remplacer le système de phases rigide de `AIBrain` par une **Utility AI** qui évalue dynamiquement tous les objectifs possibles et choisit le meilleur.

### Fichiers

```
src/AI/AIStrategy.hpp     (nouveau, remplace AIBrain)
src/AI/AIStrategy.cpp
```

### Concept

Chaque tour, la couche stratégie évalue N **objectifs candidats** et attribue un score d'utilité à chacun. L'objectif avec le score le plus élevé est transmis à la couche tactique.

### Objectifs candidats

```cpp
enum class StrategicObjective {
    RUSH_ATTACK,         // Attaquer immédiatement avec ce qu'on a
    ECONOMY_EXPAND,      // Occuper mines/fermes, accumuler de l'or
    BUILD_ARMY,          // Produire des unités dans les casernes
    BUILD_INFRASTRUCTURE,// Construire casernes/murs
    PURSUE_QUEEN,        // Mariage à l'église
    DEFEND_KING,         // Protéger notre roi
    CHECKMATE_PRESS,     // Encercler et mater le roi ennemi
    CONTEST_RESOURCES,   // Disputer une mine/ferme à l'ennemi
    RETREAT_REGROUP,     // Regrouper les troupes dispersées
};
```

### Calcul des scores d'utilité

Chaque objectif a une **fonction de score** basée sur l'état :

```cpp
float scoreRushAttack(const AITurnContext& ctx) {
    float score = 0;
    int myPieces  = ctx.selfPieceCount;
    int enemyPieces = ctx.enemyPieceCount;

    // Plus on a d'avantage matériel, plus le rush est tentant
    if (myPieces > enemyPieces)
        score += (myPieces - enemyPieces) * 20;

    // Si l'ennemi n'a que son roi, rush fortement
    if (enemyPieces == 1) score += 80;

    // Si on a du matériel de mat suffisant
    if (hasMatingMaterial) score += 30;

    // Pénaliser si notre roi est en danger
    if (ctx.selfKingInCheck) score -= 50;

    return std::clamp(score, 0.0f, 100.0f);
}

float scoreEconomyExpand(const AITurnContext& ctx) {
    float score = 0;

    // Moins on a de revenus, plus c'est urgent
    int income = ctx.myIncomePerTurn;
    if (income == 0) score += 70;
    else if (income < 20) score += 40;
    else score += 10;

    // Moins on a d'or, plus c'est urgent
    if (ctx.myGold < 30) score += 30;

    // Ressources libres disponibles = opportunité
    score += ctx.freeResourceCells.size() * 5;

    // Réduire si déjà en bonne position éco
    if (income >= 40) score -= 20;

    return std::clamp(score, 0.0f, 100.0f);
}

float scoreBuildArmy(const AITurnContext& ctx) {
    float score = 0;

    // Besoin de troupes si on en a peu
    if (ctx.selfPieceCount <= 2) score += 50;
    else if (ctx.selfPieceCount <= 4) score += 30;
    else score += 10;

    // Or disponible pour produire
    if (ctx.myGold >= 60) score += 20;  // assez pour une tour
    if (ctx.myGold >= 30) score += 10;  // assez pour cavalier/fou

    // Casernes disponibles (non occupées)
    int freeBarracks = countFreeBarracks(ctx);
    score += freeBarracks * 15;

    // Réduire si trop de troupes en attente
    if (ctx.unitsInBacklog >= 5) score -= 30;

    return std::clamp(score, 0.0f, 100.0f);
}

float scoreCheckmatePress(const AITurnContext& ctx) {
    float score = 0;

    // Roi ennemi statique depuis longtemps = opportunité
    score += ctx.enemyKingStaticTurns * 10;

    // Pièces proches du roi ennemi (Manhattan ≤ 4)
    int nearPieces = ctx.piecesNearEnemyKing;
    score += nearPieces * 15;

    // Cases de fuite du roi ennemi bloquées
    int blockedEscapes = 8 - ctx.enemyKingEscapeSquares;
    score += blockedEscapes * 12;

    // Roi ennemi en échec = presser
    if (ctx.enemyKingInCheck) score += 40;

    // Matériel de mat nécessaire
    if (!hasMatingMaterial) score -= 60;

    return std::clamp(score, 0.0f, 100.0f);
}

float scorePursueQueen(const AITurnContext& ctx) {
    float score = 0;

    // Déjà une reine → 0
    if (ctx.hasQueen) return 0;

    // Pièces nécessaires présentes (roi + fou + pion)
    if (!ctx.hasBishop || !ctx.hasPawn) return 0;

    score += 40;  // Base : c'est toujours utile

    // Proximité des pièces à l'église
    float avgDistToChurch = ctx.avgDistToChurch;
    score += std::max(0.0f, 30.0f - avgDistToChurch * 2.0f);

    // Plus la partie est longue, plus la reine est précieuse
    if (ctx.turnNumber > 20) score += 15;
    if (ctx.turnNumber > 40) score += 15;

    return std::clamp(score, 0.0f, 100.0f);
}

float scoreDefendKing(const AITurnContext& ctx) {
    float score = 0;

    if (ctx.selfKingInCheck) score += 90;  // Priorité max

    // Cases de fuite de notre roi
    int myEscapes = ctx.selfKingEscapeSquares;
    if (myEscapes <= 2) score += 40;
    else if (myEscapes <= 4) score += 15;

    // Pièces ennemies proches de notre roi (Manhattan ≤ 4)
    score += ctx.enemyPiecesNearMyKing * 12;

    return std::clamp(score, 0.0f, 100.0f);
}
```

### Sélection de l'objectif

```cpp
StrategicObjective AIStrategy::chooseObjective(const AITurnContext& ctx) {
    struct Candidate {
        StrategicObjective obj;
        float score;
    };

    std::vector<Candidate> candidates = {
        {RUSH_ATTACK,          scoreRushAttack(ctx)},
        {ECONOMY_EXPAND,       scoreEconomyExpand(ctx)},
        {BUILD_ARMY,           scoreBuildArmy(ctx)},
        {BUILD_INFRASTRUCTURE, scoreBuildInfra(ctx)},
        {PURSUE_QUEEN,         scorePursueQueen(ctx)},
        {DEFEND_KING,          scoreDefendKing(ctx)},
        {CHECKMATE_PRESS,      scoreCheckmatePress(ctx)},
        {CONTEST_RESOURCES,    scoreContestResources(ctx)},
        {RETREAT_REGROUP,      scoreRetreatRegroup(ctx)},
    };

    // Trier par score décroissant
    std::sort(candidates.begin(), candidates.end(),
              [](auto& a, auto& b) { return a.score > b.score; });

    // Variation aléatoire légère (top-2 sélection stochastique)
    if (candidates.size() >= 2 && m_config.randomness > 0) {
        float diff = candidates[0].score - candidates[1].score;
        if (diff < 10.0f) {
            // Choisir aléatoirement entre les 2 meilleurs
            float roll = randomFloat(0.0f, 1.0f);
            if (roll < 0.3f) return candidates[1].obj;
        }
    }

    return candidates[0].obj;
}
```

### Sous-objectif combiné (important !)

Un tour comporte **4 slots d'action** (move + build + produce + marry). La stratégie peut donc formuler un **plan composite** :

```cpp
struct TurnPlan {
    StrategicObjective primaryObjective;  // Guide le mouvement
    bool shouldProduce;                   // Lancer production ?
    PieceType preferredProduction;        // Quel type ?
    bool shouldBuild;                     // Construire ?
    BuildingType preferredBuilding;       // Quel bâtiment ?
    bool shouldMarry;                     // Tenter mariage ?
};
```

Par exemple, même si l'objectif principal est RUSH_ATTACK (= le mouvement sera offensif), l'IA peut simultanément lancer des productions dans toutes ses casernes et construire une caserne si le roi est adjacent à un bon emplacement.

---

## 5. Phase 4 — Couche Tactique (MCTS simplifié)

### Objectif

Remplacer le scoring local de `AITacticalEngine` par un **MCTS (Monte Carlo Tree Search)** qui explore les séquences d'actions sur plusieurs tours pour trouver le meilleur coup.

### Fichiers

```
src/AI/AIMCTS.hpp
src/AI/AIMCTS.cpp
```

### Pourquoi MCTS est adapté ici

- Le facteur de branchement est grand → MCTS gère bien les grands espaces
- Le budget temps est variable → MCTS s'arrête naturellement quand le temps est épuisé
- Pas besoin de profondeur fixe → MCTS explore les branches prometteuses en profondeur
- Compatible avec le Forward Model → il suffit de cloner + appliquer + évaluer

### Algorithme

```
MCTS(état_initial, objectif, budget_ms):
    root = Nœud(état_initial)

    TANT QUE temps < budget_ms:
        node = SELECTION(root)       // UCB1 pour descendre l'arbre
        node = EXPANSION(node)       // Ajouter un fils aléatoire valide
        score = SIMULATION(node)     // Playout rapide (rollout)
        BACKPROPAGATION(node, score) // Remonter le score

    RETOURNER meilleur_fils(root)    // Fils avec le plus de visites
```

### Structure du nœud

```cpp
struct MCTSNode {
    GameSnapshot state;
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;

    // Action qui a mené à cet état depuis le parent
    struct Action {
        enum Type { MOVE, BUILD, PRODUCE, MARRY, END_TURN } type;
        int pieceId;           // pour MOVE
        Vec2i destination;     // pour MOVE et BUILD
        BuildingType bldType;  // pour BUILD
        PieceType prodType;    // pour PRODUCE
        int barracksId;        // pour PRODUCE
        int pawnId;            // pour MARRY
    };
    Action action;

    // Statistiques
    int visits = 0;
    float totalScore = 0.0f;
    float averageScore() const { return visits > 0 ? totalScore / visits : 0; }
};
```

### Génération d'actions (réduction du branchement)

Le facteur de branchement brut peut être > 1000. **L'élagage intelligent est crucial** :

```cpp
std::vector<Action> generateCandidateActions(
    const GameSnapshot& s,
    KingdomId k,
    StrategicObjective objective)
{
    std::vector<Action> actions;

    // --- MOUVEMENTS (le plus gros facteur) ---
    for (auto& piece : s.kingdom(k).pieces) {
        auto moves = ForwardModel::getLegalMoves(s, piece.id);

        // ÉLAGAGE : filtrer selon l'objectif
        for (auto& dest : moves) {
            float relevance = scoreMoveRelevance(s, piece, dest, objective);
            if (relevance > RELEVANCE_THRESHOLD)
                actions.push_back({MOVE, piece.id, dest});
        }
    }

    // --- PRODUCTIONS (1 par caserne libre) ---
    for (auto& b : s.kingdom(k).buildings) {
        if (b.type == Barracks && !b.isProducing) {
            for (PieceType pt : {Pawn, Knight, Bishop, Rook}) {
                if (s.kingdom(k).gold >= recruitCost(pt))
                    actions.push_back({PRODUCE, 0, {}, {}, pt, b.id});
            }
        }
    }

    // --- CONSTRUCTIONS ---
    // Seulement si l'objectif le demande et si or suffisant
    if (objective == BUILD_INFRASTRUCTURE || objective == ECONOMY_EXPAND) {
        auto buildSpots = findBuildSpots(s, k);
        for (auto& [type, pos] : buildSpots)
            actions.push_back({BUILD, 0, pos, type});
    }

    // --- MARIAGE ---
    if (objective == PURSUE_QUEEN && canMarry(s, k))
        actions.push_back({MARRY, 0, {}, {}, {}, 0, findMarriagePawn(s, k)});

    // --- FIN DE TOUR (pass) ---
    actions.push_back({END_TURN});

    return actions;
}
```

### Fonction de pertinence des mouvements (élagage)

```cpp
float scoreMoveRelevance(const GameSnapshot& s,
                          const SnapPiece& piece,
                          Vec2i dest,
                          StrategicObjective obj) {
    float score = 0;
    Vec2i eKing = findEnemyKing(s)->position;
    int distToEnemyKing = manhattan(dest, eKing);

    // Captures = toujours pertinent
    if (auto* victim = s.pieceAt(dest))
        score += pieceValue(victim->type) * 2.0f;

    switch (obj) {
        case RUSH_ATTACK:
        case CHECKMATE_PRESS:
            // Mouvements vers le roi ennemi
            score += std::max(0.0f, 50.0f - distToEnemyKing * 2.0f);
            // Bloque une case de fuite du roi ennemi
            if (isAdjacentTo(dest, eKing)) score += 30;
            break;

        case ECONOMY_EXPAND:
        case CONTEST_RESOURCES:
            // Mouvements vers les ressources
            if (isResourceCell(s, dest)) score += 40;
            break;

        case PURSUE_QUEEN:
            // Mouvements vers l'église
            if (isChurchCell(s, dest)) score += 50;
            score += std::max(0.0f, 30.0f - distToChurch(dest) * 2.0f);
            break;

        case DEFEND_KING:
            // Mouvements protégeant notre roi
            Vec2i myKing = findMyKing(s)->position;
            if (isAdjacentTo(dest, myKing)) score += 30;
            break;
    }

    // Seuil minimal
    return score;
}
```

**Seuil recommandé :** `RELEVANCE_THRESHOLD = 5.0f` — ne garder que les mouvements "intéressants". Cela réduit le branchement de ~500 à ~20-50 actions par nœud.

### Simulation (rollout)

Le rollout est un **playout rapide** sur N tours (pas un playout complet jusqu'au mat) :

```cpp
float rollout(GameSnapshot state, KingdomId aiKingdom, int maxDepth) {
    for (int d = 0; d < maxDepth; ++d) {
        KingdomId active = (d % 2 == 0) ? aiKingdom : enemyOf(aiKingdom);

        // Vérifier fin de partie
        if (ForwardModel::isCheckmate(state, aiKingdom))
            return -10000.0f + d;  // Perte (plus tôt = pire)
        if (ForwardModel::isCheckmate(state, enemyOf(aiKingdom)))
            return +10000.0f - d;  // Victoire (plus tôt = mieux)

        // Sélectionner action rapide (heuristique, pas MCTS)
        auto action = selectRolloutAction(state, active);
        applyAction(state, action);
        ForwardModel::advanceTurn(state, active);
    }

    // Évaluer la position finale
    return AIEvaluator::evaluate(state, aiKingdom);
}
```

**Profondeur de rollout recommandée :** 4-8 tours (2-4 tours par joueur).

**Action de rollout :** Pour la rapidité, le rollout utilise une **heuristique simple** (pas de MCTS récursif) :
- Si capture rentable disponible → capturer
- Si roi en danger → fuir
- Sinon → mouvement aléatoire pondéré vers l'objectif

### Calibrage MCTS

| Paramètre | Valeur |
|---|---|
| Budget temps par tour | 100-300 ms |
| Constante d'exploration UCB1 (C) | 1.41 (√2) |
| Profondeur de rollout | 6 tours |
| Seuil de pertinence (élagage) | 5.0 |
| Max enfants par nœud | 30 |
| Max itérations si temps épuisé | 500 |

---

## 6. Phase 5 — Couche Exécution (action pipeline)

### Objectif

Remplacer `AIController::computeTurnPlan()` par un **pipeline propre** qui orchestre stratégie → tactique → exécution.

### Fichier

```
src/AI/AIDirector.hpp    (remplace AIController)
src/AI/AIDirector.cpp
```

### Pipeline d'un tour IA

```cpp
void AIDirector::computeTurn(Game& game) {
    auto timer = TimeBudget(m_config.maxTurnTimeMs);  // 300ms

    // 1. Construire le contexte
    m_context.build(game);

    // 2. Détection immédiate : mat en 1
    if (auto mateMove = m_checkmateSolver.findMateIn1(m_context)) {
        queueMove(game, *mateMove);
        // Lancer aussi les productions (ne coûte rien)
        queueAllProductions(game);
        return;
    }

    // 3. Couche STRATÉGIE : choisir objectif + plan composite
    TurnPlan plan = m_strategy.computePlan(m_context);

    // 4. Couche TACTIQUE (MCTS) : trouver le meilleur mouvement
    //    pour l'objectif principal
    if (timer.remainingMs() > 50) {
        auto bestAction = m_mcts.search(
            m_context.snapshot,
            m_context.aiKingdom,
            plan.primaryObjective,
            timer.remainingMs() - 20  // garder 20ms pour l'exécution
        );
        if (bestAction.type == MOVE)
            queueMove(game, bestAction);
    }

    // 5. Exécuter les actions secondaires (non-MCTS, heuristiques)

    // 5a. Construire si demandé
    if (plan.shouldBuild && !m_hasBuilt)
        executeBuild(game, plan);

    // 5b. Produire dans TOUTES les casernes libres
    if (plan.shouldProduce)
        executeAllProductions(game, plan);

    // 5c. Mariage si demandé et possible
    if (plan.shouldMarry && !m_hasMarried)
        executeMarriage(game);
}
```

### Production dans TOUTES les casernes (résolution P4)

```cpp
void AIDirector::executeAllProductions(Game& game, const TurnPlan& plan) {
    auto& kingdom = game.getKingdom(m_aiKingdom);

    for (auto& building : kingdom.buildings) {
        if (building.type != BuildingType::Barracks) continue;
        if (building.isProducing) continue;

        PieceType type = chooseProductionType(plan, kingdom);
        int cost = m_config.getRecruitCost(type);

        if (kingdom.gold >= cost) {
            game.getTurnSystem().queueCommand({
                TurnCommand::Produce,
                m_aiKingdom,
                .buildingId = building.id,
                .produceType = type
            });
            // Note : le système de tour actuel n'autorise qu'1 production.
            // → IL FAUT MODIFIER TurnSystem pour permettre N productions
            //   (1 par caserne) dans le même tour.
        }
    }
}
```

> **Point critique** : Le `TurnSystem` actuel a un flag `m_hasProduced` qui bloque après 1 production. **Il faut modifier TurnSystem** pour autoriser 1 production par caserne par tour, pas 1 production globale. C'est un changement dans `TurnSystem::queueCommand()` : au lieu de vérifier `m_hasProduced`, vérifier si la caserne cible est déjà en production.

---

## 7. Phase 6 — Module Checkmate Solver

### Objectif

Remplacer l'actuel `findCheckmateIn1()` (O(pièces² × coups), lent) par un solver performant capable de détecter mat en 1 rapidement et mat en 2-3 coups en background.

### Fichiers

```
src/AI/CheckmateSolver.hpp
src/AI/CheckmateSolver.cpp
```

### Mat en 1 (optimisé)

```cpp
std::optional<MoveAction> CheckmateSolver::findMateIn1(const AITurnContext& ctx) {
    const auto& snapshot = ctx.snapshot;
    Vec2i eKing = findEnemyKing(snapshot)->position;

    // Pré-calculer les cases de fuite du roi ennemi
    auto escapeSquares = computeEscapeSquares(snapshot, eKing);

    for (auto& [pieceId, moves] : ctx.selfMoves) {
        auto* piece = snapshot.findPiece(pieceId);
        if (!piece) continue;
        if (piece->type == PieceType::King) continue;  // Roi ne mate pas seul

        for (auto& dest : moves) {
            // Skip: capturer le roi directement (interdit)
            if (dest == eKing) continue;

            // Le mouvement met-il le roi en échec ?
            if (!wouldGiveCheck(snapshot, *piece, dest, eKing)) continue;

            // Simulation rapide : le roi peut-il fuir ?
            auto simState = snapshot;
            ForwardModel::applyMove(simState, pieceId, dest);

            bool canEscape = false;
            for (auto& escape : escapeSquares) {
                if (isLegalEscape(simState, eKing, escape)) {
                    canEscape = true;
                    break;
                }
            }

            // Vérifier aussi : un défenseur peut-il interposer ou capturer ?
            if (!canEscape)
                canEscape = canDefendCheck(simState, enemyKingdom);

            if (!canEscape)
                return MoveAction{pieceId, dest};
        }
    }
    return std::nullopt;
}
```

**Optimisations vs l'actuel :**
1. **`wouldGiveCheck()` rapide** — Vérifie si le mouvement attaque le roi AVANT de simuler. Élimine 90% des candidats.
2. **Cases de fuite pré-calculées** — Calculées une seule fois, pas pour chaque candidat.
3. **Short-circuit sur les défenses** — Dès qu'un interposeur ou capteur est trouvé, on passe au candidat suivant.

### Mat en 2-3 (optionnel, background)

Si le budget temps le permet (> 100ms restantes), chercher un mat en 2-3 :

```cpp
std::optional<MoveAction> CheckmateSolver::findMateInN(
    const GameSnapshot& snapshot, KingdomId ai, int maxDepth, int budgetMs)
{
    auto timer = TimeBudget(budgetMs);

    // Alpha-beta minimax limité à maxDepth=4-6 demi-coups
    // Avec élagage agressif : ne considérer que les coups donnant échec
    // ou bloquant les cases de fuite

    return alphaBetaMate(snapshot, ai, 0, maxDepth, -INF, +INF, timer);
}
```

Ce solver utilise un **minimax alpha-beta** mais **uniquement sur les positions de mat** : il ne considère que les coups qui donnent échec ou bloquent une case de fuite. Cela réduit drastiquement le facteur de branchement (de ~200 à ~10-20 coups intéressants).

---

## 8. Phase 7 — Module Économie & Production

### Objectif

Optimiser la gestion économique pour que l'IA : (a) maximise ses revenus tôt, (b) produise le bon mix d'unités, (c) utilise toutes ses casernes.

### Fichiers

```
src/AI/AIEconomyModule.hpp
src/AI/AIEconomyModule.cpp
```

### Planification de récolte

```cpp
struct ResourcePlan {
    std::vector<std::pair<int, Vec2i>> assignments;  // pieceId → cell cible
    int expectedIncome;                                // revenu projeté
};

ResourcePlan AIEconomyModule::planResourceGathering(const AITurnContext& ctx) {
    ResourcePlan plan;

    // Identifier toutes les cases de resources libres (non occupées par nous)
    auto freeResources = ctx.freeResourceCells;

    // Identifier nos pièces non critiques (pas le roi, pas en combat)
    auto availablePieces = getIdlePieces(ctx);

    // Assigner optimalement : algorithme glouton distance-minimale
    // (assignment problem simplifié)
    std::sort(freeResources.begin(), freeResources.end(),
        [](auto& a, auto& b) {
            // Prioriser mines (10g) sur fermes (5g)
            return resourceValue(a) > resourceValue(b);
        });

    for (auto& cell : freeResources) {
        if (availablePieces.empty()) break;

        // Trouver la pièce la plus proche qui n'est pas déjà assignée
        int bestIdx = -1;
        int bestDist = INT_MAX;
        for (int i = 0; i < availablePieces.size(); ++i) {
            int dist = manhattan(availablePieces[i].position, cell);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }

        if (bestIdx >= 0 && bestDist <= 15) {  // Ne pas envoyer trop loin
            plan.assignments.push_back({availablePieces[bestIdx].id, cell});
            availablePieces.erase(availablePieces.begin() + bestIdx);
        }
    }

    plan.expectedIncome = calculateExpectedIncome(ctx, plan);
    return plan;
}
```

### Planification de production

```cpp
struct ProductionPlan {
    std::vector<std::pair<int, PieceType>> orders;  // barracksId → type
};

ProductionPlan AIEconomyModule::planProduction(
    const AITurnContext& ctx,
    StrategicObjective objective)
{
    ProductionPlan plan;
    auto& kingdom = ctx.myKingdom;
    int availableGold = kingdom.gold;

    // Réserve d'or selon la phase
    int reserve = computeGoldReserve(ctx);
    int budget = std::max(0, availableGold - reserve);

    // Compter ce qu'on a et ce qu'on veut
    auto composition = analyzeArmyComposition(ctx);
    auto desired = computeDesiredComposition(ctx, objective);

    // Pour chaque caserne libre, décider quoi produire
    for (auto& b : kingdom.buildings) {
        if (b.type != BuildingType::Barracks || b.isProducing) continue;

        PieceType best = chooseBestUnit(composition, desired, budget);
        int cost = recruitCost(best);

        if (budget >= cost) {
            plan.orders.push_back({b.id, best});
            budget -= cost;
            composition[best]++;  // Mettre à jour pour le prochain choix
        }
    }

    return plan;
}
```

### Composition d'armée désirée selon l'objectif

```cpp
struct ArmyComposition {
    int pawns, knights, bishops, rooks, queens;
};

ArmyComposition computeDesiredComposition(const AITurnContext& ctx,
                                           StrategicObjective obj) {
    switch (obj) {
        case RUSH_ATTACK:
            // Attaque rapide : pions rapides à produire + 1 cavalier
            return {3, 1, 0, 0, 0};

        case ECONOMY_EXPAND:
            // Éco : pions pour occuper les ressources
            return {4, 0, 1, 0, 0};

        case BUILD_ARMY:
            // Armée équilibrée
            return {2, 2, 1, 1, 0};

        case CHECKMATE_PRESS:
            // Siège : tours + cavaliers pour couvrir les fuites
            return {1, 2, 1, 2, 0};

        case PURSUE_QUEEN:
            // Mariage : besoin au minimum d'un fou et un pion
            return {2, 1, 1, 1, 0};  // + queen via mariage

        default:
            return {2, 1, 1, 1, 0};
    }
}
```

### Réserve d'or

```cpp
int computeGoldReserve(const AITurnContext& ctx) {
    // En early game : tout dépenser
    if (ctx.turnNumber <= 8) return 0;

    // Garder assez pour une caserne si on n'en a qu'une
    if (ctx.myBarracksCount <= 1) return 50;

    // En aggression : dépenser agressivement
    if (ctx.phase == AGGRESSION) return 20;

    // Par défaut : garder 30g de tampon
    return 30;
}
```

---

## 9. Phase 8 — Module Construction

### Objectif

Construire intelligemment en tenant compte de la position stratégique.

### Fichier

```
src/AI/AIBuildModule.hpp
src/AI/AIBuildModule.cpp
```

### Logique de construction

```cpp
std::optional<BuildAction> AIBuildModule::suggestBuild(
    const AITurnContext& ctx,
    StrategicObjective objective)
{
    auto& kingdom = ctx.myKingdom;
    int gold = kingdom.gold;

    // PRIORITÉ 1 : Première caserne
    if (ctx.myBarracksCount == 0 && gold >= 50)
        return buildBarracks(ctx, nearestResourceArea(ctx));

    // PRIORITÉ 2 : Deuxième caserne (si revenus suffisants)
    if (ctx.myBarracksCount == 1
        && ctx.myIncomePerTurn >= 15
        && gold >= 50
        && ctx.turnNumber > 10)
        return buildBarracks(ctx, oppositeFlankOfFirst(ctx));

    // PRIORITÉ 3 : Troisième caserne (si cash-flow très bon)
    if (ctx.myBarracksCount == 2
        && ctx.myIncomePerTurn >= 30
        && gold >= 80)
        return buildBarracks(ctx, nearEnemyApproach(ctx));

    // PRIORITÉ 4 : Murs défensifs si menace
    if (objective == DEFEND_KING
        && ctx.enemyPiecesNearMyKing >= 2
        && gold >= 20)
        return buildWall(ctx);

    return std::nullopt;
}
```

### Placement intelligent des casernes

```cpp
BuildAction buildBarracks(const AITurnContext& ctx, Vec2i idealPosition) {
    Vec2i kingPos = ctx.myKing->position;

    // Les casernes doivent être adjacentes au roi
    // Chercher dans un rayon croissant autour de idealPosition
    // mais en vérifiant la contrainte d'adjacence au roi

    // Si le roi est loin de l'ideal, placer à côté du roi
    // en direction de l'ideal
    Vec2i direction = normalize(idealPosition - kingPos);
    Vec2i candidate = kingPos + direction;

    // Tester les positions dans l'ordre de préférence
    for (auto& pos : spiralAround(candidate, 5)) {
        if (canBuildBarracks(ctx.snapshot, kingPos, pos))
            return {BuildingType::Barracks, pos};
    }

    // Fallback : n'importe où adjacent au roi
    for (auto& pos : spiralAround(kingPos, 3)) {
        if (canBuildBarracks(ctx.snapshot, kingPos, pos))
            return {BuildingType::Barracks, pos};
    }

    return {};  // Impossible de construire
}
```

---

## 10. Phase 9 — Module Mariage & Spécial

### Objectif

Garantir que l'IA poursuive le mariage quand c'est avantageux, sans se bloquer.

### Fichier

```
src/AI/AISpecialModule.hpp
src/AI/AISpecialModule.cpp
```

### Logique de poursuite du mariage

Le problème actuel : l'IA abandonne le mariage si les pièces sont à > 8 cases ou si la phase change. Corrections :

```cpp
struct MarriagePlan {
    bool pursuing;
    int kingTargetCell;     // Case d'église pour le roi
    int bishopTargetCell;   // Case d'église pour le fou
    int pawnTargetCell;     // Case d'église pour le pion
    int estimatedTurns;     // Combien de tours pour y arriver
};

MarriagePlan AISpecialModule::evaluateMarriage(const AITurnContext& ctx) {
    MarriagePlan plan{false};

    // Déjà une reine → pas de mariage
    if (ctx.hasQueen) return plan;

    // Pièces nécessaires ?
    auto* king = ctx.myKing;
    auto* bishop = findBestBishop(ctx);   // Le plus proche de l'église
    auto* pawn = findBestPawn(ctx);       // Le plus proche de l'église

    if (!king || !bishop || !pawn) return plan;

    // Calculer la distance totale
    Vec2i churchCenter = ctx.churchPosition;
    auto churchCells = ctx.churchCells;

    // Pour chaque pièce, trouver la case d'église la plus proche
    auto kingCell = closestChurchCell(king->position, churchCells);
    auto bishopCell = closestChurchCell(bishop->position, churchCells);
    auto pawnCell = closestChurchCell(pawn->position, churchCells);

    // Assurer adjacence roi-pion en choisissant des cases adjacentes
    adjustForAdjacency(kingCell, pawnCell, churchCells);

    // Estimation du nombre de tours
    int kingTurns = estimateMoveTurns(king, kingCell);
    int bishopTurns = estimateMoveTurns(bishop, bishopCell);
    int pawnTurns = estimateMoveTurns(pawn, pawnCell);
    plan.estimatedTurns = std::max({kingTurns, bishopTurns, pawnTurns});

    // Le mariage vaut-il le coup ?
    // Reine = 900 de valeur. Coût = N tours de non-combat.
    // Conversion : 1 tour perdu ≈ 50 points d'opportunité
    float marriageValue = 900.0f - plan.estimatedTurns * 50.0f;

    // Si > 0, le mariage vaut le détour
    if (marriageValue > 0) {
        plan.pursuing = true;
        plan.kingTargetCell = kingCell;
        plan.bishopTargetCell = bishopCell;
        plan.pawnTargetCell = pawnCell;
    }

    // Cas spécial : si les 3 pièces sont DÉJÀ sur l'église → marier immédiatement
    if (isOnChurch(king) && isOnChurch(bishop) && isOnChurch(pawn)
        && areAdjacent(king, pawn))
    {
        plan.pursuing = true;
        plan.estimatedTurns = 0;
    }

    return plan;
}
```

### Mouvement de mariage intégré au MCTS

Quand l'objectif est PURSUE_QUEEN, le MCTS va naturellement évaluer les mouvements vers l'église comme ayant un bon score, car :
- La fonction `scoreMoveRelevance()` booste les mouvements vers l'église
- La fonction `evaluate()` donne un bonus `DevelopmentScore` pour une reine

---

## 11. Phase 10 — Anti-freeze et budget temps

### Objectif

**Garantir que le tour IA ne dépasse jamais un temps fixe** (ex: 300ms, configurable).

### Fichier

```
src/AI/TimeBudget.hpp
```

### Implémentation

```cpp
class TimeBudget {
    std::chrono::steady_clock::time_point m_start;
    int m_budgetMs;

public:
    TimeBudget(int budgetMs)
        : m_start(std::chrono::steady_clock::now())
        , m_budgetMs(budgetMs) {}

    int elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_start).count();
    }

    int remainingMs() const {
        return std::max(0, m_budgetMs - elapsedMs());
    }

    bool expired() const { return elapsedMs() >= m_budgetMs; }

    // Vérifie s'il reste au moins N ms
    bool hasAtLeast(int ms) const { return remainingMs() >= ms; }
};
```

### Points de contrôle dans le pipeline

Chaque sous-système vérifie le budget :

```cpp
// Dans MCTS
while (!timer.expired()) {
    iterate();
}

// Dans CheckmateSolver
for (auto& move : candidates) {
    if (timer.expired()) break;
    ...
}

// Dans le pipeline global
if (timer.remainingMs() < 20) {
    // Pas le temps pour MCTS → utiliser heuristique rapide
    fallbackToHeuristic();
}
```

### Configuration

```json
{
  "timing": {
    "max_turn_time_ms": 300,
    "mcts_budget_fraction": 0.6,
    "checkmate_solver_budget_fraction": 0.2,
    "fallback_heuristic_time_ms": 10
  }
}
```

L'allocation du budget :
- **60%** (180ms) pour MCTS
- **20%** (60ms) pour le checkmate solver
- **10%** (30ms) pour l'évaluation stratégique + contexte
- **10%** (30ms) de marge pour l'exécution

---

## 12. Phase 11 — Tests, self-play et calibrage

### Tests unitaires

| Test | Ce qu'il vérifie |
|---|---|
| `test_forward_model_clone` | Le clone est indépendant de l'original |
| `test_forward_model_move` | Les mouvements sont corrects |
| `test_forward_model_capture` | Les captures fonctionnent |
| `test_forward_model_income` | Les revenus sont corrects |
| `test_evaluator_material` | Le score matériel est cohérent |
| `test_evaluator_checkmate` | Mat détecté = score extrême |
| `test_mcts_basic` | MCTS trouve un mat en 1 simple |
| `test_mcts_budget` | MCTS respecte le budget temps |
| `test_checkmate_solver` | Le solver trouve des mats en 1/2/3 connus |
| `test_utility_ai_crisis` | En check, DEFEND_KING est top priorité |
| `test_utility_ai_economy` | Sans revenus, ECONOMY_EXPAND est top priorité |
| `test_production_all_barracks` | Toutes les casernes produisent |

### Scénarios de test

| Scénario | Situation initiale | Comportement attendu |
|---|---|---|
| **Solo roi vs roi** | 2 rois seuls | L'IA cherche des ressources, construit, produit |
| **Rush ennemi** | Joueur attaque vite avec 3 pions | L'IA défend, produit des défenseurs |
| **Roi ennemi coincé** | Roi ennemi dans un coin, IA a 3+ pièces | L'IA fait mat en < 5 tours |
| **Économie longue** | Les deux camps accumulent | L'IA construit plusieurs casernes, crée une armée forte |
| **Poursuite mariage** | IA a roi + fou + pion, église libre | L'IA déplace les 3 pièces vers l'église |
| **Fin de partie** | IA a reine + tour vs roi seul | L'IA fait mat rapidement |
| **Anti-freeze** | 15 pièces de chaque côté | Le tour IA ne dépasse jamais 300ms |

### Self-play automatisé

```cpp
struct SelfPlayResult {
    int whiteWins, blackWins, draws;
    float avgTurns;
    float avgTurnTimeMs;
};

SelfPlayResult runSelfPlay(int numGames, AIConfig configA, AIConfig configB) {
    SelfPlayResult result{};

    for (int i = 0; i < numGames; ++i) {
        Game game;
        game.startNewGame(/*seed=*/i);

        AIDirector aiWhite(configA, KingdomId::White);
        AIDirector aiBlack(configB, KingdomId::Black);

        while (!game.isOver() && game.getTurnNumber() < 200) {
            if (game.getActiveKingdom() == KingdomId::White)
                aiWhite.computeTurn(game);
            else
                aiBlack.computeTurn(game);
            game.commitTurn();
        }

        // Enregistrer le résultat
        if (game.getWinner() == KingdomId::White) result.whiteWins++;
        else if (game.getWinner() == KingdomId::Black) result.blackWins++;
        else result.draws++;

        result.avgTurns += game.getTurnNumber();
    }

    result.avgTurns /= numGames;
    return result;
}
```

Utiliser le self-play pour **calibrer les poids** de la fonction d'évaluation et les seuils de la Utility AI.

### Métriques de performance

| Métrique | Cible |
|---|---|
| Taux de victoire vs IA actuelle | > 80% |
| Temps moyen par tour | < 200ms |
| Temps max par tour | < 400ms |
| Tours moyens pour conclure (roi ennemi seul) | < 15 |
| Tours avant 1re caserne | ≤ 5 |
| Casernes exploitées / casernes possédées | > 80% |
| Fréquence de freeze (> 1s) | 0% |

---

## 13. Résumé des fichiers à créer/modifier

### Nouveaux fichiers

| Fichier | Rôle |
|---|---|
| `src/AI/ForwardModel.hpp/cpp` | Simulation d'état (clone, appliquer, évaluer) |
| `src/AI/GameSnapshot.hpp/cpp` | État léger clonable du jeu |
| `src/AI/AIDirector.hpp/cpp` | Orchestrateur principal (remplace `AIController`) |
| `src/AI/AIStrategy.hpp/cpp` | Couche stratégie (Utility AI, remplace `AIBrain`) |
| `src/AI/AIMCTS.hpp/cpp` | Monte Carlo Tree Search |
| `src/AI/CheckmateSolver.hpp/cpp` | Détection de mat en 1/2/3 |
| `src/AI/AIEconomyModule.hpp/cpp` | Gestion économie + production |
| `src/AI/AIBuildModule.hpp/cpp` | Placement de bâtiments |
| `src/AI/AISpecialModule.hpp/cpp` | Mariage et upgrades |
| `src/AI/TimeBudget.hpp` | Garde-fou temporel |

### Fichiers à modifier

| Fichier | Modification |
|---|---|
| `src/AI/AIEvaluator.hpp/cpp` | Refonte complète (7 composantes) |
| `src/AI/AITurnContext.hpp/cpp` | Étendre avec snapshot, scores pré-calculés |
| `src/AI/ThreatMap.hpp` | Inchangé (déjà performant) |
| `src/Systems/TurnSystem.hpp/cpp` | Autoriser N productions (1 par caserne, pas 1 globale) |
| `src/Core/Game.hpp/cpp` | Brancher `AIDirector` au lieu de `AIController` |
| `assets/config/ai_params.json` | Ajouter paramètres MCTS, timing, évaluation |

### Fichiers qui deviennent obsolètes

| Fichier | Remplacé par |
|---|---|
| `src/AI/AIController.hpp/cpp` | `AIDirector` |
| `src/AI/AIBrain.hpp/cpp` | `AIStrategy` |
| `src/AI/AIStrategyMove.hpp/cpp` | `AIMCTS` + `AIEconomyModule` |
| `src/AI/AIStrategyBuild.hpp/cpp` | `AIBuildModule` |
| `src/AI/AIStrategyEcon.hpp/cpp` | `AIEconomyModule` |
| `src/AI/AIStrategySpecial.hpp/cpp` | `AISpecialModule` |
| `src/AI/AITacticalEngine.hpp/cpp` | `AIMCTS` + `CheckmateSolver` |

> **Recommandation** : ne pas supprimer les anciens fichiers immédiatement. Les garder en parallèle et implémenter un flag `USE_NEW_AI` dans la config pour basculer entre l'ancienne et la nouvelle IA pendant le développement.

---

## 14. Paramètres et configuration

### Nouvelle structure `ai_params.json`

```json
{
  "__comment": "Configuration IA — A Normal Chess Game",

  "general": {
    "use_new_ai": true,
    "randomness": 0.15,
    "log_decisions": false
  },

  "timing": {
    "max_turn_time_ms": 300,
    "mcts_budget_fraction": 0.6,
    "checkmate_solver_budget_ms": 60,
    "fallback_heuristic_ms": 10
  },

  "mcts": {
    "exploration_constant": 1.41,
    "rollout_depth": 6,
    "max_children_per_node": 30,
    "max_iterations": 500,
    "relevance_threshold": 5.0
  },

  "evaluation": {
    "piece_values": {
      "pawn": 100,
      "knight": 320,
      "bishop": 330,
      "rook": 500,
      "queen": 900,
      "king": 10000
    },
    "weights_by_phase": {
      "EARLY_GAME":  {"material":1.0, "economy":3.0, "map":2.0, "king_safety":1.0, "development":2.0, "threat":0.5, "checkmate":0.3},
      "BUILD_UP":    {"material":1.5, "economy":2.0, "map":1.5, "king_safety":1.0, "development":1.5, "threat":1.0, "checkmate":0.5},
      "MID_GAME":    {"material":2.0, "economy":1.0, "map":1.0, "king_safety":1.5, "development":1.0, "threat":1.5, "checkmate":1.5},
      "AGGRESSION":  {"material":2.0, "economy":0.3, "map":0.5, "king_safety":1.5, "development":0.3, "threat":2.0, "checkmate":3.0},
      "ENDGAME":     {"material":1.5, "economy":0.2, "map":0.3, "king_safety":2.0, "development":0.2, "threat":2.0, "checkmate":4.0},
      "CRISIS":      {"material":0.5, "economy":0.1, "map":0.1, "king_safety":5.0, "development":0.1, "threat":0.5, "checkmate":0.5}
    }
  },

  "strategy": {
    "gold_reserve_early": 0,
    "gold_reserve_mid": 30,
    "gold_reserve_aggression": 20,
    "max_barracks": 3,
    "second_barracks_income_threshold": 15,
    "third_barracks_income_threshold": 30,
    "marriage_opportunity_cost_per_turn": 50,
    "marriage_value": 900
  },

  "production": {
    "backlog_threshold_attack": 5,
    "backlog_threshold_normal": 8,
    "desired_composition": {
      "rush":     {"pawns":3, "knights":1, "bishops":0, "rooks":0},
      "balanced": {"pawns":2, "knights":2, "bishops":1, "rooks":1},
      "siege":    {"pawns":1, "knights":2, "bishops":1, "rooks":2},
      "economy":  {"pawns":4, "knights":0, "bishops":1, "rooks":0}
    }
  },

  "checkmate_solver": {
    "mate_in_1_always": true,
    "mate_in_2_budget_ms": 40,
    "mate_in_3_budget_ms": 80
  }
}
```

---

## Annexe A — Valeurs de référence du jeu

| Donnée | Valeur |
|---|---|
| Plateau | Circulaire, rayon 25 (diamètre 50) |
| Mines | 2 sur la carte, 6×6 cases, 10g/case/tour |
| Fermes | 3 sur la carte, 4×3 cases, 5g/case/tour |
| Église | 1 (centre), 4×3 cases |
| Arène | Générée, 3×3 cases, 10XP/tour/pièce |
| Caserne | 4×2 cases, coût 50g |
| Mur de bois | 1×1, 1PV, coût 20g |
| Mur de pierre | 1×1, 3PV, coût 40g |
| Pion | 10g, 2 tours de production, 100 valeur |
| Cavalier | 30g, 4 tours de production, 320 valeur |
| Fou | 30g, 4 tours de production, 330 valeur |
| Tour | 60g, 6 tours de production, 500 valeur |
| Reine | Mariage uniquement, 900 valeur |
| Portée max | 8 cases (fou, tour, reine) |
| Pion | 1 case, 4 directions (haut/bas/gauche/droite) |
| Cavalier | L standard (8 positions) |
| Or initial | 0 |
| Actions/tour | 1 mouvement + 1 construction + N productions (1/caserne) + 1 mariage |

---

## Annexe B — Pseudo-code des algorithmes clés

### B.1 — Pipeline complet d'un tour IA

```
FONCTION computeTurn(game):
    timer ← TimeBudget(300ms)

    // 1. Contexte
    ctx ← construireContexte(game)
    snapshot ← ForwardModel.createSnapshot(game)

    // 2. Mat en 1 immédiat ?
    SI matEn1 ← CheckmateSolver.findMateIn1(ctx):
        exécuter(matEn1)
        lancerToutesProductions(game)
        RETOURNER

    // 3. Stratégie
    plan ← AIStrategy.computePlan(ctx)

    // 4. Tactique (MCTS)
    SI timer.restant() > 50ms:
        action ← MCTS.search(snapshot, plan.objectif, timer.restant() - 30ms)
        SI action.type == MOVE:
            exécuter(action)

    // 5. Construction
    SI plan.doitConstruire ET PAS déjàConstruit:
        build ← AIBuildModule.suggest(ctx, plan.objectif)
        SI build: exécuter(build)

    // 6. Toutes les productions
    SI plan.doitProduire:
        prodPlan ← AIEconomyModule.planProduction(ctx, plan.objectif)
        POUR CHAQUE (barracksId, type) DANS prodPlan.orders:
            exécuter(Produce(barracksId, type))

    // 7. Mariage
    SI plan.doitMarier ET conditionsReunies():
        exécuter(Marry)

    // 8. Fallback : si aucun mouvement n'a été fait
    SI PAS aMouvementé:
        // Heuristique rapide : meilleur coup local
        coup ← heuristiqueFallback(ctx)
        SI coup: exécuter(coup)
```

### B.2 — MCTS (simplifié)

```
FONCTION MCTS.search(snapshot, objectif, budgetMs):
    root ← NœudMCTS(snapshot)
    timer ← TimeBudget(budgetMs)

    TANT QUE PAS timer.expiré():
        // Sélection (UCB1)
        node ← root
        TANT QUE node.enfants non vide ET node.complètementExpansé:
            node ← enfantAvecMeilleurUCB1(node)

        // Expansion
        SI node.nonTerminal():
            actions ← générerActionsCandidates(node.état, objectif)
            action ← choisirActionNonExplorée(actions)
            enfant ← NœudMCTS(appliquer(node.état, action))
            node.enfants.ajouter(enfant)
            node ← enfant

        // Simulation (rollout)
        score ← rollout(node.état, profondeur=6)

        // Rétropropagation
        TANT QUE node ≠ null:
            node.visites += 1
            node.scoreTotal += score
            node ← node.parent

    RETOURNER root.enfants.maxPar(visites).action
```

### B.3 — Utility AI (sélection d'objectif)

```
FONCTION choisirObjectif(ctx):
    candidats ← []

    candidats.ajouter(RUSH_ATTACK,      scoreRush(ctx))
    candidats.ajouter(ECONOMY_EXPAND,    scoreÉconomie(ctx))
    candidats.ajouter(BUILD_ARMY,        scoreArmée(ctx))
    candidats.ajouter(BUILD_INFRA,       scoreInfra(ctx))
    candidats.ajouter(PURSUE_QUEEN,      scoreMarriage(ctx))
    candidats.ajouter(DEFEND_KING,       scoreDéfense(ctx))
    candidats.ajouter(CHECKMATE_PRESS,   scoreMat(ctx))
    candidats.ajouter(CONTEST_RESOURCES, scoreContest(ctx))

    trier(candidats, parScoreDécroissant)

    // Variation stochastique légère
    SI randomness > 0 ET diff(top1, top2) < 10:
        SI random() < 0.3: RETOURNER top2

    RETOURNER top1
```

### B.4 — Checkmate Solver (mat en 1 optimisé)

```
FONCTION findMateIn1(ctx):
    roiEnnemi ← ctx.enemyKingPosition
    casesFuite ← calculerCasesFuite(roiEnnemi)

    POUR CHAQUE (pièceId, coups) DANS ctx.selfMoves:
        pièce ← ctx.trouverPièce(pièceId)
        SI pièce.type == ROI: CONTINUER

        POUR CHAQUE dest DANS coups:
            SI dest == roiEnnemi: CONTINUER    // Pas capture roi

            // Filtre rapide : donne-t-il échec ?
            SI PAS donneÉchec(pièce, dest, roiEnnemi): CONTINUER

            // Simulation
            sim ← ctx.snapshot.clone()
            appliquerMouvement(sim, pièceId, dest)

            // Le roi peut-il fuir ?
            fuitePossible ← FAUX
            POUR CHAQUE fuite DANS casesFuite:
                SI estFuiteLégale(sim, roiEnnemi, fuite):
                    fuitePossible ← VRAI
                    SORTIR

            // Un défenseur peut-il interposer/capturer ?
            SI PAS fuitePossible:
                fuitePossible ← peutDéfendreÉchec(sim)

            SI PAS fuitePossible:
                RETOURNER (pièceId, dest)    // MAT !

    RETOURNER RIEN
```

---

## Ordre d'implémentation recommandé

| Étape | Phase | Durée estimée | Prérequis |
|---|---|---|---|
| 1 | Forward Model (Phase 1) | — | Aucun |
| 2 | Évaluateur refondé (Phase 2) | — | Phase 1 |
| 3 | TimeBudget (Phase 10) | — | Aucun |
| 4 | Checkmate Solver (Phase 6) | — | Phase 1 |
| 5 | Utility AI — Stratégie (Phase 3) | — | Phase 1, 2 |
| 6 | MCTS (Phase 4) | — | Phase 1, 2 |
| 7 | AIDirector — Pipeline (Phase 5) | — | Phase 3, 4, 5, 6 |
| 8 | Module Économie (Phase 7) | — | Phase 5 |
| 9 | Module Construction (Phase 8) | — | Phase 5 |
| 10 | Module Spécial (Phase 9) | — | Phase 5 |
| 11 | Modifier TurnSystem pour N productions | — | Aucun |
| 12 | Tests & Self-play (Phase 11) | — | Tout le reste |

Les étapes 1, 3, et 11 sont indépendantes et peuvent être menées en parallèle.

---

*Document rédigé pour le projet "A Normal Chess Game" — plan d'implémentation de l'IA hybride Utility AI + MCTS.*
