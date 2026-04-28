# Rapport d'audit exhaustif des processus aleatoires du runtime gameplay

## 1. Objet, perimetre, methode

Ce document liste les processus aleatoires effectivement utilises par le runtime gameplay actuel.

Perimetre retenu:

- gameplay runtime uniquement
- etat actuel du code comme source d'autorite
- pas de reprise des anciens plans/rapports comme verite fonctionnelle
- pas d'inclusion des utilitaires hors gameplay, des generateurs HTML, ni des hypotheses sur un dossier AI absent dans ce workspace

Sources de verite inspectees:

- `src/Board/BoardGenerator.cpp`
- `src/Systems/XPSystem.cpp`
- `src/Systems/RewardProfileSampling.hpp`
- `src/Systems/ChestSystem.cpp`
- `src/Systems/ChestLootProgression.cpp`
- `src/Systems/WeatherSystem.cpp`
- `src/Systems/InfernalSystem.cpp`
- `src/Config/GameConfig.cpp`
- `src/Core/Game.cpp`
- `src/Save/SaveManager.cpp`
- `assets/config/master_config.json`

Conclusion courte:

1. Tous les processus aleatoires gameplay actuellement actifs sont bien derives du `worldSeed` de la partie, directement ou via des seeds derives du `worldSeed`.
2. Les systemes stateful XP, coffres, meteo et infernal utilisent un schema `worldSeed + rngCounter serialise`, ce qui permet une reprise deterministe apres save/load.
3. La generation de carte est un processus one-shot base directement sur `worldSeed`; le resultat concret de la carte est ensuite serialise dans la sauvegarde, y compris les visuels de terrain.
4. La meteo ajoute un cas particulier important: les fronts actifs serialisent aussi leurs `shapeSeed` et `densitySeed`, necessaires pour reconstruire exactement le masque de brouillard en cours.

## 2. Architecture transversale du determinisme

### 2.1 Precedence de configuration runtime

Le jeu charge maintenant `assets/config/master_config.json` comme source unique de configuration runtime. Les valeurs runtime actuelles proviennent donc de `master_config.json`, completees par les defaults de `GameConfig::setDefaults()` pour les cles non exposees.

Consequences pour l'aleatoire:

- les familles XP, infernal, meteo et coffres sont pilotees par des cles presentes dans `master_config.json`
- certains parametres de spawn ou de distribution restent sur defaults code lorsqu'ils ne sont pas encore exposes dans `master_config.json`

### 2.2 Trois patterns de generation aleatoire

#### A. Flux evenementiel `worldSeed + rngCounter`

Schema commun:

- `baseSeed = (worldSeed == 0) ? 1u : worldSeed`
- `seed = mixSeed(baseSeed, rngCounter++)`
- `std::mt19937(seed)` pour l'evenement courant

Ce pattern est utilise par:

- XP
- spawn de coffres
- reward des coffres via un compteur separe
- meteo
- infernal

Ce pattern garantit:

- une suite pseudo-aleatoire deterministe par systeme
- une reprise exacte apres chargement si le compteur et l'etat associe sont serialises
- une isolation partielle des flux quand un systeme a plusieurs compteurs dedies

#### B. Generation one-shot directement seedee par `worldSeed`

La generation de carte cree un `std::mt19937(worldSeed == 0 ? 1u : worldSeed)` puis derive localement ses sous-processus. Il n'y a pas de compteur serialise pour rejouer la generation en cours, car la carte generee et ses visuels sont deja sauvegardes explicitement.

#### C. Champs proceduraux derives de seeds fixes

Certains effets ne tirent pas des evenements independants mais un champ spatial deterministe a partir d'un seed fixe:

- bruit Dirt/Water de la carte
- flip mask de terrain
- luminosite de l'herbe
- contour de front meteo via `valueNoise`
- densite locale meteo via log-normale reseedee par cellule

Ce ne sont pas des suites libres qui avancent a chaque frame, mais des fonctions deterministes de `(seed, position)`.

### 2.3 Resume des etats serialises qui preservent l'aleatoire

