# Rapport d'analyse exhaustive des processus aleatoires du runtime

## 1. Objet, perimetre et methode

Ce rapport recense l'ensemble des lois de probabilite, variables aleatoires usuelles, tirages ponderes, permutations aleatoires et processus pseudo-aleatoires effectivement utilises par le runtime actuel du jeu.

Perimetre analyse :

- le runtime C++ sous src/
- la chaine de chargement des configurations
- la serialisation des etats aleatoires dans les sauvegardes
- les tests qui verifient la reproductibilite de certains systemes

Perimetre explicitement exclu du coeur du rapport :

- build/ et build-msvc/, qui sont des artefacts de compilation
- les plans et notes de conception quand ils ne correspondent pas a du comportement runtime effectif
- les generateurs d'assets du dossier generator/, qui ne pilotent pas le jeu en execution

Point de configuration important :

- le jeu charge d'abord assets/config/master_config.json, puis retombe sur assets/config/game_params.json seulement si master_config.json est absent
- l'IA charge d'abord assets/config/master_config.json, puis retombe sur assets/config/ai_params.json en secours
- la configuration actuelle expose des parametres XP, infernal et meteo dans master_config.json
- la configuration actuelle n'expose pas de section chest dans master_config.json ni dans game_params.json ; le systeme de coffres tourne donc avec les valeurs par defaut codees dans GameConfig::setDefaults()

Point de reproductibilite important :

- les systemes XP, coffres, infernal et meteo derivent tous leurs tirages d'un couple worldSeed + rngCounter serialize, ce qui permet une reprise deterministe apres sauvegarde si l'etat est restaure fidelement
- la generation du monde est seed-driven a partir de worldSeed
- le MCTS de l'IA n'est pas derive de worldSeed et reste non reproductible entre executions
- la generation de sel multijoueur est egalement hors du schema worldSeed

## 2. Resume executif

### 2.1 Lois et familles effectivement presentes dans le runtime

Lois ou familles standard explicitement echantillonnees via l'API C++ <random> :

1. loi uniforme discrete
2. loi de Bernoulli
3. loi categorielle / loi discrete generale ponderee
4. loi de Poisson
5. loi normale, mais tronquee et discretisee
6. loi de Weibull, mais discretisee et bornee inferieurement
7. loi Gamma, mais translatee, discretisee et utilisee comme delai
8. loi log-normale, puis transformee par clamp pour l'opacite meteo
9. loi Beta, echantillonnee via deux lois Gamma puis transformee
10. loi lineaire par morceaux
11. permutation aleatoire uniforme

Processus pseudo-aleatoires importants mais non assimilables a une loi usuelle simple :

1. bruit spatial de type value noise puis fBm pour le terrain
2. champs pseudo-uniformes deterministes par hachage pour certains masques visuels et meteo
3. sorties brutes de PRNG utilisees comme seeds internes
4. tirages uniformes ou ponderes sur sous-ensembles dynamiques apres filtrage, top-k ou elimination

### 2.2 Lois non trouvees dans le runtime actuel

Les familles suivantes n'ont pas ete trouvees dans le code runtime actuel :

- loi binomiale
- loi geometrique
- loi hypergeometrique
- loi exponentielle
- loi du chi-deux
- loi de Cauchy
- loi de Student
- loi de Fisher
- loi binomiale negative
- uniform_real_distribution
- piecewise_constant_distribution

Remarque : le runtime utilise bien gamma_distribution et piecewise_linear_distribution, mais pas les variantes ci-dessus.

## 3. Configuration effective au moment de l'analyse

### 3.1 Monde et terrain

Configuration runtime actuelle lue dans master_config.json :

- rayon de carte : 25
- nombre d'octaves du terrain : 3
- couverture cible terre : 14 %
- couverture cible eau : 4 %
- nombre cible de taches de terre : 6
- nombre cible de lacs : 3
- zone de spawn joueur : 25 % de la largeur
- zone de spawn IA : 25 % de la largeur

### 3.2 XP

Configuration runtime actuelle lue dans master_config.json :

- kill_pawn : moyenne 20, sigma multiplier 0.18, clamp multiplier 2.00, minimum 1
- kill_knight : moyenne 50, sigma multiplier 0.16, clamp multiplier 2.00, minimum 1
- kill_bishop : moyenne 50, sigma multiplier 0.16, clamp multiplier 2.00, minimum 1
- kill_rook : moyenne 100, sigma multiplier 0.12, clamp multiplier 2.00, minimum 1
- kill_queen : moyenne 300, sigma multiplier 0.10, clamp multiplier 2.00, minimum 1
- destroy_block : moyenne 10, sigma multiplier 0.15, clamp multiplier 2.00, minimum 1
- arena_per_turn : moyenne 10, sigma multiplier 0.15, clamp multiplier 2.00, minimum 1

### 3.3 Pieces infernales

Configuration runtime actuelle lue dans master_config.json :

