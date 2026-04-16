# Plan de refactoring de la codebase — A Normal Chess Game

---

## Objectif

Ce document definit un plan complet, progressif et exploitable pour faire evoluer la codebase vers une architecture plus propre, plus testable et plus maintenable, sans casser le jeu en cours de route.

L'objectif n'est **pas** de re-ecrire le projet from scratch, mais de :

- conserver les briques solides deja en place ;
- reduire les zones de couplage fort ;
- eliminer la dette structurelle qui ralentit les evolutions ;
- clarifier les responsabilites entre moteur, logique, IA, rendu, UI et runtime applicatif ;
- rendre le depot plus propre et plus proche d'un standard de production.

Ce plan est volontairement **incremental** : chaque phase doit laisser une base compilable, testable et jouable.

---

## Statut d'implementation

### Tranche deja executee

Les points suivants ont deja ete implementes dans la codebase :

- build CMake passe d'une collecte automatique globale des `.cpp` a une liste explicite des sources ;
- `.gitignore` a ete durci pour mieux couvrir les dossiers de build, l'environnement local et les dumps de debug ;
- `ToolState` a ete sorti de `Input` vers une couche `Core`, ce qui supprime une dependance directe `Core -> Input` ;
- `InputSelectionBookmark` a ete deplace hors de `Core` vers `Input`, ce qui retire un type de selection frontend du coeur runtime ;
- l'execution asynchrone de l'IA a ete extraite de `Game` dans `Core/AITurnRunner` ;
- le runtime actif du jeu n'utilise plus le chemin legacy `AIController` et repose uniquement sur `AIDirector` ;
- un `TurnValidationContext` partage a ete introduit pour centraliser les dependances de validation/projection de tour ;
- `PendingTurnProjection`, `TurnSystem`, `InputHandler` et la planification legacy de `AIController` consomment maintenant ce contexte partage sur leurs chemins principaux ;
- `Game` expose maintenant un helper unique de contexte autoritaire pour supprimer les signatures de validation les plus verbeuses ;
- `CheckResponseRules`, `SelectionMoveRules`, `BuildOverlayRules` et `InGameViewModelBuilder` ont ete rebranches sur ce contexte partage sur leurs chemins actifs ;
- `InputHandler` ne conserve plus de pointeurs persistants de selection et stocke des identifiants stables pour les pieces et batiments ;
- `Game` resout maintenant la selection affichee a partir de ces identifiants, ce qui retire une source de pointeurs fragiles entre input, rendu et UI ;
- une partie de la logique d'actions UI a ete extraite des grosses lambdas de `setupUICallbacks()` vers des methodes dediees de `Game` ;
- `GameEngine` construit maintenant son propre contexte autoritaire de validation de tour, valide le tour pending et execute le flux coeur `commit -> advance -> evaluation de fin de partie` ;
- `Game` delegue desormais ce flux coeur a `GameEngine`, et le staging autoritaire d'un lot de commandes pending passe aussi par le moteur ;
- `GameEngine` prepare maintenant aussi les tours pending de l'IA, y applique les replis de securite partages (reponse minimale au check, disbands anti-faillite) et laisse `Game` se limiter au polling async, au logging et au commit ;
- un `MultiplayerRuntime` dedie encapsule maintenant client/serveur LAN, hint d'hote, etat de reconnexion et handshake de jointure, ce qui retire de `Game` une partie substantielle de la plomberie reseau ;
- un `MultiplayerJoinCoordinator` dedie encapsule maintenant la reconstruction de requete de reconnexion, la preparation locale avant jointure client et le flux handshake+restore du snapshot LAN, ce qui retire de `Game` le coeur du flux de jointure/reconnexion client ;
- un `MultiplayerEventCoordinator` dedie encapsule maintenant la planification des evenements host/client LAN, la restauration de snapshot client et le cleanup de deconnexion cote client, ce qui retire de `Game` le coeur du switch evenementiel reseau ;
- un `MultiplayerRuntimeCoordinator` dedie encapsule maintenant le polling runtime des evenements LAN, l'application concrete des plans host/client, et la presentation des alertes/reconnexions multiplayer, ce qui retire de `Game` le coeur du flux runtime multiplayer restant ;
- un `InputCoordinator` dedie encapsule maintenant le routage pre/post-GUI des evenements SFML, les raccourcis gameplay bloques et la decision d'injecter ou non un evenement dans le monde, ce qui retire de `Game` le coeur de la boucle de filtrage d'input ;
- un `RenderCoordinator` dedie encapsule maintenant la planification des overlays monde, previews de build, marqueurs d'actions pending et frames de selection, ainsi que leur application au renderer, ce qui retire de `Game` le coeur de l'orchestration de rendu monde ;
- un `UpdateCoordinator` dedie encapsule maintenant la planification du tick runtime (camera, reseau, IA, UI), ce qui retire de `Game` le coeur de l'orchestration de la boucle `update` ;
- un `TurnCoordinator` dedie encapsule maintenant l'execution du commit autoritaire, la planification des suites de commit, la soumission de tour client LAN et la validation/application d'un tour distant host, ce qui retire de `Game` le coeur du flux de tour reseau et de commit ;
- un `TurnLifecycleCoordinator` dedie encapsule maintenant l'application runtime des commits/soumissions/resets de tour, la presentation des erreurs de soumission client LAN et la reconciliation draft/selection autour de `TurnCoordinator`, ce qui retire de `Game` le coeur restant du cycle de tour local ;
- un `AITurnCoordinator` dedie encapsule maintenant le demarrage, le polling, le filtrage des resultats obsoletes, le logging et le staging des tours IA asynchrones, ce qui retire de `Game` le coeur de l'orchestration runtime des tours IA ;
- un `InGamePresentationCoordinator` dedie encapsule maintenant la planification du menu in-game et l'orchestration concrete HUD/dashboard/toolbar/panneaux lateraux via `FrontendCoordinator`, ce qui retire de `Game` le coeur de la presentation in-game courante ;
- un `PanelActionCoordinator` dedie encapsule maintenant l'annulation de constructions queued, les upgrades, les sacrifices et la production depuis les panneaux UI, ce qui retire de `Game` le coeur des actions gameplay declenchees par les panneaux lateraux ;
- un `UICallbackBinder` dedie encapsule maintenant le branchement concret des callbacks `UIManager` vers des closures de runtime, ce qui retire de `Game` la plomberie repetitive de registration des `setOn...` ;
- un `UICallbackCoordinator` dedie encapsule maintenant la construction des callbacks runtime `game menu / main menu / HUD / toolbar / panneaux`, y compris leurs guards d'etat/permissions et l'ouverture du panneau de build, ce qui retire de `Game` le coeur restant du wiring UI ;
- un `SelectionQueryCoordinator` dedie encapsule maintenant la resolution des selections affichees et des bookmarks par IDs/metadonnees de batiments sur la vue projetee, ce qui retire de `Game` le coeur de la recherche d'entites affichees ;
- un `TurnDraftCoordinator` dedie encapsule maintenant la politique d'activation, d'invalidation et de rebuild du draft local projete, ainsi que la reconciliation de selection associee, ce qui retire de `Game` le coeur du cycle de vie du `TurnDraft` ;
- un `BuildOverlayCoordinator` dedie encapsule maintenant le cache de preview de build, son invalidation et son rafraichissement via `BuildOverlayRules`, ce qui retire de `Game` le coeur de la maintenance du cache d'overlay de build ;
- un `SessionFlow` dedie encapsule maintenant le coeur du cycle de vie start/load/save des sessions autoritaires, y compris la restauration moteur, l'auto-host LAN associe et l'initialisation du debug snapshot, ce qui retire une partie du flux de session hors de `Game` ;
- un `SessionPresentationCoordinator` dedie encapsule maintenant les resets et la presentation post-start/post-load/retour menu autour de `SessionFlow`, ce qui retire de `Game` le coeur des transitions visuelles de session ;
- un `SessionRuntimeCoordinator` dedie encapsule maintenant l'application runtime des transitions `start/load/join/reconnect/retour menu` autour de `SessionFlow`, `SessionPresentationCoordinator` et `MultiplayerJoinCoordinator`, ce qui retire de `Game` le coeur restant des transitions de session ;
- un `FrontendCoordinator` dedie encapsule maintenant les regles frontend pures de perspective locale, permissions d'interaction, construction d'`InputContext`, presentation HUD locale, composition du dashboard in-game et routage des panneaux lateraux, en plus de la presentation reseau/UI, ce qui retire un bloc significatif de logique de coordination hors de `Game` ;
- des tests moteur couvrent desormais explicitement le staging IA en sortie de check et en recuperation de faillite ;
- des tests couvrent maintenant aussi le cycle de vie de l'etat de reconnexion du runtime multiplayer extrait ;
- des tests couvrent maintenant aussi la reconstruction de requete de reconnexion, la preparation locale de jointure et le rejet d'une jointure invalide extraits dans `MultiplayerJoinCoordinator` ;
- des tests couvrent maintenant aussi les plans d'alerte et les transitions client extraites dans `MultiplayerEventCoordinator` ;
- des tests couvrent maintenant aussi les decisions extraites dans `InputCoordinator` pour les raccourcis gameplay et le routage de l'input monde apres filtrage GUI ;
- des tests couvrent maintenant aussi les plans d'overlays monde extraits dans `RenderCoordinator` pour la selection, les commandes pending et les previews de build ;
- des tests couvrent maintenant aussi les decisions extraites dans `UpdateCoordinator` pour les ticks `playing`, `paused` et `menu reseau` ;
- des tests couvrent maintenant aussi les plans de commit et le flux reseau de tour extraits dans `TurnCoordinator` ;
- des tests couvrent maintenant aussi les transitions du menu in-game extraites dans `InGamePresentationCoordinator` ;
- des tests couvrent maintenant aussi le toggling des upgrades, sacrifices et productions extraits dans `PanelActionCoordinator` ;
- des tests couvrent maintenant aussi un round-trip `start -> save -> load` via `SessionFlow` sur un repertoire de sauvegarde isole ;
- des tests couvrent maintenant aussi les plans de reset/session/menu extraits dans `SessionPresentationCoordinator` ;
- des tests couvrent maintenant aussi les regles extraites de `FrontendCoordinator` pour le HUD local, le verrouillage des commandes pendant l'attente de confirmation distante, le dashboard projete et le routage des panneaux lateraux ;
- des tests couvrent maintenant aussi le cycle de vie du cache extrait dans `BuildOverlayCoordinator` pour le calcul, le clear sous verrou d'actions et l'invalidation hors mode build ;
- la build et les tests ont ete revalidees apres cette tranche.