| Systeme | Etat serialise utile a la reproductibilite |
| --- | --- |
| Partie | `worldSeed` |
| Carte | la grille elle-meme, avec `type`, `terrainFlipMask`, `terrainBrightness` |
| XP | `xpState.rngCounter` |
| Coffres | `chestState.rngCounter`, `rewardRngCounter`, etat complet de `lootProgression` |
| Meteo | `weatherState.rngCounter`, fronts actifs complets, `shapeSeed`, `densitySeed`, progression du front, cache de masque |
| Infernal | `infernalState.rngCounter`, `nextSpawnTurn`, dettes de sang, identifiant actif |

## 3. Inventaire detaille par systeme

### 3.1 Generation de carte et visuels de terrain

Parametres runtime actifs issus de `master_config.json`:

- rayon de carte: `25`
- mines neutres: `2`
- fermes neutres: `3`
- distance min batiments publics: `10`
- largeur zone spawn joueur: `25%`
- largeur zone spawn adversaire: `25%`
- `terrain_noise_scale = 14`
- `terrain_octaves = 3`
- couverture Dirt cible: `14%`
- couverture Water cible: `4%`
- Dirt blobs: `6`, rayons configures `2..5`
- Lakes: `3`, rayons configures `2..3`

| Processus | Variable / famille usuelle | Parametres effectifs | Divergence / particularite | Seed et usage | But gameplay |
| --- | --- | --- | --- | --- | --- |
| Seed Dirt global | Tirage entier brut depuis `mt19937` | derive du `worldSeed` a la creation de la carte | pas expose directement en config | `dirtNoiseSeed = random()` | seed interne du champ Dirt |
| Champ Dirt | champ continu pseudo-aleatoire de type value-noise/fBm | `noiseScale=14`, `octaves=3`, coverage cible `14%`, `6` regions, taille region guidee par rayons `2..5` | pas une suite de variables independantes; champ spatial correle, puis threshold/pruning/selection de composantes deterministes | `dirtNoiseSeed` derive du `worldSeed` | faconner les zones Dirt hors colonnes protegees de spawn |
| Seed Water global | Tirage entier brut depuis `mt19937` | derive du `worldSeed` a la creation de la carte | meme logique que Dirt | `waterNoiseSeed = random()` | seed interne du champ Water |
| Champ Water | champ continu pseudo-aleatoire de type value-noise/fBm, avec biais annulaire | `noiseScale=14`, `octaves=3`, coverage cible `4%`, `3` regions, tailles `2..3` | ajout d'un `waterRingBias`; threshold/pruning/selection ensuite deterministes; suppression de region si elle casse la connectivite gauche-droite | `waterNoiseSeed` derive du `worldSeed` | faconner les lacs sans couper la carte |
| Rotation des mines/fermes neutres | uniforme discrete | `U{0,1,2,3}` pour chaque request | aucune | flux carte seed par `worldSeed` | varier l'orientation des batiments publics |
| Flip des mines/fermes neutres | uniforme discrete | `U{0,1,2,3}` sur les masques H/V | aucune | flux carte seed par `worldSeed` | varier le mirroring des batiments publics |
| Ordre de placement des mines/fermes | permutation uniforme (`shuffle`) | sur l'ensemble des `2 + 3 = 5` placements neutres | aucune | flux carte seed par `worldSeed` | eviter un ordre fixe mine puis ferme |
| Choix de position batiment public | uniforme discrete conditionnelle | premier batiment: uniforme sur tous les candidats; ensuite uniforme sur un top-k score deterministe, avec `topCount = min(N, max(3, (N+5)/6))` | ce n'est pas une uniforme sur tout l'espace lorsque des batiments existent deja; le score de dispersion filtre d'abord | flux carte seed par `worldSeed` | produire une dispersion plausible des ressources publiques |
| Spawn joueur blanc | uniforme discrete | uniforme sur les cellules valides de la zone gauche | fallback deterministe si plus aucun candidat | flux carte seed par `worldSeed` | position initiale du roi/joueur |
| Spawn joueur noir | uniforme discrete | uniforme sur les cellules valides de la zone droite | fallback deterministe si plus aucun candidat | flux carte seed par `worldSeed` | position initiale du roi/adversaire |
| Luminosite de l'herbe | Beta transformee via Gamma/Gamma | `Beta(alpha=7.0, beta=2.0)`, seuil conserve a `0.90`, contraste `1.8`, brightness min `0.68` | seulement sur `Grass`; la Beta est ensuite ecrasee vers `1.0` au-dessus du seuil et remappee non lineairement en dessous | seed local derive de `worldSeed`, d'un salt fixe et de la position | casser la monotonie visuelle du terrain herbe |
| Flip mask du terrain | pseudo-uniforme discrete par hash | support `{0,1,2,3}` hors `Void` | pas tire par `uniform_int_distribution`; equivalence par extraction de bits d'un hash mixe | hash de `worldSeed`, position et type de cellule | varier l'orientation des sprites de terrain |

