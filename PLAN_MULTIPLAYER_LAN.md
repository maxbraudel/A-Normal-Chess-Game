# Plan D'Implementation Multiplayer LAN

## Objectif Produit

Ajouter un mode multijoueur LAN pour les parties `Human vs Human`:

- Lors de la creation d'une save, si les deux royaumes sont controles par des humains, l'utilisateur peut activer `Multiplayer`.
- Si `Multiplayer` est active, la save stocke un port TCP et un mot de passe de serveur.
- Quand cette save est lancee par son createur, l'application demarre un serveur LAN sur `0.0.0.0:port`.
- Le createur est toujours l'hote et controle `White`.
- Le joueur qui rejoint controle toujours `Black`.
- Depuis le menu principal, un bouton `Join Multiplayer` permet de saisir `IP`, `Port`, `Password`, puis de rejoindre la partie.

## Contraintes Architecturales Constatees

Le code actuel donne de tres bons points d'ancrage:

- `GameSessionConfig` et `SaveData` portent deja les metadonnees de session.
- `GameEngine` et `TurnSystem` sont deja l'autorite logique du jeu.
- `TurnSystem` accumule deja des commandes en attente avant `commitTurn()`, ce qui est ideal pour du multijoueur tour par tour.
- `SaveManager` sait deja serialiser un snapshot complet de partie.

Le principal point a corriger avant tout code reseau est celui-ci:

- le runtime actuel assimile `humain` et `joueur local` via `humanKingdomId()`;
- en multiplayer, les deux royaumes sont humains, mais un seul est local sur chaque machine;
- il faut donc introduire une notion explicite de `siege local` ou `localPlayerKingdom`, distincte des metadonnees de session.

## Decisions Techniques Recommandees

### 1. Mode Reseau

Utiliser un modele `host authoritative server`:

- l'hote est la seule source de verite;
- le client ne commit jamais directement l'etat;
- le client soumet son tour au serveur;
- le serveur valide, applique, sauvegarde, puis diffuse le snapshot resultant.

Pourquoi:

- pas de rollback;
- pas de simulation pair-a-pair fragile;
- surface de desynchronisation minimale;
- parfaitement adapte a un jeu tour par tour.

### 2. Transport

Utiliser `standalone Asio` en TCP.

Pourquoi Asio plutot que `sfml-network`:

- la couche reseau doit rester independante du frontend legacy SFML/TGUI;
- Asio est mieux adaptee a une boucle reseau dediee, aux timeouts, aux heartbeats et aux reconnexions;
- la future migration frontend ne cassera pas la couche multiplayer.

Pourquoi TCP plutot que UDP:

- debit faible;
- messages critiques et ordonnes;
- besoin de fiabilite plutot que de latence ultra basse.

### 3. Securite Mot De Passe

Ne jamais stocker le mot de passe en clair dans la save.

Choix recommande:

- `libsodium` pour `Argon2id` ou equivalent via `crypto_pwhash` pour le stockage;
- handshake avec challenge/response base sur nonce pour eviter l'envoi trivial du mot de passe brut.

Si on veut une V1 plus simple, il faut etre transparent:

- mot de passe hash en base locale;
- verification simple a la connexion;
- securite suffisante pour un LAN de confiance, mais pas chiffrement fort du trafic.

Recommendation: pour rester coherent avec l'objectif de robustesse/surete, prendre `libsodium` des le debut.

### 4. Format D'Echange

Ne pas inventer deux modeles d'etat differents.

Recommendation:

- extraire la serialisation de snapshot hors de `SaveManager` vers un module partage de type `SessionSerializer`;
- `SaveManager` devient un wrapper fichier autour de ce serializer;
- le reseau reutilise le meme serializer pour transmettre le snapshot complet.

## Refactor Structurel Obligatoire

### Problematique

Les appels actuels a `humanKingdomId()` servent a:

- centrer la camera;
- choisir quel royaume est inspectable;
- autoriser ou non les commandes locales;
- afficher les panneaux de contexte;
- choisir le point de vue quand ce n'est pas le tour actif.

En multiplayer, cela ne fonctionne plus:

- sur le client, `Black` est local mais `humanKingdomId()` retournerait `White` aujourd'hui.

### Solution

Introduire une configuration runtime distincte de la session:

```cpp
enum class MultiplayerRole {
    None,
    Host,
    Client
};

struct LocalPlayerContext {
    MultiplayerRole role = MultiplayerRole::None;
    KingdomId localKingdom = KingdomId::White;
    bool isNetworked = false;
};
```

Puis remplacer l'usage de `humanKingdomId()` dans `Game` par des helpers runtime:

- `localKingdomId()`
- `isLocalPlayersTurn()`
- `canLocalPlayerIssueCommands()`
- `viewedKingdomId()`

Le `GameEngine` reste neutre: il sait quels royaumes sont humains ou IA, mais il ne sait pas quel humain est sur cette machine.

## Evolutions Du Modele De Donnees

### `GameSessionConfig`

Ajouter un bloc multiplayer:

```cpp
struct MultiplayerConfig {
    bool enabled = false;
    std::uint16_t port = 0;
    std::string passwordHash;
    std::string passwordSalt;
    std::uint32_t protocolVersion = 1;
};
```

Puis dans `GameSessionConfig`:

```cpp
MultiplayerConfig multiplayer;
```

### `SaveData`

Persister exactement ce bloc dans la save.

### `GameStateValidator`

Ajouter les regles suivantes:

- `multiplayer.enabled` interdit sauf si les deux controllers sont `Human`;
- `port` obligatoire si multiplayer active;
- plage UI: `1..65535`;
- le hash de mot de passe doit exister si multiplayer active;
- `White` est reserve a l'hote et `Black` au joueur qui rejoint.

## Architecture De Modules Cible

Nouveaux modules recommandes:

- `src/Multiplayer/Protocol.hpp`
- `src/Multiplayer/ProtocolMessages.hpp`
- `src/Multiplayer/SessionSerializer.hpp/.cpp`
- `src/Multiplayer/MultiplayerManager.hpp/.cpp`
- `src/Multiplayer/MultiplayerServer.hpp/.cpp`
- `src/Multiplayer/MultiplayerClient.hpp/.cpp`
- `src/Multiplayer/PasswordAuth.hpp/.cpp`
- `src/Multiplayer/MessageFramer.hpp/.cpp`

Modules a modifier:

- `src/Core/GameSessionConfig.hpp`
- `src/Save/SaveData.hpp`
- `src/Save/SaveManager.hpp/.cpp`
- `src/Core/GameStateValidator.hpp/.cpp`
- `src/Core/Game.hpp/.cpp`
- `src/UI/MainMenuUI.hpp/.cpp`
- `tests/TestMain.cpp`

## Protocole Reseau Recommande

Transport:

- TCP uniquement
- un framing longueur-prefixee
- header minimal: `magic`, `version`, `messageType`, `requestId`, `payloadSize`

Messages:

1. `ServerInfoRequest`
2. `ServerInfoResponse`
3. `JoinHello`
4. `AuthChallenge`
5. `AuthProof`
6. `JoinAccepted`
7. `JoinRejected`
8. `StateSnapshot`
9. `TurnSubmission`
10. `TurnAccepted`
11. `TurnRejected`
12. `ResyncSnapshot`
13. `Heartbeat`
14. `DisconnectNotice`

## Flow Runtime Recommande

### A. Creation De Save Multiplayer

Dans la popup de creation:

- si `White = Human` et `Black = Human`, afficher une checkbox `Multiplayer`;
- si cochee, afficher:
  - `Server Port`
  - `Server Password`
- si l'un des controllers n'est plus humain, masquer le bloc et desactiver `Multiplayer`.

Validation UI:

- port entier strict;
- plage `1..65535`;
- mot de passe non vide;
- message d'erreur clair si invalide.

### B. Lancement D'Une Save Multiplayer Par L'Hote

Quand l'utilisateur clique `Play Save` sur une save multiplayer:

1. charger la save normalement;
2. demarrer le serveur LAN sur `0.0.0.0:port`;
3. si le bind echoue, ne pas ouvrir la partie et afficher l'erreur;
4. ouvrir un overlay `Waiting for player to join...`;
5. tant que le client n'est pas authentifie et synchronise, aucun tour ne peut commencer;
6. apres handshake et chargement du snapshot cote client, lever l'overlay et lancer la partie.

Recommendation produit:

- geler la partie avant la connexion du joueur 2;
- afficher l'adresse IP LAN detectee et le port pour aider l'utilisateur.

### C. Rejoindre Depuis `Join Multiplayer`

Le nouveau bouton de menu principal ouvre une popup minimale:

- `Server IP`
- `Server Port`
- `Password`
- bouton `Join Game`

Flow:

1. tentative de connexion TCP courte;
2. `ServerInfoRequest` pour verifier que le serveur existe et parle le bon protocole;
3. handshake d'authentification;
4. reception du `StateSnapshot`;
5. restauration du snapshot dans un `GameEngine` local;
6. runtime local configure avec `MultiplayerRole::Client` et `localKingdom = Black`.

## Synchronisation De Jeu

### Decision Cle

Ne pas synchroniser les clics, les selections, ni les previews de construction au fil de l'eau.

Synchroniser uniquement:

- le snapshot initial;
- la soumission du tour du joueur distant;
- le snapshot apres commit.

### Pourquoi C'est Le Meilleur Choix Ici

Le jeu est deja concu autour de commandes en attente dans `TurnSystem`.

Le client peut donc:

- preparer ses commandes localement exactement comme aujourd'hui;
- visualiser ses previews localement;
- cliquer `End Turn`;
- envoyer l'ensemble de `pendingCommands` dans un `TurnSubmission`.

Le serveur:

- recharge ces commandes sur l'etat autoritatif;
- valide chacune via les regles deja existantes;
- commit le tour;
- avance le tour;
- sauvegarde si necessaire;
- envoie un snapshot resultant aux deux machines.

En cas d'echec:

- `TurnRejected` + message clair;
- `ResyncSnapshot` pour remettre le client exactement sur l'etat du serveur.

## Integration Dans `Game`