### Consequence sur la roadmap

- **Phase 1** : partiellement realisee ;
- **Phase 2** : avancee via l'extraction de l'execution IA asynchrone, l'introduction d'un contexte partage de validation de tour, la reduction du couplage de `Game` aux appels de validation, le transfert du commit autoritaire du tour dans `GameEngine` et la sortie du fallback detaille de tour IA hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via une premiere extraction explicite de la plomberie LAN dans `MultiplayerRuntime`, qui absorbe des pans du host/client runtime et du handshake de connexion ;
- **Phase 2** : avancee aussi via l'extraction de `MultiplayerJoinCoordinator`, qui absorbe le coeur du flux de jointure/reconnexion client LAN hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `MultiplayerEventCoordinator`, qui absorbe le coeur de la reaction aux evenements LAN et des resets client hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `MultiplayerRuntimeCoordinator`, qui absorbe le polling runtime LAN, l'application concrete des evenements host/client et les alertes de reconnexion hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `InputCoordinator`, qui absorbe le coeur du filtrage d'input gameplay et du routage evenementiel pre/post GUI hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `RenderCoordinator`, qui absorbe le coeur de la planification de rendu monde et des overlays hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `UpdateCoordinator`, qui absorbe le coeur des decisions de tick runtime hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `TurnCoordinator`, qui absorbe le coeur du commit autoritaire, de la soumission client et de l'application d'un tour LAN distant hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `TurnLifecycleCoordinator`, qui absorbe l'application runtime des commits/soumissions/resets de tour et la presentation des echecs de soumission client hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `AITurnCoordinator`, qui absorbe le coeur du demarrage/polling/staging/logging des tours IA asynchrones hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `InGamePresentationCoordinator`, qui absorbe le coeur de la presentation in-game et des transitions du menu hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `PanelActionCoordinator` et `UICallbackBinder`, qui absorbent les actions gameplay issues des panneaux UI et la plomberie repetitive de branchement des callbacks hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `UICallbackCoordinator`, qui absorbe la construction des callbacks runtime UI et leurs guards hors de `Game.cpp` ;
- **Phase 5** : avancee aussi via l'extraction de `SelectionQueryCoordinator`, qui absorbe la resolution de selection projetee/affichee et la reconciliation par bookmark hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `TurnDraftCoordinator`, qui absorbe le cycle de vie du draft local projete et sa reconciliation de selection hors de `Game.cpp` ;
- **Phase 2** : avancee aussi via l'extraction de `BuildOverlayCoordinator`, qui absorbe le cache runtime d'overlay de build hors de `Game.cpp` ;
- **Phase 2** : avancee egalement via une premiere extraction explicite du cycle de vie de session dans `SessionFlow`, qui absorbe le noyau `start/load/save` hors de `Game.cpp` ;
- **Phase 2** : avancee egalement via l'extraction de `SessionPresentationCoordinator`, qui absorbe les transitions de presentation autour des entrees/sorties de session hors de `Game.cpp` ;
- **Phase 2** : avancee egalement via l'extraction de `SessionRuntimeCoordinator`, qui absorbe l'application runtime des transitions `start/load/join/reconnect/retour menu` hors de `Game.cpp` ;
- **Phase 2** : avancee egalement via l'extraction et l'extension d'un noyau `FrontendCoordinator`, qui absorbe une partie des helpers de permissions/input/presentation puis le coeur du dashboard et du choix de panneau lateral hors de `Game.cpp` ;
- **Phase 3** : amorcee concretement via la convergence de `Game`, `PendingTurnProjection`, `TurnSystem`, `CheckResponseRules`, `SelectionMoveRules` et `BuildOverlayRules` autour du meme contexte de validation ;
- **Phase 4** : avancee via la suppression de la dependance `Core -> Input/ToolState`, la sortie de `InputSelectionBookmark` hors de `Core` et l'abandon des pointeurs persistants de selection dans `InputHandler` ;
- **Phase 6** : amorcee via l'abandon du chemin runtime `AIController` et le recentrage du staging autoritaire des tours IA autour de `GameEngine` + `AIDirector`.
- **Phase 8** : amorcee plus concretement via l'isolement du flux de jointure LAN et de l'etat de reconnexion dans une runtime multiplayer dediee.