Notes importantes:

- apres generation des scores Dirt/Water, les etapes `selectThreshold`, `pruneSparseMask`, `extractComponents`, `selectComponents` et le test de connectivite sont deterministes
- l'eglise centrale n'introduit pas un nouveau tirage: sa position, sa rotation et son flip sont fixes; seuls ses visuels de terrain reutilisent les fonctions pseudo-aleatoires de flip/luminosite deja inventoriees
- la sauvegarde n'a pas besoin de reserialiser les seeds Dirt/Water: elle stocke directement la grille finale et ses visuels

### 3.2 XP

Famille utilisee:

- normale tronquee puis discretisee en entier par arrondi
- implementation commune dans `RewardProfileSampling::sampleTruncatedNormal`
- si `sigmaMultiplier <= 0` ou `clampSigmaMultiplier <= 0`, la variable degenerate en valeur fixe `max(minimum, mean)`

Parametres runtime actifs issus de `master_config.json`:

| Source XP | mean | sigma multiplier | clamp sigma multiplier | minimum |
| --- | ---: | ---: | ---: | ---: |
| kill_pawn | 20 | 0.18 | 2.00 | 1 |
| kill_knight | 50 | 0.16 | 2.00 | 1 |
| kill_bishop | 50 | 0.16 | 2.00 | 1 |
| kill_rook | 100 | 0.12 | 2.00 | 1 |
| kill_queen | 300 | 0.10 | 2.00 | 1 |
| destroy_block | 10 | 0.15 | 2.00 | 1 |
| arena_per_turn | 10 | 0.15 | 2.00 | 1 |

Caracterisation runtime:

- variable associee: quantite d'XP attribuee a l'evenement
- variable de reference usuelle: `X ~ N(mean, sigma)` puis `X` tronquee a `[mean - delta, mean + delta]`, avec `sigma = max(1, mean * sigmaMultiplier)` et `delta = sigma * clampMultiplier`
- divergence implementation: le minimum gameplay est reapplique a la fin, puis la valeur est arrondie par `lround`
- seed: `worldSeed + xpState.rngCounter`, un tirage par attribution XP
- usage du seed: garantir que les memes evenements dans le meme ordre redonnent les memes montants apres chargement

Il n'existe pas d'autre source d'aleatoire XP dans le runtime actuel. Les seuils d'evolution sont deterministes.

### 3.3 Coffres

Etat de config runtime actuel:

- valeurs exposees dans `master_config.json`:
  - `current_loot_catch_up_enabled = true`
  - poids early: gold `8`, move bonus `3`, build bonus `3`
  - poids late: gold `4`, move bonus `6`, build bonus `6`
  - `gold_reward.mean = 35`
  - `gold_reward.sigma_multiplier_times_100 = 18`
  - `gold_reward.clamp_sigma_multiplier_times_100 = 200`
  - `gold_reward.minimum = 1`