- tour minimal avant premier spawn : 3
- cooldown de respawn : 4
- retry si echec de spawn : 1
- lambda Poisson de base : 0.020
- lambda supplementaire par unite de dette sanguine totale : 0.012
- lambda capee : 0.250
- poids de type cible : Pawn 8, Knight 14, Bishop 14, Rook 26, Queen 38

### 3.4 Meteo

Configuration runtime actuelle lue dans master_config.json :

- cooldown minimal entre fronts : 5 tours
- Gamma d'arrivee : shape 2.40, scale 2.20
- vitesse : 50 blocs par 100 tours
- poids de direction : 1 pour chacune des 8 directions, donc uniforme de fait dans la config actuelle
- poids de position d'entree sur bord : centre 1.80, coins 0.70, milieu exact 1.98
- couverture tiree uniformement entre 10 % et 30 %
- aspect ratio tire uniformement sur l'ensemble discret 1.80, 1.81, ..., 2.60
- log-normale de densite : mu -0.12, sigma 0.35
- alpha de base : 0.48, alpha min : 0.22, alpha max : 0.82
- amplitude du bruit de front : 100 %
- douceur de bord : 18 %

### 3.5 Coffres

Le runtime n'ayant pas de section chest active dans les fichiers de config actuels, les valeurs effectives sont celles par defaut de GameConfig::setDefaults() :

- min_spawn_turn : 4
- respawn_cooldown_turns : 4
- spawn_retry_turns : 1
- Weibull shape : 1.80
- Weibull scale : 6 tours
- distance minimale aux rois : 6
- reward Gold : 35
- reward Movement bonus : 1
- reward Build bonus : 1
- bascule late game : tour 10
- poids early game : Gold 8, Movement 3, Build 3
- poids late game : Gold 4, Movement 6, Build 6

## 4. Inventaire detaille des lois et processus

## 4.1 Loi uniforme discrete et variantes conditionnelles

Definition mathematique de reference : une variable uniforme discrete sur un support fini S attribue a chaque element de S la meme probabilite.

### 4.1.1 Generation du worldSeed

Localisation : src/Core/GameEngine.cpp, fonction makeRandomWorldSeed.

Definition dans le code :

- tirage uniforme sur l'intervalle entier ferme allant de 1 a 2147483647
- PRNG : std::mt19937
- source d'entropie initiale : std::random_device

Domaine de definition :

- tous les entiers positifs de l'intervalle [1, 2147483647]

Modification par rapport a une uniforme sur 32 bits non signes :

- la valeur 0 est explicitement exclue, car elle sert de sentinelle interne pour seed absente

Feature servie :

- attribuer un seed de monde a une nouvelle partie quand l'utilisateur n'en fournit pas

Nature :

- loi usuelle stricte
- hors gameplay strict, mais structurante pour tout le reste

### 4.1.2 Rotations aleatoires des ressources publiques

Localisation : src/Board/BoardGenerator.cpp, buildPublicResourcePlacementRequests.

Definition dans le code :

- tirage uniforme sur {0, 1, 2, 3}
- interpretation : nombre de quarts de tour

Domaine de definition :

- 4 orientations discretes

Modification :

- l'eglise centrale est exclue de ce mecanisme et reste fixee a rotation 0

Feature servie :

- varier visuellement mines et fermes sans changer leurs regles

Nature :

- loi usuelle stricte

### 4.1.3 Flip masks aleatoires des ressources publiques

Localisation : src/Board/BoardGenerator.cpp, buildPublicResourcePlacementRequests.

Definition dans le code :

- tirage uniforme sur {0, 1, 2, 3}
- interpretation : aucun flip, horizontal, vertical, ou les deux

Domaine de definition :

- 4 etats discrets

Feature servie :

- diversifier l'apparence des ressources publiques

Nature :

- loi usuelle stricte

### 4.1.4 Placement uniforme parmi candidats ou parmi un top-k disperse

Localisation : src/Board/BoardGenerator.cpp, selectDispersedCandidate et findValidBuildingPos.

Definition dans le code :

- si aucune ressource publique n'est encore placee, le systeme choisit uniformement parmi toutes les cases candidates admissibles
- sinon, il calcule un score de dispersion pour chaque candidat, trie les candidats par score decroissant, garde les meilleurs topCount, puis tire uniformement dans ce sous-ensemble

Parametres et domaine :

- support : ensemble dynamique des positions valides pour le batiment considere
- topCount = min(nombre de candidats, max(3, (nombre de candidats + 5) / 6))

Modification par rapport a une uniforme stricte :

- la loi n'est uniforme que sur le sous-ensemble de tete, pas sur l'ensemble complet des candidats
- elle est donc conditionnelle et top-k, pas uniforme globale

Feature servie :

- eviter le clustering des mines et fermes, tout en conservant de la variete entre parties partageant des contraintes proches

Nature :

- variante maison d'un tirage uniforme conditionnel

### 4.1.5 Spawn joueur et spawn IA