Le reste du document decrit la suite du chantier.

---

## Table des matieres

- [1. Principes directeurs](#1-principes-directeurs)
- [2. Diagnostic synthetique](#2-diagnostic-synthetique)
- [3. Architecture cible](#3-architecture-cible)
- [4. Regles de dependance a imposer](#4-regles-de-dependance-a-imposer)
- [5. Roadmap de refactoring](#5-roadmap-de-refactoring)
- [6. Chantier detaille par zone](#6-chantier-detaille-par-zone)
- [7. Nettoyage du code mort et des reliquats](#7-nettoyage-du-code-mort-et-des-reliquats)
- [8. Organisation cible des dossiers](#8-organisation-cible-des-dossiers)
- [9. Nommage et conventions a harmoniser](#9-nommage-et-conventions-a-harmoniser)
- [10. Strategie de test et de validation](#10-strategie-de-test-et-de-validation)
- [11. Metriques cibles de fin de chantier](#11-metriques-cibles-de-fin-de-chantier)
- [12. Ordre d'execution recommande](#12-ordre-dexecution-recommande)

---

## 1. Principes directeurs

1. **Refactorer sans regressions**
   - toute refonte structurelle importante doit etre accompagnee de tests ou d'un protocole de verification reproductible ;
   - on ne deplace pas de logique critique sans verrouiller son comportement.

2. **Separer moteur et application**
   - le coeur du jeu ne doit pas dependre de SFML, TGUI, ni de la notion de widget ou d'outil UI ;
   - le runtime applicatif peut dependre du frontend, pas l'inverse.

3. **Conserver une seule source de verite par regle**
   - une validation de commande ne doit pas exister dans trois classes differentes ;
   - une regle d'economie ou de mouvement doit etre centralisee et re-utilisee partout.

4. **Privilegier les donnees stables aux pointeurs vivants**
   - toute selection, preview ou reference longue duree doit s'appuyer sur des identifiants ou des snapshots, pas sur des pointeurs fragiles.

5. **Rendre chaque couche testable isolee**
   - les Systems, la projection de tour, l'IA et les regles de mouvement doivent pouvoir etre testes sans lancer la boucle complete du jeu.

6. **Eviter le grand soir**
   - chaque phase doit etre livrable seule ;
   - les gros remplacements doivent etre caches derriere des interfaces ou des couches de compatibilite temporaires.

---

## 2. Diagnostic synthetique

## Forces a preserver

- La separation entre `GameEngine` et le runtime applicatif est une bonne base.
- Le projet dispose deja de modules metier distincts : `Board`, `Units`, `Buildings`, `Kingdom`, `Systems`, `AI`, `Save`, `Multiplayer`, `UI`, `Render`.
- Le couple `TurnDraft` / projection locale est une excellente direction pour distinguer etat autoritaire et etat affiche.
- Le pattern `InGameViewModel` / `InGameViewModelBuilder` va dans le bon sens pour decorreler l'UI du moteur.
- L'IA dispose deja d'une base solide avec `ForwardModel`, `GameSnapshot`, `AIDirector`, `AIMCTS`, `CheckmateSolver`.
- Les regles de jeu ont deja commence a etre extraites dans des modules dedies (`EconomySystem`, `PublicBuildingOccupation`, `ProductionSpawnRules`, `CheckResponseRules`, etc.).
- La presence d'un coeur `ANormalChessGameCore` est une bonne fondation pour tester et factoriser.

## Problemes structurants prioritaires

| Priorite | Sujet | Symptome | Impact |
|---|---|---|---|
| P1 | `Game.cpp` / `Game.hpp` trop centraux | boucle, input, UI, rendu, IA async, save/load, LAN, overlays, etat local | couplage fort, fichier difficile a faire evoluer |
| P1 | pipeline de validation du tour duplique | `TurnSystem`, `PendingTurnProjection`, `CheckResponseRules`, logique indirecte dans l'UI/preview | divergence potentielle des regles |
| P1 | double chemin IA | `AIController` + `AIDirector`, flag `useNewAI` | maintenance double, comportement difficile a raisonner |
| P1 | frontiere moteur / frontend floue | `Core` depend de `Input/ToolState`, plusieurs types SFML remontent haut | architecture moins propre, tests plus durs |
| P2 | stockage de selection par pointeurs | `InputHandler` memorise `Piece*` et `Building*` | references fragiles lors des projections, annulations, commits |
| P2 | build/repo hygiene | artefacts runtime, debug dumps, dossiers vides, glob de sources | depot plus bruyant, build moins maitrisable |
| P2 | tests trop concentres | `tests/TestMain.cpp` grossit avec tout | lecture difficile, faible granularite |
| P3 | nommage heterogene | `Rules`, `System`, `OverlayRules`, `TacticalEngine` | comprehension moins immediate |

---

## 3. Architecture cible

L'architecture cible recommandee reste pragmatique : on conserve les grands domaines deja presents, mais on clarifie les frontieres.

### Vue logique cible

```text
Application / Runtime
    ├─ boucle principale
    ├─ orchestration des sessions
    ├─ coordination frontend
    ├─ execution async de l'IA
    └─ integration multiplayer

Core / Engine
    ├─ GameEngine
    ├─ GameSessionConfig
    ├─ GameStateValidator
    ├─ TurnDraft
    └─ modeles runtime-agnostiques

Domain
    ├─ Board
    ├─ Units
    ├─ Buildings
    └─ Kingdom

Systems
    ├─ validation des commandes
    ├─ projection de tour
    ├─ economie
    ├─ production
    ├─ combat
    ├─ check / reponse au check
    └─ occupation / integrite / spawn

AI
    ├─ simulation snapshot
    ├─ evaluation
    ├─ planification
    └─ modules specialises

Frontend
    ├─ Input
    ├─ UI
    └─ Render

Infrastructure
    ├─ Save
    ├─ Multiplayer
    └─ Assets / Config
```

### Intention de cette architecture

- `Application / Runtime` pilote le jeu mais ne porte pas les regles metier.
- `Core` expose l'etat, les contrats et les mecanismes de session, sans connaitre le frontend.
- `Systems` devient la couche unique des regles et validations.
- `AI` s'appuie sur les memes regles que le runtime, via snapshots ou adaptateurs, sans re-coder les regles.
- `Frontend` ne manipule plus directement les entites du moteur sur la duree.

---

## 4. Regles de dependance a imposer

Ces regles doivent devenir des conventions fermes du projet.

### Regle 1 — `Core` ne depend plus de `Input`, `UI` ni `Render`

Concretement :

- `Core/InteractionPermissions.hpp` ne doit plus inclure `Input/ToolState.hpp` ;
- tout type commun de selection ou d'outil doit etre deplace dans une couche neutre, ou remonte dans `Application` / `Frontend`.

### Regle 2 — les Systems ne dependent pas de widgets ni de rendu

- aucune regle de jeu ne doit appeler l'UI ou raisonner en terme de panneau ou d'overlay.

### Regle 3 — l'UI consomme des view models, pas des objets metier bruts

- `UIManager`, les panels, et idealement `Renderer` doivent consommer des DTO/view models ou des structures d'affichage ;
- ils ne doivent plus garder des references longues sur `Piece`, `Building` ou `Kingdom`.

### Regle 4 — une commande de tour suit un seul pipeline

Toutes les validations de commande doivent passer par une meme couche, re-utilisee par :

- le joueur local ;
- l'IA ;
- la projection locale ;
- le multiplayer ;
- le commit autoritaire.

### Regle 5 — l'IA ne re-code pas les regles du jeu

- si l'IA a besoin d'une information de mouvement, de traversabilite, de revenus, de production ou de check, elle doit utiliser une API partagee ou un adaptateur sur les regles existantes.

---

## 5. Roadmap de refactoring

Le chantier est decoupe en 8 phases principales. Chaque phase doit produire une base stable.

## Phase 0 — Stabilisation initiale et garde-fous

### Objectif

Figurer un point de depart propre avant toute refonte structurelle.

### Actions

- figer un etat de reference compilable ;
- lancer et documenter un smoke test manuel minimal ;
- documenter 5 a 10 scenarios critiques a rejouer apres chaque phase :
  - creation de partie ;
  - load/save ;
  - tour joueur ;
  - tour IA ;
  - construction ;
  - production ;
  - echec / mat / validation ;
  - LAN host/client si disponible.

### Livrables

- une checklist de regression ;
- un point de depart de build/test stable.

### Critere de sortie

- l'equipe sait valider rapidement qu'une phase n'a pas casse le jeu.

---

## Phase 1 — Hygiene du depot et du build

### Objectif

Nettoyer l'environnement de travail pour que le refactoring ne soit pas parasite par des artefacts ou par une structure de build trop permissive.

### Actions prioritaires

1. **Nettoyer le depot des artefacts runtime et generes**
   - ne plus versionner `build/`, `build-msvc/`, `debug_game_state/`, les saves runtime et autres sorties temporaires ;
   - clarifier si certains JSON de `saves/` sont des fixtures de test ou de vraies sorties runtime ;
   - documenter le statut de `generator/` ou le laisser hors perimetre produit.

2. **Durcir `.gitignore`**
   - couvrir au minimum :
     - `build/`
     - `build-msvc/`
     - `debug_game_state/`
     - sorties runtime temporaires dans `saves/`
     - eventuels logs temporaires

3. **Remplacer le `GLOB_RECURSE` de `src/*.cpp`**
   - basculer vers `target_sources(...)` ou des `CMakeLists.txt` par sous-dossier ;
   - objectif : rendre les inclusions de modules explicites.

4. **Rendre le build plus strict**
   - ajouter un profil warnings eleves ;
   - preparer un profil de verification type `clang-tidy` / `clang-format` / sanitizers si l'environnement le permet.

5. **Clarifier les dossiers vides ou ambigus**
   - `assets/ui/css/` et `assets/ui/js/` sont aujourd'hui vides ;
   - soit les supprimer, soit les documenter comme reserve technique future.

### Fichiers / zones concernees

- `CMakeLists.txt`
- `.gitignore`
- racine du depot
- dossiers runtime et build

### Critere de sortie

- le depot ne contient plus de bruit runtime non justifie ;
- la build n'embarque plus automatiquement tous les `.cpp` par glob ;
- un nouveau fichier source doit etre ajoute explicitement au build.

---

## Phase 2 — Sortir `Game` du role de mega-orchestrateur

### Objectif

Transformer `Game` en coordinatrice legere plutot qu'en centre de gravite de tout le projet.

### Constats

`Game.hpp` / `Game.cpp` cumulent actuellement :

- boucle principale ;
- gestion des fenetres ;
- input ;
- rendu ;
- UI ;
- cycle de vie des sessions ;
- save/load ;
- LAN ;
- thread IA ;
- overlays et etats caches.

### Refactoring recommande

Extraire progressivement les responsabilites suivantes :

1. **`AITurnRunner`**
   - encapsule le thread / job async IA ;
   - possede l'etat `AsyncAITaskState` ;
   - fournit `start()`, `poll()`, `cancel()`, `isRunning()`.

2. **`SessionFlow`**
   - gere `startNewGame`, `loadGame`, `saveGame`, `returnToMainMenu`, `quitToMenu`.

3. **`MultiplayerRuntime`**
   - encapsule host/client, reconnexion, soumission distante, propagation de snapshots, presentation des etats reseau.

4. **`FrontendCoordinator`**
   - gere la construction de `InputContext`, les permissions, la mise a jour UI, la visibilite des panneaux, les overlays et l'etat de selection.

5. **`GameApp` ou `GameRuntime`**
   - nouveau role cible pour l'actuel `Game` ;
   - ne garde que : init, boucle, tick, orchestration haut niveau.

### Convention de reorganisation recommandee

- deplacer l'orchestration applicative hors de `Core` a terme ;
- par exemple vers `src/App/` ou `src/Runtime/`.

### Criteres de sortie

- `Game.hpp` devient un fichier nettement plus court ;
- `Game.cpp` ne contient plus la logique detaillee de l'IA async ni la logique LAN complete ;
- le flux detaille `start/load/save` n'est plus entierement code dans `Game.cpp` ;
- les regles frontend pures de permissions/perspective/contexte input, ainsi qu'une partie substantielle du dashboard et du choix de panneau lateral, ne sont plus entierement codees dans `Game.cpp` ;
- chaque sous-runtime extrait peut etre lu et teste sans parcourir tout `Game.cpp`.

---

## Phase 3 — Unifier le pipeline de validation et de projection des tours

### Objectif

Faire en sorte qu'une commande de tour soit validee, projetee et committe via une chaine unique.

### Probleme actuel

La logique est partagee entre plusieurs endroits :

- `TurnSystem`
- `PendingTurnProjection`
- `CheckResponseRules`
- logique de previsualisation et de gating dans le runtime

### Refactoring recommande

Introduire explicitement deux briques :

1. **`TurnCommandValidator`**
   - verifie si une commande est legalement ajoutable au tour courant ;
   - retourne un resultat structure (`ok`, `erreur`, `raison`, `etat derive`).

2. **`CommandProjector`**
   - applique une sequence de commandes sur un etat de travail ;
   - produit un etat projete reutilisable par l'UI, l'IA et les validations suivantes.

### Cible de repartition des roles

- `TurnSystem` : orchestre et stocke le tour ;
- `TurnCommandValidator` : valide ;
- `CommandProjector` : simule ;
- `CheckResponseRules` : specialise les contraintes liees au roi et au check, mais n'est plus un point d'entree concurrent.

### Taches concretes

- remplacer les signatures tres longues par un `TurnValidationContext` ;
- faire consommer ce contexte par la validation et la projection ;
- faire deleguer `PendingTurnProjection` vers `CommandProjector` ;
- supprimer les branches de validation dupliquees.

### Criteres de sortie

- ajouter une commande dans le tour local, dans une simulation IA ou via le reseau passe par les memes regles ;
- une modification de regle ne doit se faire qu'a un seul endroit.

---

## Phase 4 — Re-isoler le moteur des details frontend

### Objectif

Faire respecter une vraie frontiere entre logique coeur et interface.

### Chantiers prioritaires

1. **Supprimer la dependance `Core -> Input/ToolState`**
   - extraire le type d'outil vers une couche neutre si c'est reellement un concept shared ;
   - sinon redescendre `InputSelectionBookmark` hors de `Core`.

2. **Limiter les types SFML au frontend et au runtime applicatif**
   - a terme, le moteur ne devrait plus exposer massivement `sf::Vector2i` dans ses contrats internes ;
   - reutiliser `Vec2i` partout ou cela a du sens.

3. **Isoler le hack `LiveResizeRenderWindow`**
   - le bricolage `#define private protected` doit etre cantonne a une couche tres peripherique, voire remplace ;
   - ne jamais laisser cette technique contaminer les couches coeur.

### Criteres de sortie

- les fichiers `Core` critiques ne connaissent plus `UIManager`, `InputHandler`, `TGUI` ni `ToolState` ;
- les types frontend ne remontent plus dans les structures coeur sauf exceptions justifiees et documentees.

---

## Phase 5 — Stabiliser les references d'entites et la couche de requetes

### Objectif

Supprimer progressivement les references fragiles et clarifier la maniere de retrouver pieces, batiments et etats affiches.

### Probleme actuel

- `InputHandler` garde des `Piece*` et `Building*` ;
- la selection, les previews et les projections peuvent rendre ces pointeurs delicats ;
- plusieurs modules re-iterent les memes donnees pour retrouver une entite.

### Strategie recommande

#### Etape 5A — court terme

- remplacer le stockage de selection longue duree par des identifiants ;
- introduire un `SelectionState` stable ;
- centraliser `findPieceById` / `findBuildingById` dans un service ou adaptateur unique.

#### Etape 5B — moyen terme

- introduire une couche `GameQueries` / `BoardQueries` / `KingdomQueries` ;
- encapsuler les requetes les plus frequentes :
  - piece par id ;
  - batiment par id ;
  - piece a une case ;
  - traversabilite ;
  - occupation ;
  - cellule defendue / menacee.

#### Etape 5C — long terme, si necessaire

- etudier le passage de certaines references de `Board::Cell` vers des IDs plutot que des pointeurs directs ;
- a ne faire que si le cout est justifie, car c'est un chantier plus invasif.

### Critere de sortie

- l'input et l'UI ne dependent plus de pointeurs d'entites stockes sur plusieurs frames ;
- les recherches par identifiant sont centralisees et coherentes.

---

## Phase 6 — Rationaliser l'IA autour d'un seul chemin canonique

### Objectif

Arreter la coexistence permanente entre ancienne et nouvelle IA, ou la rendre explicitement transitoire.

### Probleme actuel

- `AIController` et `AIDirector` coexistent ;
- le flag `useNewAI` maintient deux pipelines vivants ;
- cela double les risques de divergence.

### Decision recommandee

Choisir une seule des deux strategies suivantes :

1. **Option recommandee : AIDirector devient le chemin officiel**
   - l'ancienne IA passe en `src/AI/Legacy/` pendant une courte periode ;
   - puis suppression planifiee apres couverture de tests suffisante.

2. **Option tolerable uniquement a court terme : dual stack assume**
   - les deux IA restent, mais via une interface commune propre (`IAgent`, `IAgentPlanner`) ;
   - elles sont clairement isolees, documentees et testees separement.

### Actions concretes

- centraliser les constantes de valeur de pieces, revenu, heuristiques de scoring ;
- re-utiliser `EconomySystem`, `CheckResponseRules`, `ProductionSpawnRules` et autres services dans l'IA au lieu de dupliquer des calculs ;
- unifier les points d'entree de simulation autour de snapshots et d'adaptateurs clairs ;
- verifier s'il faut fusionner ou clarifier `CheckmateSolver`, `ForwardModel::isCheckmate` et les autres formes de detection.

### Criteres de sortie

- un seul pipeline IA par defaut ;
- plus de flag transitoire `useNewAI` en production ;
- les regles critiques utilisees par l'IA proviennent des memes modules que le gameplay runtime.

---

## Phase 7 — Rendre le frontend plus propre : Input, UI, Render

### Objectif

Transformer le frontend en couche de presentation lisible et moins couplee au moteur.

### Chantiers UI

1. **Faire consommer des view models aux panels**
   - `UIManager` et les panels ne devraient plus recevoir directement `Piece`, `Building`, `Kingdom`, `Cell` quand cela peut etre prepare en amont ;
   - etendre le principe deja amorce avec `InGameViewModel`.

2. **Mutualiser le code repetitif des panels**
   - plusieurs panels partagent le meme squelette de creation, style, positionnement et masquage ;
   - introduire une base `SidebarPanelBase`, un builder de widgets ou une petite couche utilitaire commune.

3. **Clarifier la responsabilite de `UIManager`**
   - `UIManager` doit coordonner les vues, pas recalculer la logique metier.

### Chantiers Input

1. **Decoupler `InputHandler` des entites metier brutes**
   - l'input devrait manipuler un `InputContext` riche et un `SelectionState`, pas des pointeurs durables ;
   - il doit se comporter comme une couche d'interpretation, pas comme un mini runtime parallele.

2. **Isoler la logique de selection multicouche**
   - sortir la resolution de selection si elle grossit encore ;
   - conserver une couche claire pour `preview -> selection -> commande`.

### Chantiers Render

1. **Separer davantage preparation des overlays et dessin effectif**
   - la construction des donnees d'overlay doit vivre avant le dessin ;
   - `OverlayRenderer` doit tendre vers un role purement graphique.

2. **Stabiliser les modeles d'affichage**
   - a terme, `Renderer` devrait consommer des DTO d'affichage plutot que relire directement tout le domaine.

### Criteres de sortie

- l'UI ne porte plus de logique de gameplay ;
- les panels sont plus uniformes et plus faciles a maintenir ;
- l'input devient plus simple a raisonner et moins fragile vis-a-vis des commits/projections.

---

## Phase 8 — Durcir save, protocoles et tests de non-regression

### Objectif

Rendre les couches de persistance et de multiplayer plus robustes et plus predictibles pendant les refontes.

### Actions

1. **Versionner explicitement les formats**
   - saves ;
   - protocoles reseau ;
   - snapshots si necessaire.

2. **Extraire un serializer partage si la duplication existe**
   - eviter que `SaveManager` et `Protocol` evoluent de facon divergente.

3. **Distinguer les donnees de compatibilite**
   - isoler clairement les chemins legacy encore necessaires ;
   - documenter les conversions et les fallback de lecture.

4. **Augmenter les tests de non-regression**
   - saves anciennes ;
   - soumission de tour en LAN ;
   - rebuild du draft local ;
   - validation de coup quand le roi est en echec.

### Critere de sortie

- toute evolution de schema est explicite ;
- les chemins de compatibilite sont identifies ;
- le refactoring ne casse ni les saves ni le LAN sans test qui le signale.

---

## 6. Chantier detaille par zone

## A. `Core`

### Cible

`Core` doit contenir les types et mecanismes centraux independants du frontend.

### A corriger

- `Game.*` est trop applicatif pour rester durablement dans `Core` ;
- `InteractionPermissions.hpp` ne devrait pas connaitre `ToolState` ;
- `LiveResizeRenderWindow.hpp` est un detail de plateforme, pas un concept coeur.

### Plan

- conserver dans `Core` : `GameEngine`, `GameClock`, `GameSessionConfig`, `GameStateValidator`, `TurnDraft`, et autres contrats metier/runtime neutres ;
- sortir a terme les elements applicatifs vers `App` ou `Runtime`.

## B. `Systems`

### Cible

`Systems` doit devenir la couche canonique des regles.

### A corriger

- chevauchements entre `TurnSystem`, `PendingTurnProjection` et certaines `Rules` ;
- signatures longues et contexte passe en morceaux ;
- nomenclature melangeant calcul, validation, UI helper et regles metier.

### Plan

- introduire un `TurnValidationContext` ;
- introduire `TurnCommandValidator` et `CommandProjector` ;
- reclasser les fichiers par sous-domaines si le dossier continue de grossir.

## C. `AI`

### Cible

Une IA principale, claire, modulaire, qui s'appuie sur les regles partagees.

### A corriger

- coexistence trop longue des deux IA ;
- heuristiques et constantes dupliquees ;
- risque de divergence entre simulation et runtime reel.

### Plan

- isoler le legacy ;
- concentrer la simulation autour des snapshots ;
- centraliser les valeurs et les regles consultees par l'IA.

## D. `Input`

### Cible

`Input` interprete les actions utilisateur et produit un etat de selection / des intentions, sans retenir des references fragiles.

### A corriger

- stockage de pointeurs ;
- logique de selection multicouche dense ;
- couplage direct a plusieurs types du domaine.

### Plan

- selection basee sur IDs ;
- resolution de selection isolee ;
- clarifier la notion d'entite selectionnee, cellule selectionnee, preview et commande en attente.

## E. `UI`

### Cible

Des panneaux purement presentationnels, nourris par des view models.

### A corriger

- repetition entre panels ;
- `UIManager` encore trop pres des objets metier ;
- responsabilites d'affichage et de construction de donnees pas toujours distinctes.

### Plan

- etendre les view models ;
- factoriser le squelette des panels ;
- limiter la logique calculee dans `UIManager`.

## F. `Render`

### Cible

Le rendu dessine un etat d'affichage deja prepare.

### A corriger

- couplage encore fort au domaine ;
- certaines preparations d'overlays pourraient etre separees du dessin effectif.

### Plan

- consolider les structures de donnees d'overlay ;
- tendre vers des render models explicites.

## G. `Save` et `Multiplayer`

### Cible

Deux couches d'infrastructure robustes, versionnees et independantes du frontend.

### A corriger

- verifier la duplication de serialisation ;
- clarifier le statut des compat legacy ;
- augmenter les tests contractuels.

### Plan

- serializer partage si pertinent ;
- versioning explicite ;
- tests de round-trip et de compatibilite.

---

## 7. Nettoyage du code mort et des reliquats

Ce nettoyage doit etre fait avec prudence : certains morceaux sont peut-etre legacy mais encore utiles en compatibilite.

### Candidats a verifier / isoler

1. **Ancienne IA**
   - `AIController`, `AIBrain`, `AITacticalEngine`, certains `AIStrategy*` si `AIDirector` est devenu le chemin standard.

2. **Overloads legacy dans `CheckSystem`**
   - garder seulement ce qui est encore appele ;
   - supprimer ou deplacer les API board-scanning si elles ne servent plus.

3. **Comparateurs / helpers devenus obsoletes**
   - ex. `Vec2iCompare` si `Vec2i` ou un type equivalent couvre deja le besoin.

4. **Dossiers frontend reserves mais vides**
   - `assets/ui/css/`
   - `assets/ui/js/`

5. **Artefacts runtime dans le depot**
   - dumps de debug ;
   - saves temporaires ;
   - contenus de build.

### Regle de nettoyage

- si un reliquat est encore necessaire : l'isoler et le documenter ;
- s'il n'est plus utile : le supprimer ;
- s'il est transitoire : le marquer explicitement comme tel avec une date ou une phase de suppression.

---

## 8. Organisation cible des dossiers

Deux options raisonnables existent.

## Option A — refactoring pragmatique a faible friction

Conserver les dossiers actuels, mais introduire des sous-groupes et clarifier les roles.

```text
src/
  Core/
  Board/
  Units/
  Buildings/
  Kingdom/
  Systems/
    Turn/
    Economy/
    Combat/
    Check/
    Build/
    Production/
  AI/
    Director/
    Simulation/
    Evaluation/
    Legacy/
  Input/
  UI/
  Render/
  Save/
  Multiplayer/
  App/          # nouveau, pour l'orchestration runtime
```

## Option B — cible plus ambitieuse

Creer une distinction nette entre `App`, `Domain`, `Systems`, `AI`, `Frontend`, `Infrastructure`.

```text
src/
  App/
  Core/
  Domain/
  Systems/
  AI/
  Frontend/
    Input/
    UI/
    Render/
  Infrastructure/
    Save/
    Multiplayer/
    Assets/
```

### Recommendation

Commencer par **Option A**, nettement plus realiste sans casser l'historique ni faire exploser les includes.

---

## 9. Nommage et conventions a harmoniser

### Renommages recommandes

| Nom actuel | Probleme | Nom cible propose |
|---|---|---|
| `Game` dans `Core/` | trop applicatif pour s'appeler coeur | `GameApp`, `GameRuntime` ou deplacement vers `App/` |
| `BuildOverlayRules` | ce n'est pas une regle de gameplay | `BuildOverlayModel`, `BuildPreviewOverlay` ou `BuildOverlayView` |
| `TurnPointRules` | parle de budget, pas de points abstraits | `TurnBudgetRules` |
| `StructureIntegrityRules` | nom un peu flou | `BuildingIntegrityRules` |
| `AITacticalEngine` | nom trompeur si c'est surtout un scorer heuristique | `AIMoveScorer` ou `AITacticalScorer` |
| `InputSelectionBookmark` dans `Core` | concept tres lie au frontend | deplacement vers `Input` ou `App` |

### Convention recommandee

- **`System`** : service metier canonique ou orchestrateur de regles ;
- **`Rules`** : ensemble stateless de regles pures ;
- **`Coordinator` / `Runtime` / `Manager`** : orchestration applicative ;
- **`ViewModel` / `Presentation` / `DisplayModel`** : donnees preparees pour UI/rendu.

---

## 10. Strategie de test et de validation

Le chantier de refactoring doit s'appuyer sur une vraie strategie de verification.

### Refactoring du dossier `tests/`

Situation cible :

- `tests/TestMain.cpp` devient presque vide et ne sert qu'a l'entree des tests ;
- les cas sont repartis par domaine.

### Structure cible recommandee

```text
tests/
  TestMain.cpp
  Core/
  Systems/
  AI/
  Input/
  Save/
  Multiplayer/
```

### Batteries prioritaires a ajouter

1. **Turn validation**
   - ajout, annulation, remplacement, budget, check, ordre des commandes.

2. **Projection locale**
   - coherence entre `TurnDraft`, projection et commit.

3. **Mouvement et check**
   - mouvements pseudo-legaux ;
   - reponses au check ;
   - checkmate ;
   - attaques sur cases roi.

4. **Economie / production / construction**
   - revenus ;
   - upkeep ;
   - occupation des batiments publics ;
   - spawn rules ;
   - placement de structures.

5. **IA**
   - generation de plan ;
   - fin de partie ;
   - non-regression sur des positions connues.

6. **Save / load / protocoles**
   - round-trip ;
   - compatibilite ancienne version ;
   - deserialisation robuste.

### Validation continue recommandee

- build debug ;
- tests unitaires ;
- smoke tests manuels ;
- optionnel a moyen terme : CI avec build + tests sur au moins une config de reference.

---

## 11. Metriques cibles de fin de chantier

Ces metriques servent de garde-fou. Elles ne sont pas absolues, mais donnent une direction claire.

| Domaine | Cible |
|---|---|
| `Game.cpp` | reduit fortement, idealement sous ~800 lignes |
| `Game.hpp` | interface plus compacte, idealement sous ~250 lignes |
| pipeline de validation | une seule voie canonique |
| IA | un seul chemin officiel en production |
| `tests/TestMain.cpp` | fichier minimal, sans accumulation de tous les cas |
| build CMake | plus de `GLOB_RECURSE src/*.cpp` |
| repo hygiene | aucun build artefact ni dump runtime commits par defaut |
| UI/Input | plus de pointeurs d'entites stockes durablement pour la selection |
| `Core` | plus de dependance directe a `Input`, `UI`, `Render` |

---

## 12. Ordre d'execution recommande

Ordre recommande pour limiter les risques.

1. **Phase 0** — garde-fous et scenarios de regression.
2. **Phase 1** — hygiene du depot et du build.
3. **Phase 2** — extraction du runtime hors de `Game`.
4. **Phase 3** — unification validation / projection des tours.
5. **Phase 4** — isolation de `Core` vis-a-vis du frontend.
6. **Phase 5** — references stables, queries et selection.
7. **Phase 6** — rationalisation definitive de l'IA.
8. **Phase 7** — cleanup frontend UI/Input/Render.
9. **Phase 8** — solidification save/protocoles/tests.

### Pourquoi cet ordre

- on nettoie d'abord l'environnement et les garde-fous ;
- on reduit ensuite le couplage le plus dangereux (`Game`) ;
- on unifie les regles avant d'attaquer les optimisations ou le polish ;
- on ne touche a l'IA en profondeur qu'une fois les interfaces plus stables.

---

## Resume executif

Le projet repose sur une base serieuse, mais la dette principale n'est plus algorithmique : elle est **structurelle**.

Les priorites les plus rentables sont :

1. **sortir les responsabilites de `Game`** ;
2. **unifier la validation/projection des commandes** ;
3. **supprimer le double chemin IA** ;
4. **re-isoler `Core` du frontend** ;
5. **remplacer les selections basees sur pointeurs par des references stables** ;
6. **nettoyer le depot et mieux structurer les tests**.

Si ces 6 objectifs sont atteints, la codebase gagnera fortement en lisibilite, en robustesse et en vitesse d'evolution, sans necessiter une re-ecriture complete du jeu.