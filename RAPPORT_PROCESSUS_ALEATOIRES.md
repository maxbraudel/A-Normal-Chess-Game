# Rapport d'analyse des processus aleatoires du jeu

## 1. Objet du rapport

Ce rapport recense, classe et explique tous les processus aleatoires et pseudo-aleatoires reperes dans la codebase du jeu.

Perimetre analyse :

- code runtime C++ sous src/
- tests sous tests/ pour verifier le comportement attendu
- configurations sous assets/config/
- documents Markdown du depot pour distinguer ce qui est implemente de ce qui n'est encore qu'une intention

Perimetre explicitement ecarte du coeur du rapport :

- build/ et build-msvc/, qui sont des artefacts de compilation
- les generateurs d'assets HTML/JS du dossier generator/ : ils ont ete verifies, mais aucun usage effectif d'un generateur aleatoire n'y a ete trouve pour le runtime du jeu

Conclusion generale en une phrase : le jeu utilise peu de lois aleatoires usuelles, et repose surtout sur une combinaison de loi uniforme discrete, de permutation aleatoire uniforme, et de bruit procedural seed-driven pour la generation du monde.

## 2. Resume executif

Familles effectivement implementees dans le runtime :

1. Loi uniforme discrete sur ensemble fini
2. Loi uniforme discrete tronquee ou conditionnelle sur un sous-ensemble de candidats
3. Permutation aleatoire uniforme
4. Processus pseudo-aleatoire spatial continu discretise : value noise puis bruit fractal de type fBm
5. Sortie brute de PRNG assimilable a une uniforme discrete 64 bits pour le sel multijoueur

Familles non implementees dans le runtime actuel :

- Bernoulli
- Binomiale
- Geometrique
- Hypergeometrique
- Poisson
- Normale / Gaussienne
- Exponentielle
- Beta
- Gamma / Erlang
- Khi-deux
- Cauchy
- Weibull
- log-normale
- discrete_distribution, piecewise distributions, uniform_real_distribution

Point important :