Localisation : src/Board/BoardGenerator.cpp, findSpawnCell.

Definition dans le code :

- le systeme construit d'abord l'ensemble des cases admissibles : case dans le cercle, non eau, sans batiment, et dans la bande horizontale autorisee pour le camp
- il tire ensuite uniformement parmi ces cases

Domaine de definition :

- support fini dynamique dependant de la carte generee et des batiments publics deja poses

Modification :

- la loi est uniforme seulement sur l'ensemble admissible
- en cas d'absence de candidat, le code retombe de facon deterministe sur la premiere case valide parcourue

Feature servie :

- faire varier le point de depart tout en conservant une zone de depart equitable

Nature :

- uniforme discrete conditionnelle sur sous-ensemble dynamique

### 4.1.6 Point d'entree diagonal du front meteo

Localisation : src/Systems/WeatherSystem.cpp, randomElement et entryEdgeForDirection.

Definition dans le code :

- pour une direction diagonale, le front peut entrer par l'un des deux bords compatibles, avec equiprobabilite 1/2 - 1/2
- pour une direction cardinale, le bord d'entree est deterministe

Domaine de definition :

- support de taille 2 pour les diagonales, support de taille 1 pour les cardinales

Feature servie :

- varier geometriquement l'entree des fronts diagonaux

Nature :

- loi usuelle stricte, mais conditionnelle au fait que la direction choisie soit diagonale

### 4.1.7 Couverture du front meteo

Localisation : src/Systems/WeatherSystem.cpp, trySpawnFront.

Definition dans le code :

- tirage uniforme entier entre 10 et 30 dans la configuration actuelle
- cette valeur est interpretee comme un pourcentage de cellules visibles a couvrir approximativement

Domaine de definition :

- entiers de [10, 30]

Modification :

- la variable tiree est discrete, alors que la notion geometrique finale de surface couverte devient continue apres conversion en aire elliptique

Feature servie :

- faire varier la taille globale du front de brouillard

Nature :

- loi usuelle stricte, suivie d'une transformation geometrique

### 4.1.8 Aspect ratio du front meteo

Localisation : src/Systems/WeatherSystem.cpp, trySpawnFront.

Definition dans le code :

- tirage uniforme entier entre 180 et 260, puis division par 100
- le support effectif est donc l'ensemble discret 1.80, 1.81, ..., 2.60

Domaine de definition :

- 81 valeurs reelles discretisees au centieme

Modification :

- ce n'est pas une uniforme continue sur [1.80, 2.60]
- c'est une uniforme discrete sur un maillage de pas 0.01

Feature servie :

- varier l'allongement de l'ellipse qui modelise le front

Nature :

- uniforme discrete transformee

### 4.1.9 Choix uniforme parmi les meilleurs retours de l'infernal

Localisation : src/Systems/InfernalSystem.cpp, chooseReturnBorderCell.

Definition dans le code :

- le systeme calcule d'abord tous les bords de sortie atteignables
- il conserve uniquement ceux dont la distance est minimale
- il tire ensuite uniformement parmi ces ex aequo

Domaine de definition :

- support fini dynamique, compose des seules cases de bord optimales

Modification :

- tirage uniforme sur l'ensemble argmin, pas sur tous les retours possibles

Feature servie :

- casser les ex aequo quand plusieurs sorties de meme qualite existent

Nature :

- uniforme discrete conditionnelle sur un ensemble d'optimums

### 4.1.10 Tirages aleatoires du MCTS

Localisation : src/AI/AIMCTS.cpp.

Trois usages distincts :

1. expansion : choix uniforme d'un enfant parmi ceux qui viennent d'etre crees
2. rollout : choix uniforme d'une piece du royaume actif
3. rollout : choix uniforme d'un coup parmi les coups legaux de la piece retenue

Domaine de definition :

- expansion : indices des enfants du noeud, avec au plus 30 enfants car le code tronque aux MAX_CHILDREN_PER_NODE les plus pertinents
- rollout piece : indices des pieces du royaume actif
- rollout move : indices des coups legaux de la piece retenue

Modifications importantes :

- l'expansion n'est uniforme que sur le sous-ensemble d'actions deja filtrees par pertinence puis tronquees au top 30
- le rollout n'est pas uniforme sur l'ensemble global de tous les coups legaux du plateau ; il est uniforme sur les pieces, puis uniforme sur les coups de la piece choisie
- le rollout abandonne apres 5 tentatives infructueuses de piece sans coup

Feature servie :

- explorer l'arbre et generer des simulations rapides

Nature :

- uniforme discrete conditionnelle et non reproductible par worldSeed
- PRNG initialisee par std::random_device dans un mt19937 thread_local

## 4.2 Permutation aleatoire uniforme

Localisation : src/Board/BoardGenerator.cpp, buildPublicResourcePlacementRequests.

Definition dans le code :

- une fois chaque requete de placement mine ou ferme preparee avec sa rotation et son flip, le vecteur des requetes est melange par std::shuffle

Domaine de definition :