- valeurs encore sur defaults code:
  - `min_spawn_turn = 4`
  - `respawn_cooldown_turns = 4`
  - `spawn_retry_turns = 1`
  - `weibull_shape_times_100 = 180`
  - `weibull_scale_turns = 6`
  - `min_distance_from_kings = 6`
  - `movement_bonus_amount = 1`
  - `build_bonus_amount = 1`
  - `late_game_turn = 10`

Le systeme coffre utilise deux flux pseudo-aleatoires distincts:

- `chestState.rngCounter` pour le spawn des coffres
- `chestState.rewardRngCounter` pour les recompenses

| Processus | Variable / famille usuelle | Parametres effectifs | Divergence / particularite | Seed et usage | But gameplay |
| --- | --- | --- | --- | --- | --- |
| Delai avant prochain coffre | Weibull discretisee | forme `1.80`, echelle `6` tours, puis `max(respawnCooldown=4, round(sample))` | la variable n'est pas une Weibull brute: elle est arrondie a l'entier puis planchee par le cooldown | `worldSeed + chestState.rngCounter` | espacer les apparitions de coffres |
| Case d'apparition du coffre | categorielle ponderee (`discrete_distribution`) | poids cellule = `1 + centrality + contestation`, avec exclusion des cases eau/batiment/piece/objet et distance min `6` des deux rois | la loi porte sur un espace de cellules filtre; si aucun candidat, pas de tirage et retry dans `1` tour | `worldSeed + chestState.rngCounter` | faire apparaitre le coffre plutot vers des zones contestables et centrales |
| Type de recompense | categorielle ponderee | early `8/3/3`, late `4/6/6`, bascule a partir du tour `10` | si la somme des poids est `<= 0`, fallback deterministe sur Gold | `worldSeed + chestState.rewardRngCounter` | faire evoluer la nature du loot selon la phase de partie |
| Montant d'or du coffre | normale tronquee discretisee | `mean=35`, `sigma=35*0.18=6.3`, clamp `+- 12.6`, `minimum=1` | meme helper que XP; si sigma ou clamp etaient nuls, on retomberait sur une valeur fixe | `worldSeed + chestState.rewardRngCounter` | eviter un gold fixe tout en gardant une moyenne controlee |

Regle de catch-up associee:

- `current_loot_catch_up_enabled = true` active un mecanisme stateful de "reward courante partagee"
- cette regle n'ajoute pas une nouvelle loi aleatoire, mais controle quand un nouveau tirage a lieu
- tant qu'une generation de reward courante n'a pas ete consommee par les deux royaumes, le systeme rejoue la meme reward partagee
- la sauvegarde serialise `hasCurrentReward`, `currentRewardGeneration`, `currentRewardType`, `currentRewardAmount`, ainsi que les dernieres generations consommees par blanc/noir

Autrement dit, dans le mode courant, l'aleatoire coffre ne se resume pas a "un tirage a chaque ouverture": il est medie par un etat de progression explicitement persiste.

### 3.4 Meteo

Parametres runtime actifs issus de `master_config.json`:

- `cooldown_min_turns = 0`
- `block_spawn_while_front_active = false`
- arrivee gamma: shape `4.00`, scale `10.00`
- duree gamma: shape `2.60`, scale `1.80`
- vitesse: `200` blocs / 100 tours, soit `2.0` blocs par tour
- poids de direction: tous a `1`
- entree: poids centre `1.80`, poids coin `0.70`
- couverture: `5..20%`
- aspect ratio: `1.80..2.60`
- `shape_noise_cell_span = 6`
- `shape_noise_amplitude_percent = 100`
- `edge_softness_percent = 18`
- `alpha_base_percent = 48`
- `alpha_min_percent = 22`
- `alpha_max_percent = 82`
- log-normale densite: `mu = -0.12`, `sigma = 0.35`