- la generation de monde est reproductible a seed fixe
- les tirs aleatoires du MCTS ne sont pas relies au worldSeed et peuvent donc faire varier les decisions de l'IA entre deux executions
- le coeur des regles de jeu (combat, economie, mouvement, production, validation d'etat) reste deterministe une fois la carte et les decisions IA fixees

## 3. Inventaire synthetique

| Famille | Nature | Localisation principale | Feature servie | Reproductible a seed fixe ? |
| --- | --- | --- | --- | --- |
| Uniforme discrete finie | Loi usuelle | src/Core/GameEngine.cpp | generation du worldSeed | non, sauf si worldSeed fourni |
| Uniforme discrete finie | Loi usuelle | src/AI/AIMCTS.cpp | exploration MCTS et rollouts | non |
| Uniforme discrete finie | Loi usuelle | src/Board/BoardGenerator.cpp | rotations, flips, spawn cells, premiers choix de placement | oui |
| Uniforme discrete tronquee | Variante maison | src/Board/BoardGenerator.cpp | dispersion des mines et fermes | oui |
| Permutation uniforme | Loi usuelle | src/Board/BoardGenerator.cpp | ordre de pose des ressources publiques | oui |
| Value noise + fBm | Processus stochastique spatial pseudo-aleatoire | src/Board/BoardGenerator.cpp | generation du terrain | oui |
| Sortie brute de mt19937_64 | Assimilee a une uniforme discrete 64 bits | src/Multiplayer/PasswordUtils.hpp | generation du sel de mot de passe | non |
| Hash pseudo-aleatoire deterministe | Pas une loi usuelle | src/Board/BoardGenerator.cpp, src/Core/GameEngine.cpp | flips de terrain et compatibilite legacy | oui |
| Pseudo-randomness AI nommee mais non active | pas un alea effectif dans le runtime actuel | src/AI/AIStrategy.cpp, src/Config/AIConfig.cpp | variation strategique top-2 envisagee | non applicable |

## 4. Loi uniforme discrete sur ensemble fini

### 4.1 Definition mathematique

On parle ici d'une variable aleatoire X uniforme discrete sur un ensemble fini S lorsque :

- domaine : S = {x1, ..., xn}
- probabilite : P(X = xi) = 1 / n pour tout element xi de S

Dans cette codebase, cette famille apparait sous deux formes :

- via std::uniform_int_distribution
- via sortie brute de generateur pseudo-aleatoire lorsque le code assimile chaque entier d'un intervalle binaire a un resultat equiprobable

### 4.2 Generation du worldSeed

Localisation : src/Core/GameEngine.cpp, fonction makeRandomWorldSeed.

Definition dans le code :

- source d'entropie : std::random_device
- PRNG : std::mt19937
- distribution : uniforme discrete sur l'intervalle entier ferme {1, ..., 2147483647}

Parametres :

- borne basse : 1
- borne haute : 0x7fffffff = 2147483647

Domaine de definition :

- ensemble des entiers strictement positifs sur 31 bits

Modification par rapport a une uniforme entiere "naturelle" :

- la valeur 0 est explicitement exclue, car elle sert de sentinelle interne pour "seed absente"
- on n'echantillonne pas tout l'espace 32 bits non signe, seulement les 31 bits positifs

Feature servie :

- produire une seed de monde pour une nouvelle partie quand l'utilisateur n'en fournit pas
- declencher ensuite toute la generation procedurale seed-driven

Effet sur le jeu :

- deux nouvelles parties sans seed explicite peuvent donner des mondes differents
- une fois la seed fixee, la generation du monde devient deterministe

### 4.3 Selection aleatoire d'un enfant lors de l'expansion MCTS

Localisation : src/AI/AIMCTS.cpp, fonction expansion.

Definition dans le code :

- PRNG : std::mt19937 thread_local seedee une fois par thread via std::random_device
- distribution : uniforme discrete sur l'ensemble des indices des enfants crees dans le noeud courant

Parametres :

- domaine : {0, ..., m - 1} ou m est le nombre d'enfants retenus
- m est borne par MAX_CHILDREN_PER_NODE = 30

Modification importante :

- le tirage n'est pas fait sur l'ensemble total des actions legales
- les actions sont d'abord filtrees par pertinence, avec seuil RELEVANCE_THRESHOLD = 5.0
- puis tronquees aux 30 meilleures au maximum
- la loi effectivement appliquee est donc uniforme seulement sur le sous-ensemble pre-filtre

Feature servie :

- introduire de la diversification dans l'exploration de l'arbre MCTS
- eviter qu'un ordre fixe des actions candidates impose toujours le meme premier enfant explore

### 4.4 Tirage de piece pendant les rollouts MCTS

Localisation : src/AI/AIMCTS.cpp, fonction selectRolloutAction.

Definition dans le code :

- distribution : uniforme discrete sur l'ensemble des pieces du royaume actif
- domaine : {0, ..., n - 1} ou n est le nombre de pieces du royaume

Modification importante :

- le code n'effectue pas un tirage uniforme sur l'ensemble des coups legaux globaux
- il effectue un tirage uniforme sur les pieces, puis un tirage uniforme sur les coups de la piece choisie
- il y a au maximum 5 tentatives
- si, pendant 5 tentatives, seules des pieces sans coup legal sont tirees, le rollout retourne END_TURN

Loi effective :

- uniforme sur les pieces tirees a chaque tentative
- puis uniforme conditionnelle sur les coups de la piece retenue
- ce n'est donc pas une uniforme sur l'ensemble de tous les coups legaux du plateau

Feature servie :

- produire des rollouts rapides et peu couteux pour le MCTS
- conserver une part de variete sans calcul exhaustif d'une politique de simulation complexe

### 4.5 Tirage de coup pendant les rollouts MCTS

Localisation : src/AI/AIMCTS.cpp, fonction selectRolloutAction.

Definition dans le code :

- apres selection d'une piece ayant au moins un coup legal, le coup est tire uniformement parmi les coups legaux de cette piece

Parametres :

- domaine : {0, ..., k - 1} ou k est le nombre de coups legaux de la piece retenue

Modification importante :

- comme le tirage est conditionne par la piece choisie, les pieces ayant peu de coups donnent a chacun de leurs coups une probabilite individuelle plus forte qu'une piece ayant beaucoup de coups

Feature servie :

- finaliser la politique de simulation du rollout MCTS

### 4.6 Rotations aleatoires des batiments publics ressources

Localisation : src/Board/BoardGenerator.cpp, fonctions buildPublicResourcePlacementRequests et generate.

Definition dans le code :

- distribution : uniforme discrete sur {0, 1, 2, 3}
- interpretation : 0, 1, 2, 3 quarts de tour, soit 0 degre, 90 degres, 180 degres, 270 degres

Parametres :

- borne basse : 0
- borne haute : 3

Modification et exceptions :

- cette alea ne concerne que les mines et les fermes publiques generees
- l'eglise centrale est explicitement fixee a rotation 0 et flipMask 0

Feature servie :

- varier l'apparence et l'orientation des ressources publiques
- casser la regularite visuelle sans changer les regles fondamentales

### 4.7 Flip masks aleatoires des batiments publics ressources

Localisation : src/Board/BoardGenerator.cpp, fonctions buildPublicResourcePlacementRequests et generate.

Definition dans le code :

- distribution : uniforme discrete sur {0, 1, 2, 3}
- interpretation des 2 bits : pas de flip, flip horizontal, flip vertical, flip horizontal + vertical

Parametres :

- borne basse : 0
- borne haute : 3

Feature servie :

- diversification visuelle des ressources publiques

### 4.8 Selection uniforme d'une case de spawn

Localisation : src/Board/BoardGenerator.cpp, fonction findSpawnCell.

Definition dans le code :

- les cases candidates sont d'abord construites en filtrant : cellule dans le cercle, non eau, sans batiment, et dans la zone autorisee du camp
- ensuite le spawn est tire uniformement parmi ces cases candidates

Parametres :

- joueur : x dans la zone gauche autorisee
- IA : x dans la zone droite autorisee
- les pourcentages de zone sont clamps entre 10% et 45% dans generate
- dans la configuration fournie, player_spawn_zone_percent = 25 et ai_spawn_zone_percent = 25

Modification importante :

- la loi n'est pas uniforme sur tout le plateau
- elle est uniforme sur un sous-ensemble admissible, lui-meme dependant de la carte generee, de l'eau et des batiments publics deja poses

Feature servie :

- varier le point de depart des royaumes tout en respectant des contraintes d'equite et de jouabilite

## 5. Loi uniforme discrete tronquee ou conditionnelle sur un top-k

### 5.1 Definition mathematique

Il ne s'agit pas d'une loi usuelle standard de la bibliotheque C++, mais d'une variante maison :

- on calcule un score pour chaque candidat
- on trie les candidats par score decroissant
- on ne conserve qu'un sous-ensemble top-k
- on tire ensuite uniformement dans ce top-k

Autrement dit, c'est une uniforme discrete conditionnelle a l'evenement "le candidat appartient au meilleur sous-ensemble autorise".

### 5.2 Placement disperse des mines et fermes

Localisation : src/Board/BoardGenerator.cpp, fonction selectDispersedCandidate.

Definition dans le code :

- si aucun batiment n'existe encore, tirage uniforme sur tous les candidats
- sinon, on score chaque candidat selon son eloignement des batiments existants
- puis on tire uniformement dans le meilleur sous-ensemble

Score utilise :

- 3.5 x distance au batiment le plus proche, quel que soit son type
- 2.0 x distance au batiment le plus proche du meme type
- 0.35 x distance moyenne a tous les batiments deja poses

Parametres du sous-ensemble top-k :

- si N est le nombre total de candidats, alors k = min(N, max(3, ceil(N / 6)))

Domaine de definition :

- ensemble des k meilleurs candidats selon le score de dispersion

Modification importante :

- ce n'est pas une alea purement uniforme dans l'espace de placement
- le tirage est volontairement biaise vers les meilleures positions de dispersion
- cette modification evite les clusters trop compacts tout en gardant de la variete
- si aucun candidat espace n'existe, le code retombe sur un ensemble relache
- si aucun candidat relache n'existe non plus, le placement final devient deterministe via un fallback central

Feature servie :

- mieux repartir mines et fermes sur la carte
- favoriser une proximite raisonnable entre types differents plutot que de regrouper systematiquement toutes les mines ensemble et toutes les fermes ensemble

## 6. Permutation aleatoire uniforme

### 6.1 Definition mathematique

Pour une liste de taille n, une permutation aleatoire uniforme donne a chaque permutation possible la meme probabilite 1 / n!.

### 6.2 Ordre de pose des ressources publiques

Localisation : src/Board/BoardGenerator.cpp, fonction buildPublicResourcePlacementRequests.

Definition dans le code :

- le generateur construit d'abord la liste des demandes de placement pour les mines et les fermes
- puis la liste est melangee par std::shuffle avec le PRNG seed-driven du monde

Parametres :

- taille de la liste = num_mines + num_farms
- dans la configuration fournie : 2 mines + 3 fermes, donc 5 requetes de placement

Modification importante :

- certaines requetes sont de meme type, donc plusieurs permutations differentes de la liste peuvent produire un ordre visuellement equivalent
- du point de vue algorithmique, le melange reste un tirage uniforme sur les permutations de la liste interne

Feature servie :

- eviter un biais fixe de pose du style "toujours poser toutes les mines puis toutes les fermes"
- rendre la carte moins previsible a seed differente

## 7. Processus pseudo-aleatoires spatiaux continus discretises : value noise et bruit fractal

Cette partie est le coeur de la generation du terrain. Ce n'est pas une loi de probabilite usuelle du type Poisson ou Gauss. C'est un processus pseudo-aleatoire spatial, continu dans sa definition d'echantillonnage, puis discretise au niveau des cellules du plateau.

### 7.1 Source elementaire : hash pseudo-uniforme sur la grille

Localisation : src/Board/BoardGenerator.cpp, fonctions mixSeed et hashValue.

Definition dans le code :

- pour chaque coin de grille entier (x, y), le code calcule un hash deterministe a partir de la seed et de la position
- il conserve ensuite 24 bits de ce hash et les normalise dans l'intervalle [0, 1]

Domaine de definition :

- entree : (seed, x, y) avec x et y entiers
- sortie : ensemble discret de 16777216 valeurs possibles entre 0 et 1

Nature mathematique :

- ce n'est pas une loi uniforme continue stricte
- c'est une quasi-uniforme discrete 24 bits issue d'un hash deterministe
- il n'y a pas de garantie mathematique stricte d'independance ou d'uniformite parfaite ; il s'agit d'une approximation pseudo-aleatoire adaptee au procedural

Feature servie :

- fournir la matiere premiere aleatoire des champs de terrain

### 7.2 Value noise 2D

Localisation : src/Board/BoardGenerator.cpp, fonction valueNoise.

Definition dans le code :

- on prend les 4 valeurs pseudo-aleatoires aux coins de la cellule de grille entourant le point reel (x, y)
- on interpole ces 4 valeurs avec un smoothStep, puis deux lerp successifs

Domaine de definition :

- entree : (x, y) dans R2
- sortie : valeur reelle dans [0, 1]

Modification importante par rapport a des variables aleatoires independantes :

- les echantillons voisins sont fortement correles spatialement
- la sortie est lisse, continue, et non i.i.d.
- on est donc face a un champ pseudo-aleatoire spatial, pas a une suite de tirages independants

Feature servie :

- produire des structures de terrain naturelles plutot que du "sel et poivre"

### 7.3 Bruit fractal de type fBm

Localisation : src/Board/BoardGenerator.cpp, fonction fractalNoise.

Definition dans le code :

- somme normalisee de plusieurs octaves de value noise
- a chaque octave, la frequence est multipliee par 2
- l'amplitude est divisee par 2
- les amplitudes commencent a 0.5 puis 0.25, 0.125, etc.

Forme qualitative :

- octave 0 : amplitude 0.5, frequence 1
- octave 1 : amplitude 0.25, frequence 2
- octave 2 : amplitude 0.125, frequence 4
- le tout est renormalise par la somme des amplitudes

Domaine de definition :

- entree : (x, y) dans R2, nombre d'octaves entier >= 1
- sortie : valeur reelle normalisee, pratiquement dans [0, 1]

Parametres effectifs dans la configuration courante :

- terrain_noise_scale = 14, puis clamp runtime a au moins 5
- terrain_octaves = 3, puis clamp runtime a au moins 1

Nature mathematique :

- processus spatial pseudo-aleatoire corrige par octaves
- plus proche d'un bruit fractal ou d'une fBm pratique que d'une loi classique isolee

Feature servie :

- former les grandes masses de terre et d'eau avec une texture multi-echelle

### 7.4 Derivation de deux seeds secondaires de bruit

Localisation : src/Board/BoardGenerator.cpp, fonction generate.

Definition dans le code :

- le PRNG std::mt19937 seed par worldSeed est interroge deux fois par sortie brute
- cela produit dirtNoiseSeed et waterNoiseSeed

Domaine de definition :

- deux entiers 32 bits issus de la sortie du generateur

Interpretation :

- ce ne sont pas deux nouvelles lois visibles au gameplay
- ce sont deux tirages pseudo-aleatoires internes servant a decorreler les champs de terre et d'eau

Feature servie :

- eviter que terre et eau reutilisent exactement la meme texture procedurale

### 7.5 Transformation des champs de bruit en scores de terrain

Localisation : src/Board/BoardGenerator.cpp, fonction generate.

Pour la terre :

- score = 0.72 x bruit macro + 0.28 x bruit detail - edgePenalty
- edgePenalty = max(0, radialDistance - 0.82) x 0.35

Pour l'eau :

- score = 0.67 x bruit macro + 0.33 x bruit detail + 0.08 x waterRingBias
- waterRingBias = max(0, 1 - abs(radialDistance - 0.58))

Modifications importantes :

- la loi de base n'est pas utilisee telle quelle
- elle est combinee en macro + detail avec poids differents selon le type de terrain
- si ce sous-ensemble est vide dans la zone preferee, le code degrade vers un fallback deterministe : d'abord la premiere case admissible trouvee sur le plateau, sinon le centre du board
- elle est deformee radialement : penalite vers les bords pour la terre, bonus anneau pour l'eau
- certaines colonnes proches des zones de spawn sont integralement protegees et exclues de la candidature terrain

Feature servie :

- obtenir des patches de terre et de petites etendues d'eau compatibles avec la jouabilite

### 7.6 Seuils, masques, elagage et contraintes topologiques

Localisation : src/Board/BoardGenerator.cpp, fonctions selectThreshold, pruneSparseMask, extractComponents, selectComponents, isConnected.

Definition dans le code :

- les scores sont convertis en masques booleens par seuil quantile-like
- couverture cible terre : clamp entre 0% et 40%
- couverture cible eau : clamp entre 0% et 12%
- dans la configuration fournie : 14% de terre, 4% d'eau

Puis le code applique successivement :

- suppression des cellules trop isolees
- extraction de composantes connexes en voisinage 8
- selection des meilleures composantes par score moyen
- limitation du nombre et de la taille des regions selon les rayons configures
- test de connectivite du plateau : une region d'eau candidate est annulee si elle coupe la connexion terre entre les deux camps

Parametres effectifs de la configuration courante :

- num_dirt_blobs = 6
- dirt_blob_min_radius = 2
- dirt_blob_max_radius = 5
- num_lakes = 3
- lake_min_radius = 2
- lake_max_radius = 3

Interpretation mathematique :

- la carte finale n'est pas un simple echantillonnage direct d'un champ aleatoire
- c'est un champ pseudo-aleatoire ensuite tronque, filtre, regularise morphologiquement, puis contraint topologiquement

Feature servie :

- garantir une carte esthetique, jouable et traversable

### 7.7 Reproductibilite du systeme de terrain

Les tests de tests/TestMain.cpp imposent explicitement que :

- une meme worldSeed reproduit exactement le meme terrain, les memes batiments publics et les memes spawns
- une worldSeed differente change le monde genere

Conclusion :

- le processus est aleatoire lors du choix initial de la seed, mais entierement deterministe ensuite

## 8. Sortie brute de PRNG 64 bits pour le sel multijoueur

### 8.1 Definition mathematique

Le code n'utilise pas une distribution explicite de la bibliotheque standard. Il utilise directement la sortie d'un generateur std::mt19937_64. Sous le modele habituel d'usage, cela s'interprete comme un tirage pseudo-aleatoire sur l'ensemble des entiers 64 bits.

### 8.2 Generation du sel de mot de passe

Localisation : src/Multiplayer/PasswordUtils.hpp, fonction generateSalt.

Definition dans le code :

- source d'entropie : std::random_device
- PRNG : std::mt19937_64
- deux sorties brutes de 64 bits sont concatenees
- le resultat final est une chaine hexadecimale de 32 caracteres + 32 caracteres = 128 bits representes en hexadecimal

Domaine de definition :

- couple (X1, X2) avec X1 et X2 dans {0, ..., 2^64 - 1}
- representation finale : chaine hexadecimale 128 bits

Modification importante :

- ce n'est pas une loi uniforme continue ni meme une distribution objet C++
- c'est une sortie brute de PRNG assimilee a une uniforme discrete sur 64 bits, repetee deux fois

Feature servie :

- generer un sel pour le hachage de mot de passe LAN

Note technique utile :

- le mecanisme est pseudo-aleatoire et non cryptographique au sens strict, car il repose sur mt19937_64

## 9. Pseudo-aleatoire deterministe non assimilable a une loi usuelle

Ces mecanismes affectent le rendu ou la compatibilite, mais ne sont pas des tirages aleatoires effectifs au moment de l'execution une fois la seed connue.

### 9.1 Flip du terrain par hash deterministe

Localisation : src/Board/BoardGenerator.cpp, fonction terrainFlipMaskFor.

Definition dans le code :

- on melange worldSeed, type de cellule et position (x, y)
- on extrait les 2 bits faibles du hash melange
- support obtenu : {0, 1, 2, 3}

Nature :

- aspect quasi-uniforme sur 2 bits
- mais resultat completement deterministe pour une cellule donnee

Feature servie :

- varier l'orientation visuelle du terrain sans casser la reproductibilite

### 9.2 Rotation et flips legacy des batiments publics charges depuis une sauvegarde

Localisation : src/Core/GameEngine.cpp, fonctions deriveLegacyPublicBuildingRotation et deriveLegacyPublicBuildingFlipMask.

Definition dans le code :

- derivees par hash deterministe depuis worldSeed, type de batiment et position du batiment
- rotation : extraction de 2 bits, donc support {0, 1, 2, 3}
- flipMask : extraction de 2 bits, donc support {0, 1, 2, 3}

Nature :

- pas d'alea runtime au moment du chargement si les entrees sont identiques
- usage pseudo-aleatoire uniquement pour reconstruire un rendu stable des anciennes sauvegardes

Feature servie :

- compatibilite avec les anciennes saves ne stockant pas directement rotation et flip

### 9.3 Seed legacy derivee par hash

Localisation : src/Core/GameEngine.cpp, fonction deriveLegacyWorldSeed.

Definition dans le code :

- hash FNV-1a like sur gameName, turnNumber, mapRadius et activeKingdom
- resultat masque sur 31 bits positifs, avec 0 remplace par 1

Nature :

- totalement deterministe
- ce n'est pas une variable aleatoire a l'execution ; c'est une reconstruction de seed a partir d'un etat legacy

Feature servie :

- restaurer un comportement seed-driven meme pour d'anciennes sauvegardes qui ne stockaient pas explicitement la worldSeed

## 10. Mecanisme nomme "randomness" mais non aleatoire dans le runtime actuel

### 10.1 Variation strategique top-2 de l'IA

Localisation :

- src/AI/AIStrategy.cpp, fonction computePlan
- src/Config/AIConfig.cpp
- assets/config/ai_params.json

Definition dans le code :

- si au moins deux objectifs existent et si randomness > 0
- si l'ecart de score entre top-1 et top-2 est < 10
- alors l'IA choisit top-2 uniquement quand turnNumber modulo 3 vaut 0

Pourquoi ce n'est pas une vraie variable aleatoire :

- aucun tirage aleatoire n'est effectue
- la decision depend uniquement du numero de tour
- a configuration courante, randomness vaut 0.0, donc le bloc ne s'active meme pas

Feature servie :

- intention de creer une legere variete strategique sur les egalites ou quasi-egalites

Conclusion :

- ce mecanisme ne doit pas etre classe parmi les lois aleatoires effectivement utilisees dans le jeu actuel

## 11. Lois usuelles recherchees explicitement mais absentes du code runtime

La recherche exhaustive dans src/, tests/, assets/config/ et le balayage complementaire du depot n'ont revele aucun usage runtime de :

- loi de Bernoulli
- loi binomiale
- loi geometrique
- loi hypergeometrique
- loi de Poisson
- loi normale / gaussienne
- loi exponentielle
- loi Beta
- loi Gamma / Erlang
- loi du khi-deux
- loi de Cauchy
- loi de Weibull
- loi log-normale
- loi uniforme reelle explicite via uniform_real_distribution
- loi discrete generale via discrete_distribution
- loi par morceaux via piecewise_constant_distribution ou piecewise_linear_distribution
- API C historique rand / srand

## 12. Elements aleatoires mentionnes dans les documents mais non implementes dans le jeu actuel

Les notes du depot contiennent plusieurs idees d'alea qui ne sont pas presentes dans le runtime execute au moment de cette analyse.

### 12.1 Notes de aléatoire.md

Le fichier aléatoire.md mentionne notamment :

- une piece rouge rogue qui spawnerait aleatoirement
- des regles de deplacement aleatoires pour certaines pieces ou situations de brouillard
- une probabilite de spawn decroissante selon le nombre deja present
- une probabilite de mercenaire en fonction des kills
- une probabilite de reparation spontanee d'une case de batiment cassee
- une probabilite de trahison ou de ralliement a l'ennemi
- des tresors qui spawneraient sur la carte
- un assombrissement de l'herbe par gaussienne

Statut :

- idees de design uniquement
- aucune implementation correspondante n'a ete trouvee dans le code runtime actuel

### 12.2 Notes de PLAN_IA_IMPLEMENTATION.md

Le plan d'IA evoque aussi une variation stochastique top-2 plus "vraie" avec un schema du type random() < 0.3 et un exemple de randomness = 0.15.

Statut :

- le code reel n'emploie pas ce tirage
- a la place, il utilise une regle deterministe basee sur turnNumber % 3, et encore uniquement si randomness > 0

## 13. Conclusion finale

Si l'on se limite aux processus effectivement utilises par le jeu au runtime, les lois ou familles distinctes a retenir sont :

1. la loi uniforme discrete sur ensemble fini
2. la loi uniforme discrete conditionnelle sur un top-k de candidats
3. la permutation aleatoire uniforme
4. un processus pseudo-aleatoire spatial de type value noise / bruit fractal fBm
5. la sortie brute d'un PRNG 64 bits, assimilee a une uniforme discrete 64 bits, pour le sel multijoueur

Le point le plus important du point de vue gameplay est le suivant :

- la carte, les ressources publiques, les flips visuels et les spawns sont seed-driven et donc reproductibles a worldSeed fixe
- l'IA MCTS reste partiellement non reproductible car son generateur aleatoire n'est pas derive du worldSeed
- les autres aleas evoques dans les documents de design ne sont pas encore implementes