- ensemble des permutations du vecteur des requetes de placement

Modification :

- aucune sur le mecanisme de permutation lui-meme
- en revanche, la permutation agit sur des objets deja porteurs de tirages precedents de rotation et de flip

Feature servie :

- varier l'ordre de pose des ressources publiques, donc la configuration finale du plateau

Nature :

- permutation uniforme usuelle

## 4.3 Loi de Bernoulli

Localisation : src/Systems/InfernalSystem.cpp, chooseTargetKingdom.

Definition dans le code :

- si les deux royaumes ont au moins une cible admissible, le systeme tire un booleen qui choisit White avec la probabilite p = dette_blanche / dette_totale, et Black avec la probabilite 1 - p

Domaine de definition :

- {White, Black}

Parametres effectifs :

- p varie dynamiquement avec la dette sanguine accumulee par chaque royaume
- si la dette totale vaut 0, le code prend p = 0.5

Modification :

- le tirage Bernoulli n'a lieu que si les deux camps sont eligibles ; sinon la cible est choisie de maniere deterministe

Feature servie :

- orienter preferentiellement l'infernal vers le camp qui a accumule le plus de dette sanguine

Nature :

- loi usuelle stricte, mais conditionnelle a l'eligibilite des deux royaumes

## 4.4 Loi categorielle / loi discrete generale ponderee

Definition mathematique de reference : variable discrete sur un support fini dont chaque categorie recoit un poids positif arbitraire, puis une probabilite proportionnelle a ce poids.

### 4.4.1 Direction du front meteo

Localisation : src/Systems/WeatherSystem.cpp, sampleDirection.

Definition dans le code :

- tirage parmi 8 directions : North, South, East, West, NorthEast, NorthWest, SouthEast, SouthWest
- poids lus depuis la config meteo

Parametres actuels :

- tous les poids valent 1 dans master_config.json
- dans la configuration actuelle, cette loi categorielle se reduit donc a une uniforme sur 8 directions, soit 12.5 % chacune

Feature servie :

- choisir l'orientation globale du prochain front

Nature :

- loi discrete generale, uniforme de fait dans la config actuelle

### 4.4.2 Type de recompense du coffre

Localisation : src/Systems/ChestSystem.cpp, sampleReward.

Definition dans le code :

- tirage parmi 3 categories : Gold, MovementPointsMaxBonus, BuildPointsMaxBonus
- les poids changent selon le stade de la partie

Parametres actuels effectifs :

- early game, avant le tour 10 : poids 8, 3, 3
- late game, a partir du tour 10 : poids 4, 6, 6

Probabilites actuelles quand tous les poids sont actifs :

- early game : Gold 57.14 %, Movement 21.43 %, Build 21.43 %
- late game : Gold 25 %, Movement 37.5 %, Build 37.5 %

Modification :

- si la somme des poids est nulle ou negative, le code force un retour Gold, donc bascule sur une issue deterministe

Feature servie :

- faire evoluer la nature moyenne des coffres entre debut et fin de partie

Nature :

- loi categorielle usuelle avec changement de parametres selon la phase

### 4.4.3 Case d'apparition du coffre

Localisation : src/Systems/ChestSystem.cpp, trySpawnChest.

Definition dans le code :

- le support est l'ensemble des cases visibles, non eau, vides, sans batiment ni objet, et suffisamment eloignees des deux rois
- chaque case recoit un poids egal a 1 + centralite + contestation

Definition des poids :

- centralite favorise les cases proches du centre de la carte
- contestation favorise les cases dont les distances aux deux rois sont proches
- la distance minimale aux rois vaut 6 dans la configuration effective actuelle

Modification :

- la loi est categorielle sur un support filtre dynamiquement par l'etat du plateau

Feature servie :

- faire apparaitre les coffres dans des zones a la fois accessibles, centrales et contestables

Nature :

- loi discrete generale ponderee sur support dynamique

### 4.4.4 Type de cible de l'infernal au spawn initial

Localisation : src/Systems/InfernalSystem.cpp, chooseSpawnOptionForKingdom.

Definition dans le code :

- tirage parmi Queen, Rook, Bishop, Knight, Pawn
- un type absent du royaume cible recoit un poids nul

Parametres actuels quand tous les types sont disponibles :

- Queen 38 %
- Rook 26 %
- Bishop 14 %
- Knight 14 %
- Pawn 8 %

Modification :

- si le type tire ne donne aucune option de spawn atteignable depuis un bord, le type est retire, puis le tirage recommence parmi les types restants
- la loi reelle est donc une categorielle avec elimination conditionnelle, pas une simple categorielle en une etape

Feature servie :

- faire emerger en priorite des proies de grande valeur, tout en respectant les contraintes de reachability depuis le bord

Nature :

- loi discrete generale modifiee par rejet et elimination

### 4.4.5 Option de spawn initial de l'infernal

Localisation : src/Systems/InfernalSystem.cpp, chooseSpawnOptionForKingdom.