| Processus | Variable / famille usuelle | Parametres effectifs | Divergence / particularite | Seed et usage | But gameplay |
| --- | --- | --- | --- | --- | --- |
| Delai entre fronts | Gamma decalee et discretisee | `minimumTurns=0`, shape `4.00`, scale `10.00`, puis `ceil(sample)` | la variable est entiere apres `ceil`; `scheduleNextSpawn` reste responsable des bornes de calendrier | `worldSeed + weatherState.rngCounter` | piloter la cadence d'apparition des fronts |
| Direction du front | categorielle ponderee | 8 directions, poids tous a `1` actuellement | aucune; directions orthogonales et diagonales sont equiprobables avec la config actuelle | `worldSeed + weatherState.rngCounter` | choisir l'axe d'entree et de deplacement |
| Bord d'entree pour une diagonale | uniforme discrete binaire | pour une diagonale, choix equiprobable entre ses 2 bords compatibles | pas de tirage pour les directions cardinales: le bord y est deterministe | meme flux meteo | choisir par quel cote concret entre une diagonale |
| Position le long du bord | piecewise linear continue | noeuds a `0%`, `25%`, `50%`, `75%`, `100%`; poids `[corner, center, center*1.1, center, corner]` avec `center=1.8`, `corner=0.7` | ce n'est pas une uniforme: le centre du bord est privilegie | meme flux meteo | faire entrer les fronts plus souvent vers le milieu du plateau |
| Couverture cible | uniforme discrete | entier `U[5,20]` en pourcentage | aucune | meme flux meteo | fixer l'aire cible du front |
| Aspect ratio cible | uniforme discrete puis re-echelle | entier `U[180,260] / 100`, soit `1.80..2.60` | discrete avant conversion en flottant | meme flux meteo | regler l'elongation initiale de l'ellipse |
| Duree visible cible | Gamma discretisee | shape `2.60`, scale `1.80`, puis `ceil(sample)` et `max(1, ...)` | point critique: cette gamma ne fixe pas directement `totalTurnSteps`; elle cible une duree visible, puis le systeme dilate/contracte l'ellipse le long de l'axe de trajet pour approcher cette duree a vitesse constante | meme flux meteo | controler la longevite perceptible du front |
| `shapeSeed` du front | tirage entier brut | `generator()` | seed derivee et serialisee, pas un parametre config | meme flux meteo, puis serialise dans le front | alimenter le bruit de contour du front |
| `densitySeed` du front | tirage entier brut | `generator()` | meme logique que `shapeSeed` | meme flux meteo, puis serialise dans le front | alimenter la densite locale du brouillard |
| Bruit de contour du front | value-noise spatial | `shape_noise_cell_span=6`, amplitude `100%` | champ spatial correle, pas des cellules independantes; il perturbe la frontiere effective avant le fade | `shapeSeed`, position cellule | casser la geometrie trop parfaite du contour |
| Densite locale du front | log-normale par cellule | `mu=-0.12`, `sigma=0.35`, puis alpha locale `clamp(alphaBase * sample, alphaMin, alphaMax)` avec base `0.48`, bornes `0.22..0.82` | la log-normale est re-seedee de facon deterministe par cellule a partir de `densitySeed`; l'alpha finale depend aussi du fade de bord | `densitySeed`, position cellule | varier localement l'opacite du brouillard |

Notes meteo importantes:

- la vitesse du front n'est pas aleatoire avec la configuration courante; elle est entierement deterministe via `speed_blocks_per_100_turns`
- quand `block_spawn_while_front_active = false`, un nouveau delai gamma peut etre reschedulable alors qu'un front est deja actif; la stochasticite de cadence reste cependant pilotee par le meme compteur serialize
- le cache de masque meteo (`alphaByCell`, `shadeByCell`, revision) est sauvegarde, mais surtout les fronts actifs sauvegardent les seeds et leur progression, ce qui est la cle de la reconstruction deterministe

### 3.5 Infernal

Parametres runtime actifs issus de `master_config.json`:

- `min_spawn_turn = 3`
- `respawn_cooldown_turns = 4`
- `spawn_retry_turns = 1`
- `poisson_lambda_base_times_1000 = 20`
- `poisson_lambda_per_debt_times_1000 = 12`
- `poisson_lambda_cap_times_1000 = 250`
- `blood_debt_decay_percent = 95`
- dettes par piece: pawn `1`, knight `2`, bishop `2`, rook `3`, queen `5`
- dette degats structure: `1`
- poids de cible: pawn `8`, knight `14`, bishop `14`, rook `26`, queen `38`
- `searching_random_move_chance_times_1000 = 333`