### Host

Sur l'hote:

- `Game` garde le `GameEngine` autoritatif;
- les entrees locales `White` fonctionnent comme aujourd'hui;
- quand c'est le tour de `Black`, l'hote est spectateur en attente du `TurnSubmission` distant.

### Client

Sur le client:

- le `GameEngine` local n'est qu'un miroir du serveur;
- commandes actives uniquement si `activeKingdom == Black`;
- `Reset Turn` efface seulement les commandes locales non soumises;
- `End Turn` envoie le batch de commandes au serveur.

### IA

Pour la V1:

- multiplayer autorise uniquement si `White` et `Black` sont humains;
- aucune IA dans une session multiplayer.

## UI A Ajouter

### `MainMenuUI`

Ajouter:

- bouton `Join Multiplayer` sur l'ecran principal;
- bloc `Multiplayer` dans le create dialog;
- popup `Join Multiplayer`.

### En Jeu

Ajouter:

- overlay d'attente cote host avant connexion;
- petit indicateur de statut reseau;
- message modal en cas de deconnexion, mauvais mot de passe, mismatch de version, port indisponible.

### Menus En Jeu

Recommandations V1:

- seul l'hote peut sauvegarder sur disque;
- cote client, `Save` est desactive ou remplace par un message d'information;
- `Pause` doit etre soit desactivee, soit geree explicitement comme pause globale de session.

## Gestion Des Erreurs Et Resilience

Cas a gerer proprement:

- port deja utilise;
- serveur introuvable;
- mauvais mot de passe;
- version/protocole incompatibles;
- snapshot corrompu;
- tentative du client de jouer hors de son tour;
- deconnexion reseau en cours de partie;
- hote qui ferme brutalement.

Comportement recommande:

- toute erreur reseau doit afficher une UI bloquante claire;
- aucun etat local ne doit continuer en aveugle apres perte de l'autorite;
- toute divergence doit se resoudre par `ResyncSnapshot`, jamais par heuristique.

## Strategie De Tests

### Tests Unitaires

Ajouter des tests pour:

- validation de `MultiplayerConfig`;
- round-trip de serialisation du bloc multiplayer;
- round-trip du protocole de messages;
- rejection d'un port invalide;
- rejection d'une save multiplayer avec controller non humain.

### Tests D'Integration

Ajouter des tests `loopback` host/client:

- handshake OK;
- mot de passe incorrect;
- restauration de snapshot cote client;
- soumission d'un tour valide;
- rejet d'un tour invalide;
- resynchronisation apres rejection;
- deconnexion du client.

### Tests Manuels

- creation d'une save multiplayer;
- lancement host;
- join depuis une autre machine du LAN;
- verification que l'hote controle `White`;
- verification que le client controle `Black`;
- verification que les deux voient la meme carte et le meme historique;
- test `Reset Turn` cote client;
- test `Save` cote host seulement.

## Ordre D'Implementation Recommande

### Phase 1. Refactor Runtime Local/Remote

- introduire `LocalPlayerContext`;
- sortir toutes les decisions `joueur local` de `GameEngine` vers `Game`;
- remplacer les usages de `humanKingdomId()` dans le runtime.

### Phase 2. Donnees Et Validation

- ajouter `MultiplayerConfig` a `GameSessionConfig` et `SaveData`;
- etendre `SaveManager`;
- etendre `GameStateValidator`;
- ajouter les tests de round-trip.

### Phase 3. UI Menu Principal

- checkbox `Multiplayer` dans le create dialog;
- champs `Port` et `Password`;
- popup `Join Multiplayer`;
- validation des saisies.

### Phase 4. Couche Reseau

- integrer Asio;
- implementer framing + protocol messages;
- implementer serveur et client;
- implementer handshake et auth.

### Phase 5. Host Authority

- lancement serveur lors du `Play Save` multiplayer;
- overlay d'attente host;
- envoi du snapshot initial;
- blocage des tours tant que le client n'est pas synchronise.

### Phase 6. Tours Distant

- serialiser `TurnCommand`;
- envoyer `TurnSubmission` depuis le client;
- validation/commit cote host;
- broadcast du snapshot resultat.

### Phase 7. Hardening

- gestion deconnexion;
- resync forcée;
- statuts UI;
- tests d'integration loopback.

## Ce Qu'Il Ne Faut Pas Faire

- ne pas utiliser du peer-to-peer;
- ne pas diffuser chaque clic ou chaque mouvement de souris;
- ne pas laisser le client modifier l'etat autoritatif;
- ne pas reutiliser `humanKingdomId()` pour representer le joueur local;
- ne pas stocker le mot de passe en clair dans la save.

## Recommendation Finale

La meilleure implementation pour cette architecture est:

- un serveur host autoritatif;
- un client miroir;
- un protocole TCP versionne;
- synchronisation par batch de tour, pas par input en temps reel;
- un bloc multiplayer dans la save;
- un contexte runtime local explicite;
- une serialisation de snapshot partagee entre disque et reseau.

Si on suit cet ordre, on obtient un multiplayer LAN robuste sans casser les fondations deja propres du moteur actuel.