Definition dans le code :

- une fois le type de cible choisi, le systeme construit toutes les paires atteignables (piece cible, case de bord de spawn)
- chaque paire recoit un poids egal a max(1, 2 * diametre - distance + 1)

Domaine de definition :

- ensemble dynamique des couples atteignables piece cible / case de bord

Modification :

- la distribution est fortement biaisee vers les spawns qui donnent des trajets plus courts vers la cible

Feature servie :

- faire apparaitre l'infernal a un bord plausible et relativement proche de sa proie preferee

Nature :

- loi discrete generale ponderee sur support dynamique

### 4.4.6 Reacquisition de cible par l'infernal

Localisation : src/Systems/InfernalSystem.cpp, chooseReplacementTarget.

Definition dans le code :

- le type de cible est retire de la meme famille categorielle, mais avec un bonus de poids si le type correspond a preferredTargetType
- si un type est prefere et disponible, son poids est double
- une fois le type choisi, la piece cible precise est tiree parmi les cibles atteignables de ce type, avec un poids de proximite identique au spawn initial

Modification :

- doublement du poids de la categorie preferee
- elimination conditionnelle des types sans cible atteignable

Feature servie :

- conserver une coherence de comportement chez l'infernal tout en lui permettant de retomber sur une autre cible atteignable quand sa cible initiale disparait

Nature :

- loi discrete generale ponderee, modifiee par un biais de preference et une elimination conditionnelle

## 4.5 Loi de Poisson

Localisation : src/Systems/InfernalSystem.cpp, trySpawnInfernal.

Definition dans le code :

- a chaque tentative eligible, le systeme tire un entier N suivant une loi de Poisson
- le spawn a lieu si et seulement si N >= 1

Parametres effectifs :

- lambda de base : 0.020
- augmentation par unite de dette totale : 0.012
- plafond : 0.250

Forme de la variable pilotee par le gameplay :

- le jeu n'utilise pas la valeur exacte de N au-dela du seuil 1
- au niveau gameplay, la Poisson est donc immediatement transformee en evenement boolen de probabilite 1 - exp(-lambda)

Plage de probabilite dans la configuration actuelle :

- au minimum, avec dette totale nulle : environ 1.98 % par tentative eligible
- au maximum, au plafond lambda = 0.250 : environ 22.12 % par tentative eligible

Modifications et garde-fous autour de la loi :

- pas de tentative avant le tour minimal 3
- pas de tentative si un infernal est deja actif
- cooldown deterministe entre apparitions
- retry deterministe d'un tour si aucun spawn valide n'a pu etre materialise

Feature servie :

- rendre l'apparition infernale rare mais de plus en plus probable quand la dette sanguine s'accumule

Nature :

- loi de Poisson usuelle, mais aussitot seuillee en evenement binaire

## 4.6 Loi normale, tronquee et discretisee

Localisation : src/Systems/XPSystem.cpp, sampleProfile.

Definition dans le code :

- pour chaque source d'XP, le systeme tire d'abord une loi normale de moyenne mean et d'ecart type sigma
- sigma vaut max(1, mean * sigmaMultiplier)
- l'echantillon continu est ensuite clamp dans l'intervalle [max(minimum, mean - delta), max(minimum, mean + delta)], avec delta = sigma * clampMultiplier
- le resultat est enfin arrondi au plus proche entier

Domaine de definition general :

- une variable discrete a valeurs entieres, obtenue a partir d'une gaussienne continue puis tronquee et arrondie

Conclusion mathematique :

- la loi runtime n'est pas une gaussienne pure
- c'est une gaussienne tronquee puis discretisee

Sous-lois actuelles par source :

- kill_pawn : moyenne 20, sigma 3.6, clamp continu [12.8, 27.2], support entier pratique 13 a 27
- kill_knight : moyenne 50, sigma 8, clamp continu [34, 66], support entier 34 a 66
- kill_bishop : moyenne 50, sigma 8, clamp continu [34, 66], support entier 34 a 66
- kill_rook : moyenne 100, sigma 12, clamp continu [76, 124], support entier 76 a 124
- kill_queen : moyenne 300, sigma 30, clamp continu [240, 360], support entier 240 a 360
- destroy_block : moyenne 10, sigma 1.5, clamp continu [7, 13], support entier 7 a 13
- arena_per_turn : moyenne 10, sigma 1.5, clamp continu [7, 13], support entier 7 a 13

Feature servie :

- faire varier les recompenses d'XP tout en gardant une plage lisible, equilibrable et facilement bornable

Nature :

- loi normale modifiee, bornee et discretisee

## 4.7 Loi de Weibull, discretisee et bornee inferieurement

Localisation : src/Systems/ChestSystem.cpp, sampleSpawnDelay.

Definition dans le code :

- le delai brut suit une loi de Weibull
- le tirage est ensuite arrondi a l'entier le plus proche
- le resultat final est force a etre au moins egal au cooldown de respawn