| Processus | Variable / famille usuelle | Parametres effectifs | Divergence / particularite | Seed et usage | But gameplay |
| --- | --- | --- | --- | --- | --- |
| Gate de spawn infernal | Poisson | `lambda = clamp(0.02 + 0.012 * (whiteDebt + blackDebt), 0, 0.25)`; spawn si `sample >= 1` | on ne consomme pas directement le nombre d'occurrences Poisson: le tirage sert de test d'existence d'un spawn | `worldSeed + infernalState.rngCounter` | rendre l'apparition plus probable quand la dette de sang augmente |
| Royaume cible | Bernoulli | `p(White) = whiteDebt / (whiteDebt + blackDebt)`, ou `0.5` si total nul | fallback deterministe si un seul royaume a des pieces eligibles | meme flux infernal | choisir qui est chasse prioritairement |
| Type de piece cible primaire | categorielle ponderee | poids par type `8,14,14,26,38` sur les types effectivement presents | si la somme des poids disponibles est `<= 0`, fallback sur `targets.front()` ou `nullopt` selon le chemin | meme flux infernal | manifester preferentiellement une piece infernale coherente avec la cible |
| Option de spawn vers cible visible | categorielle ponderee | poids option = `max(1, 2 * boardDiameter - distance + 1)` sur toutes les options atteignables | la loi porte sur des paires `(cible, cellule frontiere)` atteignables; la distance est un plus court chemin gameplay, pas une distance geometrique simple | meme flux infernal | privilegier les spawns frontiere qui rapprochent de la cible |
| Spawn frontiere fallback | uniforme discrete | uniforme sur les cellules de bord quand aucun spawn cible n'a ete resolu | seulement utilise en phase Searching si aucun objectif visible atteignable n'existe | meme flux infernal | tout de meme faire apparaitre l'infernal |
| Type de remplacement en chasse | categorielle ponderee biaisee | memes poids par type, mais poids doubles pour `preferredTargetType` s'il est disponible | le biais n'est pas additif fixe: il double le poids existant du type prefere | meme flux infernal | conserver une inertie de comportement dans le retargeting |
| Cible de remplacement | categorielle ponderee | poids cible = `max(1, 2 * boardDiameter - distance + 1)` parmi les cibles atteignables du type retenu | meme logique de shortest-path que le spawn cible | meme flux infernal | retargeter vers une proie visible plausible |
| Gate du mouvement aleatoire en phase Searching | Bernoulli implemente par seuil uniforme | `p = 333 / 1000` | implementation concrete: `uniform_int_distribution(0,999)` puis comparaison au seuil; equivalent a une Bernoulli discrete a resolution 1/1000 | meme flux infernal | injecter de l'errance quand rien de mieux n'est visible |
| Choix du mouvement aleatoire | uniforme discrete | uniforme sur les coups legaux filtres | conditionne par la gate precedente et par le filtrage des coups qui captureraient une piece ennemie non visible | meme flux infernal | deplacer l'infernal pendant la recherche |
| Choix du bord de retour | uniforme discrete conditionnelle | uniforme parmi les cellules de bord qui minimisent la distance de retour | pas uniforme sur tout le bord: tie-break seulement entre meilleurs candidats | meme flux infernal | desengager l'infernal de maniere plausible quand il repart |

Notes infernal importantes:

- la decay de dette de sang (`95%`) est deterministe; elle n'est pas un processus aleatoire
- les plus courts chemins et la validation de mouvements sont deterministes; seul le choix parmi options eligibles est aleatoire
- `forceSpawnInfernal` saute la gate Poisson mais reutilise ensuite les memes tirages cibles/spawn que le chemin normal

## 4. Ce qui est aleatoire versus ce qui ne l'est pas