Parametres effectifs actuels :

- shape : 1.80
- scale : 6 tours
- cooldown minimum final : 4 tours

Domaine de definition code :

- entiers >= 4

Modification par rapport a une Weibull usuelle :

- discretisation par arrondi
- troncature inferieure via un plancher deterministic a 4

Feature servie :

- espacer les apparitions de coffres avec une variabilite non purement uniforme, tout en garantissant un temps de repos minimal

Nature :

- loi de Weibull modifiee par arrondi et plancher

## 4.8 Loi Gamma, translatee et discretisee

Localisation : src/Systems/WeatherSystem.cpp, sampleGammaTurns et scheduleNextSpawn.

Definition dans le code :

- le systeme tire une loi Gamma continue
- il applique ensuite un ceil
- il ajoute enfin un minimumTurns fixe
- le tout est converti en demi-tours car la meteo avance sur 2 steps par tour

Parametres effectifs actuels :

- shape : 2.40
- scale : 2.20
- minimumTurns ajoute : 5
- kStepsPerTurn : 2

Domaine de definition :

- au niveau des tours, entiers >= 5, avec 6 comme premier resultat pratique attendu pour une Gamma strictement positive
- au niveau du scheduler interne, multiples de 2 demi-tours

Modification par rapport a une Gamma usuelle :

- translation par un minimum fixe
- discretisation par ceil
- conversion ulterieure en demi-tours

Feature servie :

- piloter le delai entre deux fronts meteo

Nature :

- loi Gamma modifiee et discretisee

Important :

- les parametres weather.duration_gamma_shape_times_100 et weather.duration_gamma_scale_times_100 existent dans GameConfig et dans master_config.json, mais ils ne sont pas utilises par le code runtime actuel
- la duree effective d'un front en jeu n'est donc pas tiree directement par une loi Gamma ; elle est deduite de la geometrie du front, de sa vitesse et des autres tirages de spawn

## 4.9 Loi log-normale, puis clamp sur l'opacite meteo

Localisation : src/Systems/WeatherSystem.cpp, sampleLogNormalCell et concealmentAlpha.

Definition dans le code :

- pour chaque cellule du front, le moteur tire un multiplicateur de densite suivant une loi log-normale
- ce multiplicateur est applique a une alpha de base
- le resultat est ensuite clamp entre alpha_min et alpha_max
- l'alpha finale du pixel de brouillard est encore multipliee par edgeFade, qui depend de la distance au bord du front

Parametres effectifs actuels :

- mu : -0.12
- sigma : 0.35
- alpha_base : 0.48
- alpha_min : 0.22
- alpha_max : 0.82

Domaine de definition :

- multiplicateur log-normal brut : reel strictement positif
- alpha locale apres clamp : reel de [0.22, 0.82]
- alpha finale apres edgeFade : reel de [0, 1]

Modification par rapport a une log-normale usuelle :

- la quantite jouee n'est pas directement la sortie log-normale, mais une transformation clamp(alpha_base * densite)
- la sortie est donc bornee et absorbe les grandes queues de la log-normale

Feature servie :

- varier localement l'opacite du brouillard, afin d'eviter un front visuellement trop homogene

Nature :

- loi log-normale modifiee par transformation affine partielle et clamp

## 4.10 Loi Beta, echantillonnee via Gamma puis transformee

Localisation : src/Board/BoardGenerator.cpp, sampleBeta et terrainBrightnessFor.

Definition dans le code :

- le moteur echantillonne une Beta(alpha, beta) par la methode classique Gamma / Gamma
- cette Beta ne devient pas directement la luminosite finale
- si l'echantillon est >= 0.90, la luminosite reste a 100 %
- sinon l'echantillon est renormalise par 0.90, eleve a la puissance 1.8, puis remappe lineairement dans l'intervalle [0.68, 1.00]
- la luminosite est enfin convertie en octet 0 a 255

Parametres effectifs actuels :

- alpha : 7.0
- beta : 2.0
- seuil de conservation pleine luminosite : 0.90
- luminosite minimale : 0.68
- exposant de contraste : 1.8

Domaine de definition :

- echantillon Beta brut : reel de [0, 1]
- luminosite finale normalisee : reel de [0.68, 1.00]
- luminosite stockee : entier 173 a 255 environ

Modification par rapport a une Beta usuelle :

- la Beta n'est qu'une variable intermediaire
- la loi finale jouee sur la luminosite est une Beta transformee, censuree au-dessus de 0.90, puis non lineairement contrastree

Feature servie :

- introduire des variations fines de teinte sur l'herbe sans sortir d'une palette proche du vert standard

Nature :

- loi Beta modifiee de maniere substantielle

## 4.11 Loi lineaire par morceaux

Localisation : src/Systems/WeatherSystem.cpp, sampleEdgePosition.

Definition dans le code :

- le point d'entree du front sur le bord est tire selon une loi continue lineaire par morceaux sur l'intervalle [0, diametre - 1]
- les noeuds sont 0 %, 25 %, 50 %, 75 %, 100 % du bord
- les poids donnent plus de masse au centre qu'aux coins

Parametres effectifs actuels :

- coins : 0.70
- quarts : 1.80
- centre exact : 1.98

Domaine de definition :

- reel continu dans [0, diametre - 1]

Modification :

- aucune sur la loi lineaire par morceaux elle-meme
- mais la variable n'est ensuite interpretee geometriquement qu'en position de bord, donc son effet passe par la projection du front sur la carte

Feature servie :

- faire entrer plus souvent les fronts par des zones laterales relativement centrales plutot que par les coins purs

Nature :

- loi standard explicite de la bibliotheque C++

## 4.12 Processus pseudo-aleatoires spatiaux et autres mecanismes non standards

### 4.12.1 Terrain : value noise puis fBm

Localisation : src/Board/BoardGenerator.cpp, hashValue, valueNoise, fractalNoise, generate.

Definition dans le code :

- deux seeds internes sont d'abord tires depuis le mt19937 principal du monde : une pour la terre, une pour l'eau
- hashValue produit un pseudo-uniforme discret a partir de la seed et de la position
- valueNoise interpole bilineairement ces valeurs sur la grille
- fractalNoise empile plusieurs octaves avec amplitude divisee par 2 et frequence multipliee par 2
- le score de terre et le score d'eau sont ensuite combines, compares a des seuils choisis pour atteindre des couvertures cibles, puis filtres par pruning de composantes et test de connectivite

Nature probabiliste :

- ce n'est pas une suite de variables independantes identiquement distribuees
- c'est un champ pseudo-aleatoire spatialement correle, seed-driven

Domaines derives :

- les scores sont reels
- les types finaux de cellule sont des variables discretes parmi Grass, Dirt, Water, mais obtenues par seuillage et contraintes globales, pas par loi elementaire simple

Feature servie :

- generer un relief tactique coherent, varie et reproductible

### 4.12.2 Bruit de forme du front meteo

Localisation : src/Systems/WeatherSystem.cpp, valueNoise et concealmentAlpha.

Definition dans le code :

- le bord du front n'est pas une ellipse parfaite
- un value noise deterministe, calibre par shapeSeed, cellSpan et amplitude, deforme la frontiere effective cellule par cellule

Parametres effectifs actuels :

- cellSpan : 6
- amplitude : 100 %

Nature probabiliste :

- champ pseudo-aleatoire correle, deterministe a seed fixe

Feature servie :

- casser la regularite geometrique du front et rendre le brouillard plus organique

### 4.12.3 Teinte locale du brouillard

Localisation : src/Systems/WeatherSystem.cpp, concealmentShade.

Definition dans le code :

- un hash pseudo-uniforme par cellule module legerement la teinte de gris du brouillard
- la teinte est ensuite assombrie selon l'alpha, puis clamp entre 160 et 225

Domaine de definition :

- niveau de gris entier dans [160, 225]

Nature probabiliste :

- pseudo-uniforme deterministe par hachage, pas loi usuelle explicite

Feature servie :

- eviter un voile de brouillard trop plat visuellement

### 4.12.4 Variantes visuelles par hachage deterministe

Localisation :

- src/Board/BoardGenerator.cpp, terrainFlipMaskFor
- src/Core/GameEngine.cpp, deriveLegacyPublicBuildingRotation et deriveLegacyPublicBuildingFlipMask

Definition dans le code :

- certaines variantes visuelles ne sont pas tirees par un PRNG a l'execution immediate, mais derivees deterministiquement d'un hash melangeant worldSeed, type, position ou metadonnees de batiment

Nature probabiliste :

- pseudo-random deterministe, a support fini, mais pas echantillonnage explicite d'une loi standard

Feature servie :

- donner de la variete visuelle stable et reproductible
- reconstituer proprement l'apparence des anciennes sauvegardes

### 4.12.5 Sorties brutes de PRNG utilisees comme seeds internes

Localisation :

- src/Board/BoardGenerator.cpp : dirtNoiseSeed et waterNoiseSeed
- src/Systems/WeatherSystem.cpp : shapeSeed et densitySeed

Definition dans le code :

- le moteur reutilise parfois directement la sortie brute d'un mt19937 seed-driven pour fabriquer une nouvelle seed interne

Domaine de definition :

- entier 32 bits du generateur sous-jacent

Nature probabiliste :

- assimilable a une uniforme discrete sur l'espace de sortie du PRNG, mais employee ici comme mecanisme d'amorcage interne plutot que comme variable de gameplay visible

Feature servie :

- decorreler differents sous-processus aleatoires tout en restant deterministe a worldSeed fixe

## 4.13 Hors gameplay strict : generation de sel multijoueur

Localisation : src/Multiplayer/PasswordUtils.hpp, generateSalt.

Definition dans le code :

- le systeme instancie un mt19937_64 avec std::random_device
- il prend deux sorties brutes successives du generateur
- il les convertit en hexadecimal et les concatene