Pour eviter les faux positifs dans une lecture fonctionnelle:

- XP thresholds: deterministes
- vitesse meteo: deterministe dans l'etat actuel de config
- selection des composantes Dirt/Water apres calcul des scores: deterministe
- montant des bonus coffre movement/build: deterministe (`1` actuellement)
- decay de blood debt infernal: deterministe
- mouvement tactique infernal hors branches de tie-break aleatoires: deterministe via shortest-path et regles de mouvement

L'aleatoire runtime se situe donc surtout dans:

- la generation initiale de la carte
- la variabilite des recompenses XP/coffres
- la geometrie et la cadence des fronts meteo
- les gates d'apparition et de selection de cibles infernales

## 5. Persistences et continuite apres sauvegarde

La propriete la plus importante de cette codebase n'est pas seulement que les tirages sont seedes, mais qu'ils sont seedes de facon rejouable apres save/load.

| Systeme | Ce qui est sauvegarde | Ce que cela garantit |
| --- | --- | --- |
| Partie globale | `worldSeed` | toutes les derivations de seeds repartent de la meme racine |
| Carte | grille complete + `terrainFlipMask` + `terrainBrightness` | la carte et ses visuels n'ont pas besoin d'etre regenes pour rester identiques |
| XP | `xpState.rngCounter` | le prochain gain XP tirera exactement le meme profil que sans interruption |
| Coffres spawn | `chestState.rngCounter`, `nextSpawnTurn`, coffre actif | la cadence et la localisation futures restent identiques |
| Coffres reward | `rewardRngCounter` + progression courante partagee | le prochain loot et la logique de catch-up restent identiques |
| Meteo | `weatherState.rngCounter`, fronts actifs complets, `shapeSeed`, `densitySeed`, progression du front, masque | le brouillard visible et sa suite d'evolution restent identiques |
| Infernal | `infernalState.rngCounter`, dette, cooldown, infernal actif | la prochaine tentative de spawn et les tirages cibles associes restent identiques |

Sur le point demande explicitement par l'utilisateur: pour tous les mecanismes gameplay effectivement aleatoires actuellement actifs, la reponse est donc oui, ils se fondent sur le `worldSeed` de la game, directement ou via des seeds derives et des compteurs serialises.

## 6. Matrice exhaustive de synthese