Domaine de definition :

- chaine hexadecimale sur 128 bits issus de deux sorties 64 bits

Modification :

- aucune loi standard explicite n'est echantillonnee ; il s'agit de sorties brutes de PRNG
- le mecanisme n'est pas cryptographiquement robuste au sens d'un CSPRNG, puisqu'il repose sur mt19937_64

Feature servie :

- fournir un sel de session pour le digest de mot de passe multijoueur LAN

Nature :

- pseudo-uniforme 64 bits repete deux fois, hors gameplay

## 4.14 Faux amis, parametres inutilises ou comportements nommes aleatoires mais non stochastiques

### 4.14.1 Parametre ai.randomness

Localisation :

- src/AI/AIStrategy.cpp
- src/Config/AIConfig.cpp
- assets/config/master_config.json et assets/config/ai_params.json

Etat actuel :

- le parametre randomness est bien charge et borne dans [0, 1]
- la configuration courante le fixe a 0.0
- meme si sa valeur etait > 0, le comportement code n'utiliserait pas un tirage aleatoire : il choisirait parfois le second objectif sur la base deterministe de turnNumber % 3 quand l'ecart de score entre top 1 et top 2 est < 10

Conclusion :

- ce champ ne correspond pas, dans le runtime actuel, a une vraie loi aleatoire
- c'est un gate de variation deterministe, pas une variable aleatoire usuelle

### 4.14.2 Parametres weather.duration_gamma_*

Localisation :

- exposes dans GameConfig
- presents dans master_config.json
- non utilises dans WeatherSystem.cpp

Conclusion :

- la duree du front meteo n'est pas echantillonnee par une Gamma dans le runtime actuel
- la duree effective est une variable composee induite par direction, point d'entree, couverture, aspect ratio, vitesse et geometrie de sortie de carte

### 4.14.3 deriveLegacyWorldSeed

Localisation : src/Core/GameEngine.cpp.

Etat actuel :

- ce mecanisme derive de maniere deterministe une seed a partir du nom de partie, du numero de tour, du rayon de carte et du royaume actif pour les vieilles sauvegardes

Conclusion :

- ce n'est pas une variable aleatoire
- c'est un hash deterministe de retrocompatibilite

## 5. Synthese par feature de jeu

### 5.1 Generation de monde

Le monde repose sur quatre grandes briques aleatoires :

- generation initiale du worldSeed par uniforme discrete si aucun seed n'est fourni
- terrain seed-driven via value noise + fBm, seuils de couverture, pruning et contraintes de connectivite
- variations visuelles de l'herbe via Beta transformee
- placement des ressources publiques via rotations/flips uniformes, permutation aleatoire et choix uniforme sur un top-k disperse

### 5.2 Evenements de carte

- les coffres utilisent une Weibull pour le delai entre apparitions et une loi categorielle pour le type de recompense
- leur case d'apparition est tiree par loi categorielle ponderee privilegiante centralite et contestation
- l'infernal utilise une Poisson seuillee pour declencher l'evenement, une Bernoulli pour choisir le royaume cible, puis plusieurs lois categorielles pour choisir le type de proie et la materialisation precise
- la meteo utilise une Gamma pour l'intervalle entre fronts, une categorielle pour la direction, une lineaire par morceaux pour la position d'entree, des uniformes discretes pour la couverture et l'aspect ratio, une log-normale pour la densite locale, puis un bruit spatial pour casser la frontiere

### 5.3 Progression des unites

- l'XP n'est pas deterministe point par point : elle suit une famille de gaussiennes tronquees et discretisees, avec un profil distinct selon la source de gain

### 5.4 IA

- le MCTS utilise des tirages uniformes pour explorer et simuler
- ces tirages ne sont pas relies au worldSeed et introduisent une non reproductibilite gameplay residuelle meme a worldSeed fixe et a etat serialize identique
- le champ ai.randomness, lui, ne declenche pas aujourd'hui de vraie stochasticite

## 6. Conclusion generale

Le runtime actuel du jeu utilise effectivement un ensemble riche de lois usuelles et de processus aleatoires : uniforme discrete, Bernoulli, categorielle ponderee, Poisson, normale, Weibull, Gamma, log-normale, Beta, piecewise linear et permutation uniforme. A cela s'ajoutent des processus seed-driven plus specifiques au jeu, surtout pour la generation spatiale du terrain et la forme du brouillard.

Les deux points d'architecture les plus importants sont les suivants :

- presque tout le gameplay aleatoire est replayable a worldSeed fixe grace a la serialisation des rngCounter
- le MCTS de l'IA et le sel multijoueur sont en dehors de ce schema et restent non reproductibles entre executions

En pratique, si l'objectif est d'auditer toutes les variables aleatoires du jeu, il faut retenir a la fois les lois explicites ci-dessus et les processus pseudo-aleatoires seed-driven qui transforment ces tirages en terrain, meteo, evenements et variations visuelles.