| # | Systeme | Processus | Loi / famille | Seed path | Persistant |
| ---: | --- | --- | --- | --- | --- |
| 1 | Board | Seed Dirt | entier brut `mt19937` | `worldSeed -> mt19937 -> random()` | indirectement via grille finale |
| 2 | Board | Champ Dirt | value-noise / fBm | `worldSeed -> dirtNoiseSeed -> field(x,y)` | indirectement via grille finale |
| 3 | Board | Seed Water | entier brut `mt19937` | `worldSeed -> mt19937 -> random()` | indirectement via grille finale |
| 4 | Board | Champ Water | value-noise / fBm | `worldSeed -> waterNoiseSeed -> field(x,y)` | indirectement via grille finale |
| 5 | Board | Rotation mine/ferme | uniforme discrete | `worldSeed -> board mt19937` | indirectement via batiments/grille |
| 6 | Board | Flip mine/ferme | uniforme discrete | `worldSeed -> board mt19937` | indirectement via batiments/grille |
| 7 | Board | Ordre des placements publics | permutation uniforme | `worldSeed -> board mt19937` | indirectement via batiments/grille |
| 8 | Board | Choix position batiment public | uniforme conditionnelle | `worldSeed -> board mt19937` | indirectement via batiments/grille |
| 9 | Board | Spawn blanc | uniforme discrete | `worldSeed -> board mt19937` | indirectement via positions initiales |
| 10 | Board | Spawn noir | uniforme discrete | `worldSeed -> board mt19937` | indirectement via positions initiales |
| 11 | Board | Luminosite herbe | Beta transformee | `worldSeed -> local cell seed` | oui, valeur serialisee |
| 12 | Board | Flip terrain | pseudo-uniforme par hash | `worldSeed -> local cell hash` | oui, valeur serialisee |
| 13 | XP | Montant XP kill/block/arena | normale tronquee discretisee | `worldSeed + xpState.rngCounter` | oui |
| 14 | Chest | Delai de respawn | Weibull discretisee | `worldSeed + chestState.rngCounter` | oui |
| 15 | Chest | Cellule de spawn | categorielle ponderee | `worldSeed + chestState.rngCounter` | oui |
| 16 | Chest | Type de reward | categorielle ponderee | `worldSeed + rewardRngCounter` | oui |
| 17 | Chest | Montant or | normale tronquee discretisee | `worldSeed + rewardRngCounter` | oui |
| 18 | Weather | Delai entre fronts | Gamma discretisee | `worldSeed + weatherState.rngCounter` | oui |
| 19 | Weather | Direction | categorielle ponderee | `worldSeed + weatherState.rngCounter` | oui |
| 20 | Weather | Bord diagonal | uniforme discrete | `worldSeed + weatherState.rngCounter` | oui |
| 21 | Weather | Position d'entree | piecewise linear | `worldSeed + weatherState.rngCounter` | oui |
| 22 | Weather | Couverture cible | uniforme discrete | `worldSeed + weatherState.rngCounter` | oui |
| 23 | Weather | Aspect ratio | uniforme discrete | `worldSeed + weatherState.rngCounter` | oui |
| 24 | Weather | Duree visible cible | Gamma discretisee | `worldSeed + weatherState.rngCounter` | oui |
| 25 | Weather | `shapeSeed` | entier brut | `worldSeed + weatherState.rngCounter` | oui |
| 26 | Weather | `densitySeed` | entier brut | `worldSeed + weatherState.rngCounter` | oui |
| 27 | Weather | Bruit de contour | value-noise spatial | `shapeSeed -> field(x,y)` | oui via front serialise |
| 28 | Weather | Densite locale | log-normale par cellule | `densitySeed -> local cell generator` | oui via front serialise |
| 29 | Infernal | Gate de spawn | Poisson | `worldSeed + infernalState.rngCounter` | oui |
| 30 | Infernal | Royaume cible | Bernoulli | `worldSeed + infernalState.rngCounter` | oui |
| 31 | Infernal | Type cible primaire | categorielle ponderee | `worldSeed + infernalState.rngCounter` | oui |
| 32 | Infernal | Option de spawn ciblee | categorielle ponderee | `worldSeed + infernalState.rngCounter` | oui |
| 33 | Infernal | Spawn frontiere fallback | uniforme discrete | `worldSeed + infernalState.rngCounter` | oui |
| 34 | Infernal | Type de remplacement | categorielle ponderee biaisee | `worldSeed + infernalState.rngCounter` | oui |
| 35 | Infernal | Cible de remplacement | categorielle ponderee | `worldSeed + infernalState.rngCounter` | oui |
| 36 | Infernal | Gate random move searching | Bernoulli via seuil uniforme | `worldSeed + infernalState.rngCounter` | oui |
| 37 | Infernal | Choix du random move | uniforme discrete | `worldSeed + infernalState.rngCounter` | oui |
| 38 | Infernal | Tie-break retour bord | uniforme discrete conditionnelle | `worldSeed + infernalState.rngCounter` | oui |

## 7. Conclusion operationnelle

Le runtime gameplay actuel ne contient pas de mecanisme aleatoire autonome qui contourne le `worldSeed` de la partie. Les variations gameplay observees proviennent de cinq familles principales:

- generation procedurale de carte
- recompenses XP
- spawns et rewards des coffres
- fronts meteo
- systeme infernal

La robustesse de la reproductibilite ne repose pas seulement sur le seed, mais sur la combinaison suivante:

- `worldSeed` stable
- compteurs RNG serialises par systeme
- serialization des etats intermediaires qui portent eux-memes des seeds derives, en particulier les fronts meteo et la progression de loot des coffres

Sous ce perimetre gameplay runtime, l'audit est exhaustif.