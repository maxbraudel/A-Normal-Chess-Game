const L = String.raw;

import { processIllustrationsByTitle } from "./randomnessIllustrations.js";

function withProcessIllustration(process) {
  const theory = processTheoryByTitle[process.title];
  const enrichedProcess = theory ? { ...process, theory } : process;
  const illustration = processIllustrationsByTitle[enrichedProcess.title];
  return illustration ? { ...enrichedProcess, illustration } : enrichedProcess;
}

const uniformProcesses = [
  {
    title: "Graine globale de la terre",
    system: "Carte",
    lawUse: "Uniforme discrète sur un espace de 32 bits",
    variable: L`S_{terre} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Produit la graine intermédiaire qui alimente ensuite le champ procédural de la terre.",
    parameters: [
      "support de taille 2^32",
      "1 tirage au début de la génération du plateau"
    ],
    why:
      "Une graine intermédiaire n'a aucune direction stratégique a privilégier; on cherche seulement une répartition uniforme des mondes possibles.",
    simulation:
      "Le générateur `std::mt19937(worldSeed)` est interrogé une fois, puis la valeur tirée nourrit `valueNoise` et `fractalNoise`.",
    parameterChoice:
      "Le format 32 bits est exactement celui des sorties natives de `mt19937`, donc pas de biais de conversion supplémentaire.",
    dependence:
      "Dépend complètement de `worldSeed`; aucun tirage en cours de partie."
  },
  {
    title: "Graine globale de l'eau",
    system: "Carte",
    lawUse: "Uniforme discrète sur un espace de 32 bits",
    variable: L`S_{eau} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Produit la graine auxiliaire du champ des lacs et poches d'eau.",
    parameters: [
      "support de taille 2^32",
      "1 tirage au début de la génération du plateau"
    ],
    why:
      "Le système d'eau doit être decorrélé de la terre tout en restant reproductible à seed fixe.",
    simulation:
      "Une sortie brute du générateur initialise la branche d'eau avant évaluation des champs spatiaux.",
    parameterChoice:
      "Le grand support évite des répétitions perceptibles lorsque plusieurs graines auxiliaires sont dérivées du même monde.",
    dependence:
      "Couplé à `worldSeed`, mais distinct de `S_{terre}` par l'ordre d'appel du générateur."
  },
  {
    title: "Rotation des mines et fermes neutres",
    system: "Carte",
    lawUse: "Uniforme discrète sur quatre quarts de tour",
    variable: L`R \in \{0,1,2,3\}`,
    phenomenon:
      "Choisit l'orientation sprite des bâtiments publics déjà placés afin d'éviter un rendu trop mécanique.",
    parameters: ["support = {0, 1, 2, 3}"],
    why:
      "Les quatre rotations sont géométriquement symétriques pour ces assets; aucune ne doit être favorisée.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` pendant la génération du plateau.",
    parameterChoice:
      "Quatre états correspondent exactement aux orientations de 90 degrés disponibles.",
    dependence:
      "Dépend du flux de génération initial, mais plus du tout après serialisation de la carte."
  },
  {
    title: "Retournement des mines et fermes neutres",
    system: "Carte",
    lawUse: "Uniforme discrète sur les masques de symetrie",
    variable: L`F \in \{0,1,2,3\}`,
    phenomenon:
      "Active ou non le retournement horizontal et vertical des bâtiments publics.",
    parameters: ["0 = aucun retournement", "1 = horizontal", "2 = vertical", "3 = double retournement"],
    why:
      "Les masques de symétrie disponibles sont équiprobables dès lors qu'on ne veut pas marquer de biais visuel.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` et transmet le masque à la footprint du bâtiment.",
    parameterChoice:
      "La cardinalite 4 vient de `kFlipHorizontalMask | kFlipVerticalMask`.",
    dependence:
      "Statiquement derive de la génération de carte."
  },
  {
    title: "Choix de position des bâtiments publics",
    system: "Carte",
    lawUse: "Uniforme conditionnelle sur le haut du classement de dispersion",
    variable: L`X \mid X \in A_{top}`,
    phenomenon:
      "Sélectionne l'origine d'une mine ou d'une ferme parmi les meilleurs candidats au regard de la dispersion spatiale.",
    parameters: [
      L`|A_{top}| = \min\left(n, \max\left(3, \left\lceil \frac{n}{6} \right\rceil\right)\right)`,
      "les candidats sont notés puis triés par score de distance"
    ],
    why:
      "Un pur optimum rendrait la carte trop déterministe; un tirage uniforme dans le top conserve la qualité géométrique sans figer la même configuration.",
    simulation:
      "`selectDispersedCandidate` trie les candidats, calcule `topCount`, puis tire uniformément un index dans ce sous-ensemble.",
    parameterChoice:
      "Le minimum de 3 laisse toujours un peu de variété même quand l'ensemble admissible est petit.",
    dependence:
      "Conditionne par les placements déjà retenus, donc fortement dépendante de l'historique de génération."
  },
  {
    title: "Apparition des royaumes",
    system: "Carte",
    lawUse: "Uniforme discrète sur les zones de départ des royaumes",
    variable: L`P_K \sim \mathcal{U}_d(A_K),\; K \in \{W,B\}`,
    phenomenon:
      "Choisit les cellules de départ des rois blanc et noir dans leurs bandes de départ respectives, avec les mêmes contraintes de terrain et de séparation.",
    parameters: [
      "zone d'apparition = 25 % du plateau sur chaque côté",
      "admissibilité géométrique, terrain non bloqué et séparation stratégique initiale"
    ],
    why:
      "Les deux royaumes obéissent à la même logique de tirage conditionnel; la bonne lecture est donc une uniforme discrète sur deux supports latéraux symétriques, pas deux mécanismes différents.",
    simulation:
      "Le générateur collecte les cellules valides de chaque bande latérale, puis tire un index uniforme dans le vecteur de candidats du royaume concerné.",
    parameterChoice:
      "Le pourcentage 25 % vient de `player_spawn_zone_percent` et `ai_spawn_zone_percent`, gardes égaux pour ne pas introduire d'avantage structurel.",
    dependence:
      "Dépend du terrain déjà généré, donc du couple `worldSeed` + champs procéduraux, et de la contrainte de séparation entre royaumes."
  },
  {
    title: "Bord diagonal d'entrée du brouillard",
    system: "Météo",
    lawUse: "Uniforme discrète sur les deux bords compatibles avec la diagonale",
    variable: L`E \in \{e_1,e_2\}`,
    phenomenon:
      "Pour les directions diagonales, choisit lequel des deux bords du plateau sert de bord d'entrée effectif.",
    parameters: ["2 bords admissibles par direction diagonale"],
    why:
      "Les deux constructions géométriques possibles sont symétriques; une loi uniforme conserve cette symetrie.",
    simulation:
      "La fonction `randomElement` appelle `std::uniform_int_distribution<int>(0, 1)`.",
    parameterChoice:
      "Deux états seulement car une diagonale entre toujours soit par un bord soit par l'autre côté compatible.",
    dependence:
      "Dépend de la direction du brouillard, elle-même tirée juste avant."
  },
  {
    title: "Couverture cible du brouillard",
    system: "Météo",
    lawUse: "Uniforme continue sur un intervalle de pourcentage",
    variable: L`C \sim \mathcal{U}([0.05, 0.20])`,
    phenomenon:
      "Fixe la proportion de cellules visibles que le nouveau brouillard doit recouvrir a sa naissance.",
    parameters: ["`coverage_min_percent = 5`", "`coverage_max_percent = 20`"],
    why:
      "Aucune taille privilégiée n'est imposee entre les bornes retenues; l'uniforme donne un eventail large mais lisible.",
    simulation:
      "Le runtime tire un entier uniforme entre 5 et 20, puis convertit ce pourcentage en aire cible.",
    parameterChoice:
      "La borné basse garde des brouillards non triviaux; la borné haute evite une occultation presque totale du plateau.",
    dependence:
      "Se combine ensuite avec l'aspect ratio et la direction pour construire la géométrie finale."
  },
  {
    title: "Allongement du brouillard",
    system: "Météo",
    lawUse: "Uniforme continue sur un intervalle borné",
    variable: L`A \sim \mathcal{U}([1.80, 2.60])`,
    phenomenon:
      "Fixe l'allongement principal du brouillard avant déformation par le bruit de contour.",
    parameters: ["`aspect_ratio_min_times_100 = 180`", "`aspect_ratio_max_times_100 = 260`"],
    why:
      "Le brouillard doit rester anisotrope sans toujours avoir la même excentricite; une plage uniforme contrôle cette variété.",
    simulation:
      "Le code tire un entier uniforme sur [180, 260], divise par 100, puis derive `radiusAlong` et `radiusAcross` a aire préservée.",
    parameterChoice:
      "Des ratios entre 1.8 et 2.6 donnent des bandes visibles sans devenir quasi linéaire.",
    dependence:
      "Partage la même seed d'événement que la couverture et les graines de contour/densité du brouillard courant."
  },
  {
    title: "Graine de forme du brouillard",
    system: "Météo",
    lawUse: "Uniforme discrète sur 32 bits",
    variable: L`S_{shape} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Fournit la graine du bruit qui perturbe le bord du brouillard.",
    parameters: ["1 tirage `generator()` par brouillard"],
    why:
      "Le brouillard doit posséder une signature spatiale propre sans collision visuelle trop fréquente.",
    simulation:
      "Une sortie brute du `mt19937` de l'événement est recopiée dans le descripteur du brouillard.",
    parameterChoice:
      "Le support de 32 bits est suffisant pour différencier des milliers de brouillards sans répétition perceptible.",
    dependence:
      "Dépend du même générateur d'événement que la direction, l'aire et l'allongement du brouillard."
  },
  {
    title: "Graine de densité du brouillard",
    system: "Météo",
    lawUse: "Uniforme discrète sur 32 bits",
    variable: L`S_{densité} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Fournit la graine qui module localement l'opacité du brouillard via une loi log-normale.",
    parameters: ["1 tirage `generator()` par brouillard"],
    why:
      "La texture d'opacité doit être reproductible mais différente du contour; il faut donc une graine propre.",
    simulation:
      "Le `mt19937` du brouillard produit une seconde sortie brute stockée dans le descripteur.",
    parameterChoice:
      "La séparation entre la graine de forme et la graine de densité évite de coupler rigidement contour et densité locale.",
    dependence:
      "Couplée au même événement d'apparition que la graine de forme, elle reste exploitée dans une chaîne de hachage distincte par cellule."
  },
  {
    title: "Apparition de secours sur la frontière pour une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Uniforme discrète sur les cases de bord encore admissibles",
    variable: L`B \sim \mathcal{U}_d(A_{bord})`,
    phenomenon:
      "Quand aucune apparition ciblée n'est valide, choisit une case de frontière parmi celles encore autorisées.",
    parameters: ["ensemble conditionne par le type de pièce, le relief et l'occupation"],
    why:
      "En situation de repli, toutes les issues de bord restantes jouent le même rôle logique.",
    simulation:
      "Le système construit la liste `candidates`, puis appelle `std::uniform_int_distribution<std::size_t>` sur sa taille.",
    parameterChoice:
      "La politique de repli minimale reduit les heuristiques supplémentaires quand la cible principale est impossible.",
    dependence:
      "Dépend fortement du plateau courant, des pièces visibles et du type manifeste de la pièce du diable."
  },
  {
    title: "Choix d'un mouvement aléatoire en phase Searching",
    system: "Pièces du diable",
    lawUse: "Uniforme discrète sur les coups admissibles",
    variable: L`M \sim \mathcal{U}_d(A_{moves})`,
    phenomenon:
      "Sélectionne un coup quand la pièce du diable entre dans sa **phase de recherche** (`Searching`), c'est-à-dire le moment où elle explore les coups encore possibles au lieu de poursuivre une cible déjà fixée.",
    parameters: ["support = coups générés, puis filtres par la visibilité locale et les collisions interdites"],
    why:
      "Une fois le mode aléatoire activé, aucun coup restant n'est prioritaire dans cette branche spécifique du comportement.",
    simulation:
      "Le code génère tous les coups, filtre ceux qui violent la logique de visibilité, puis tire un index uniforme.",
    parameterChoice:
      "L'uniformité limite l'introduction d'une seconde heuristique dans un chemin déjà déclenché de façon probabiliste.",
    dependence:
      "Conditionné par le tirage bernoulli d'entrée dans cette branche et par l'ensemble de coups restants."
  },
  {
    title: "Tie-break de retour vers le bord",
    system: "Pièces du diable",
    lawUse: "Uniforme discrète sur les sorties équivalentes",
    variable: L`R_{edge} \sim \mathcal{U}_d(A_{eq})`,
    phenomenon:
      "Départage plusieurs trajectoires de repli équivalentes quand la pièce du diable veut revenir vers un bord.",
    parameters: ["support = directions a même cout de chemin"],
    why:
      "Les options de même coût ne doivent pas être ordonnées arbitrairement par l'ordre de parcours du code.",
    simulation:
      "Le tie-break se fait par tirage uniforme sur le sous-ensemble des directions ex aequo.",
    parameterChoice:
      "Un tirage conditionnel est suffisant car seules les directions de cout minimal entrent dans le support.",
    dependence:
      "Dépend d'une analyse de plus court chemin préalable, donc du terrain et des obstacles courants."
  }
];

const permutationProcesses = [
  {
    title: "Ordre de placement des mines et fermes neutres",
    system: "Carte",
    lawUse: "Permutation uniforme sur les objets a placer",
    variable: L`\Pi \in \mathfrak{S}_n`,
    phenomenon:
      "Mélange l'ordre dans lequel les fermes et les mines sont insérées sur la carte avant choix de position.",
    parameters: ["`n = num_mines + num_farms = 5` dans la config actuelle"],
    why:
      "Une permutation uniforme supprime un biais systématique du type 'les mines sont toujours placées avant les fermes'.",
    simulation:
      "Le générateur appelle `std::shuffle(placements.begin(), placements.end(), random)` avant la boucle de placement.",
    parameterChoice:
      "La taille `n = 5` vient directement de `num_mines = 2` et `num_farms = 3`.",
    dependence:
      "Influe indirectement sur tout le reste du placement public car les meilleurs candidats changent après chaque insertion."
  }
];

const categoricalProcesses = [
  {
    title: "Case d'apparition d'un coffre",
    system: "Coffres",
    lawUse: "Catégorielle pondérée sur les cellules admissibles",
    variable: L`C \in \{c_1,\dots,c_n\}`,
    phenomenon:
      "Choisit la cellule de réapparition d'un coffre parmi les cases libres et suffisamment éloignées des deux rois.",
    parameters: [
      L`w(c)=1+\mathrm{centrality}(c)+\mathrm{contestation}(c)`,
      "`min_distance_from_kings = 6`",
      "centralite mesuree par la distance au centre, contestation par l'équilibre des distances aux deux rois"
    ],
    why:
      "Le système veut privilégier les zones contestables et centrales, pas seulement tirer une case uniforme.",
    simulation:
      "Le runtime calcule un poids entier pour chaque candidat puis utilise `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Le poids `1 + centrality + contestation` garantit une probabilité strictement positive pour toute case admissible.",
    dependence:
      "Dépend de la position instantanee des rois et de l'occupation du plateau au moment de l'apparition."
  },
  {
    title: "Type de récompense du coffre",
    system: "Coffres",
    lawUse: "Catégorielle pondérée à deux régimes temporels",
    variable: L`R \in \{\text{gold},\text{move},\text{build}\}`,
    phenomenon:
      "Choisit si le coffre donne de l'or, un bonus permanent du **budget de mouvement par tour** ou un bonus permanent du **budget de construction par tour**.",
    parameters: [
      "début de partie: poids (8, 3, 3)",
      "fin de partie: poids (4, 6, 6)",
      "**Bascule à partir du tour 10** (`late_game_turn = 10`)",
      "Mode de rattrapage actif (`current_loot_catch_up_enabled = true`) : **les deux royaumes partagent la même récompense courante** tant qu'ils ne l'ont pas tous les deux recueillie"
    ],
    why:
      "Le système favorise **plus d'or très tôt**, puis **davantage de capacité d'action** ensuite; une catégorielle pondérée est la loi naturelle pour ce genre de choix nominal.",
    simulation:
      "`sampleReward` construit le vecteur de poids selon le tour courant, puis appelle `std::discrete_distribution<int>`.",
    parameterChoice:
      "**À partir du tour 10**, les bonus d'action prennent plus de poids afin d'accélérer le milieu de partie.",
    dependence:
      "Si le mode de rattrapage est actif, **le tirage suivant n'apparaît que lorsque les deux royaumes ont déjà pris la récompense courante**; l'état `currentRewardGeneration` lie donc directement les ouvertures de coffres des deux camps."
  },
  {
    title: "Direction du brouillard",
    system: "Météo",
    lawUse: "Catégorielle pondérée sur huit directions",
    variable: L`D \in \{N,S,E,W,NE,NW,SE,SW\}`,
    phenomenon:
      "Choisit la direction cardinale ou diagonale du prochain brouillard.",
    parameters: ["les huit poids valent actuellement 1."],
    why:
      "Le système est écrit de façon générique pour pouvoir biaiser certaines directions plus tard, mais la configuration active réalise une équiprobabilité via une catégorielle à poids égaux.",
    simulation:
      "Le runtime lit `direction_weights`, puis passe le tableau d'entiers a `std::discrete_distribution<int>`.",
    parameterChoice:
      "Les huit poids unitaires font de cette catégorielle une uniforme deguisement, tout en gardant un point d'extension clair.",
    dependence:
      "La direction pilote ensuite le bord d'entrée, la trajectoire et les rayons du brouillard."
  },
  {
    title: "Type de cible primaire d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée sur les types de pièces visibles",
    variable: L`T \in \{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    phenomenon:
      "Choisit quel type de pièce ennemie la pièce du diable va chercher en priorité.",
    parameters: [
      "poids actifs: pawn 8, knight 14, bishop 14, rook 26, queen 38",
      "les types absents du champ visible recoivent le poids 0"
    ],
    why:
      "Le comportement cherche une priorisation stratégique explicite: les pièces majeures doivent être attirées plus souvent que les pions.",
    simulation:
      "Le système construit `typeWeights`, met à zéro les types indisponibles, puis tire via `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Les poids croissants codent un ordre de valeur tactique sans rendre le choix déterministe.",
    dependence:
      "Dépend de la visibilité courante et des types réellement présents chez le royaume cible."
  },
  {
    title: "Option d'apparition ciblée d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée par proximité de chemin",
    variable: L`O \in \{o_1,\dots,o_m\}`,
    phenomenon:
      "Choisit, parmi plusieurs options de projection vers une cible visible, celle qui sera retenue au moment de l'apparition.",
    parameters: [
      L`w(o)=\max\bigl(1, 2D-\mathrm{dist}(o)+1\bigr)`,
      "`D = board.getDiameter()`"
    ],
    why:
      "On veut favoriser les options qui approchent vite la cible sans pour autant annuler complètement les autres entrées valides.",
    simulation:
      "Le runtime calcule un poids entier inversement lié à la distance de plus court chemin, puis tire avec `std::discrete_distribution`.",
    parameterChoice:
      "Le plancher à 1 conserve une queue de probabilité sur toute option encore jouable.",
    dependence:
      "Conditionné par le type de pièce infernale manifestée et par le graphe de déplacements accessible."
  },
  {
    title: "Type de remplacement d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée avec bonus de persistance",
    variable: L`T \in \{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    phenomenon:
      "Quand la cible initiale devient impossible, choisit un nouveau type de cible visible.",
    parameters: [
      "même base de poids (8, 14, 14, 26, 38)",
      "si le type correspond à `preferredTargetType`, son poids est doublé"
    ],
    why:
      "Le remplacement doit rester cohérent avec la priorité précédente tout en laissant une vraie possibilité de redirection.",
    simulation:
      "La boucle de remplacement reconstruit les poids restants puis applique `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Le doublement du poids mémorise une inertie comportementale simple à expliquer et facile à régler.",
    dependence:
      "Dépend du type précédemment poursuivi et des types encore visibles."
  },
  {
    title: "Cible de remplacement d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée parmi les cibles atteignables du type retenu",
    variable: L`Y \in \{y_1,\dots,y_r\}`,
    phenomenon:
      "Choisit la cible concrète une fois le type de remplacement fixe.",
    parameters: [
      L`w(y)=\max\bigl(1, 2D-\mathrm{dist}(y)+1\bigr)`,
      "seules les cibles atteignables sont conservées"
    ],
    why:
      "La priorité reste donnée aux cibles rapides d'accès, mais le tirage conserve de la diversité tactique.",
    simulation:
      "Le code reconstruit `reachableTargets` et `reachableWeights`, puis échantillonne une cible par `std::discrete_distribution`.",
    parameterChoice:
      "Le même schéma de poids que pour les options d'apparition maintient une logique unique de proximité pour les pièces du diable.",
    dependence:
      "Fortement couplé à l'état du plateau, au type retenu juste avant et au masque de visibilité météo."
  }
];

const bernoulliProcesses = [
  {
    title: "Royaume cible d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Bernoulli à probabilité d'état",
    variable: L`K \sim \mathrm{Bernoulli}(p_t)`,
    phenomenon:
      "Choisit si la pièce du diable cible le royaume blanc ou noir quand les deux sont éligibles.",
    parameters: [
      L`p_t = \frac{\mathrm{debt}_{white}}{\mathrm{debt}_{white}+\mathrm{debt}_{black}}`,
      L`p_t = 0.5 \text{ si la dette totale vaut } 0`
    ],
    why:
      "Une Bernoulli suffit dès qu'il n'y a que deux royaumes éligibles; l'état du jeu déforme directement la probabilité.",
    simulation:
      "Le code calcule `whiteProbability`, la borne dans `[0,1]`, puis appelle `std::bernoulli_distribution`.",
    parameterChoice:
      "La probabilité proportionnelle à la dette de sang rend le système réactif aux pertes infligées à chaque camp.",
    dependence:
      "Très dépendante de l'historique des destructions, donc non stationnaire sur une partie."
  },
  {
    title: "Activation d'un mouvement aléatoire en phase Searching",
    system: "Pièces du diable",
    lawUse: "Bernoulli simple",
    variable: L`B \sim \mathrm{Bernoulli}(0.333)`,
    phenomenon:
      "Décide si, lors d'un tour de **phase de recherche** (`Searching`), la pièce du diable tente effectivement un mouvement purement aléatoire plutôt qu'un déplacement entièrement piloté par ses heuristiques.",
    parameters: ["probabilité de 33,3 % (`searching_random_move_chance_times_1000 = 333`)"],
    why:
      "Il s'agit d'un interrupteur oui/non sur une branche comportementale unique; la Bernoulli est la loi minimale adéquate.",
    simulation:
      "Le système tire un entier uniforme sur `[0, 999]` et compare au seuil 333, ce qui réalise une Bernoulli discrétisée.",
    parameterChoice:
      "Une probabilité proche de 1/3 maintient de la pression sans rendre la branche erratique dominante.",
    dependence:
      "Conditionne par l'existence d'au moins un coup admissible après filtrage."
  }
];

const poissonProcesses = [
  {
    title: "Déclenchement d'apparition d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Poisson observée via l'événement {N >= 1}",
    variable: L`N \sim \mathrm{Poisson}(\lambda_t)`,
    phenomenon:
      "Détermine si une nouvelle apparition d'une pièce du diable se déclenche à ce tour.",
    parameters: [
      L`\lambda_t = \min(0.25, 0.02 + 0.012\,\mathrm{debt}_t)`,
      "`poisson_lambda_base_times_1000 = 20`",
      "`poisson_lambda_per_debt_times_1000 = 12`",
      "`poisson_lambda_cap_times_1000 = 250`"
    ],
    why:
      "La conception veut un compteur d'arrivées rares dont l'intensité croît avec la dette de sang; la Poisson est le modèle naturel de comptage d'événements.",
    simulation:
      "Le runtime échantillonne `std::poisson_distribution<int>(lambda)` puis déclenche l'apparition si le résultat est au moins 1.",
    parameterChoice:
      "Le cap à 0.25 borne `P(N \\ge 1) = 1 - e^{-\\lambda}` en dessous de 0.221, donc les pièces du diable restent menaçantes sans saturer la partie.",
    dependence:
      "`\\lambda_t` dépend de la dette agrégée, elle-même mise à jour à chaque perte ou dégât structurel."
  }
];

const truncatedNormalProcesses = [
  {
    title: "Récompenses d'XP",
    system: "XP",
    lawUse: "Normale tronquée puis arrondie à l'entier",
    variable: L`Y = \max\bigl(m, \mathrm{round}(\mathrm{clip}(X,[a,b]))\bigr)`,
    phenomenon:
      "Attribue l'XP pour les kills, la destruction de bloc et le gain passif d'arene.",
    parameters: [
      "kill_pawn: mean 20, sigma = 3.6, clamp = +/- 7.2, minimum = 1",
      "kill_knight / kill_bishop: mean 50, sigma = 8, clamp = +/- 16, minimum = 1",
      "kill_rook: mean 100, sigma = 12, clamp = +/- 24, minimum = 1",
      "kill_queen: mean 300, sigma = 30, clamp = +/- 60, minimum = 1",
      "destroy_block / arena_per_turn: mean 10, sigma = 1.5, clamp = +/- 3, minimum = 1"
    ],
    why:
      "Une variable gaussienne tronquée préserve une moyenne intuitive tout en autorisant une dispersion contrôlée autour de chaque source d'XP.",
    simulation:
      "`RewardProfileSampling::sampleTruncatedNormal` tire une normale, la tronque sur `[mean - delta, mean + delta]`, puis arrondit et applique le minimum.",
    parameterChoice:
      "Le sigma est proportionnel à la moyenne via `sigma_multiplier_times_100`, ce qui garde une volatilité relative comparable entre petites et grosses récompenses.",
    dependence:
      "Les tirages sont indépendants conditionnellement au profil choisi, mais le profil dépend du type d'événement de jeu."
  },
  {
    title: "Montant d'or d'un coffre",
    system: "Coffres",
    lawUse: "Normale tronquée puis arrondie à l'entier",
    variable: L`G = \max\bigl(1, \mathrm{round}(\mathrm{clip}(X,[\mu-2\sigma,\mu+2\sigma]))\bigr)`,
    phenomenon:
      "Quand la récompense choisie est de l'or, fixe le montant réel donné au joueur.",
    parameters: [
      "`mean = 35`",
      "`sigma_multiplier_times_100 = 18`, donc sigma = 6.3",
      "`clamp_sigma_multiplier_times_100 = 200`, donc troncature à +/- 12.6",
      "`minimum = 1`"
    ],
    why:
      "La même famille que pour l'XP permet d'obtenir une valeur centrale stable, avec un peu de volatilité sans valeurs aberrantes gigantesques.",
    simulation:
      "Le chemin `sampleGoldRewardAmount -> sampleTruncatedNormal` réutilisé exactement le moteur commun de profils de récompense.",
    parameterChoice:
      "La moyenne 35 est cohérente avec l'économie initiale et reste bien au-dessus du minimum même après troncature.",
    dependence:
      "Conditionne par le fait que la catégorielle de type de récompense ait déjà choisi la branche or."
  }
];

const weibullProcesses = [
  {
    title: "Délai de réapparition d'un coffre",
    system: "Coffres",
    lawUse: "Weibull discrétisée et tronquée inférieurement",
    variable: L`D = \max\bigl(c, \mathrm{round}(T)\bigr),\quad T \sim \mathrm{Weibull}(k,\lambda)`,
    phenomenon:
      "Fixe le nombre de tours à attendre avant le prochain coffre.",
    parameters: [
      "`k = 1.80` via `weibull_shape_times_100 = 180`",
      "`lambda = 6` tours via `weibull_scale_turns = 6`",
      "cooldown plancher `c = 4` tours",
      "tour minimal de tout premier apparition = 4"
    ],
    why:
      "La Weibull est adaptée aux temps d'attente flexibles: avec `k > 1`, le hazard croissant rend les réapparitions plus plausibles après plusieurs tours sans coffre.",
    simulation:
      "`sampleSpawnDelay` échantillonne `std::weibull_distribution<double>(shape, scale)`, arrondit, puis applique `max(respawnCooldown, randomDelay)`.",
    parameterChoice:
      "`k = 1.8` garde des délais variables tout en évitant une concentration trop forte près de zéro.",
    dependence:
      "Le délai est resamplé à chaque collecte ou échec d'apparition, mais la logique de placement peut encore reporter l'apparition."
  }
];

const gammaProcesses = [
  {
    title: "Délai entre deux brouillards",
    system: "Météo",
    lawUse: "Gamma discrétisée par plafond",
    variable: L`D = m + \lceil T \rceil,\quad T \sim \Gamma(k,\theta)`,
    phenomenon:
      "Fixe le nombre de tours avant le prochain essai d'apparition d'un brouillard.",
    parameters: [
      "config active: `k = 4.00`, `theta = 10.00`, minimum `m = 0`",
      "par héritage code, la version par défaut était `k = 3.20`, `theta = 2.40`"
    ],
    why:
      "Une Gamma contrôle naturellement des temps d'attente positifs et asymétriques, plus souples qu'une exponentielle simple.",
    simulation:
      "`scheduleNextSpawn` appelle `sampleGammaTurns`, qui échantillonne `std::gamma_distribution`, prend le plafond puis convertit en pas de temps.",
    parameterChoice:
      "Le passage par la config permet de rallonger ou compresser très simplement la cadence globale des brouillards sans toucher au code.",
    dependence:
      "La tentative suivante reste aussi bloquée tant qu'un brouillard actif occupe déjà la carte."
  },
  {
    title: "Durée visible d'un brouillard",
    system: "Météo",
    lawUse: "Gamma discrétisée par plafond",
    variable: L`V = \max(1, \lceil T \rceil),\quad T \sim \Gamma(k,\theta)`,
    phenomenon:
      "Fixe le nombre de tours pendant lesquels le brouillard doit rester sensiblement visible avant de quitter la carte.",
    parameters: [
      "`k = 2.60` via `duration_gamma_shape_times_100 = 260`",
      "`theta = 1.80` via `duration_gamma_scale_times_100 = 180`",
      "minimum logique appliqué ensuite: au moins 1 tour visible"
    ],
    why:
      "Une durée positive et asymétrique est mieux modélisée par une Gamma que par une loi symétrique, surtout pour éviter des vies négatives ou quasi nulles.",
    simulation:
      "Le runtime tire `visibleTurnCount`, convertit en nombre de pas, puis adapte le trajet et l'élongation du brouillard pour respecter cette cible temporelle.",
    parameterChoice:
      "La moyenne continue `k\\theta = 4.68` tours donne des brouillards visibles mais pas permanents.",
    dependence:
      "La durée interagit ensuite avec la vitesse, l'aire préservée et la géométrie du plateau."
  }
];

const logNormalProcesses = [
  {
    title: "Densité locale d'un brouillard",
    system: "Météo",
    lawUse: "Log-normale cellule par cellule, puis clamp d'alpha",
    variable: L`X(c) \sim \mathrm{LogNormal}(\mu,\sigma^2)`,
    phenomenon:
      "Multiplie l'opacité locale du brouillard pour obtenir des zones plus ou moins opaques à l'intérieur d'une même masse nuageuse.",
    parameters: [
      "`mu = -0.12` via `density_mu_times_100 = -12`",
      "`sigma = 0.35` via `density_sigma_times_100 = 35`",
      "alpha local = clamp(0.48 * X(c), 0.22, 0.82)"
    ],
    why:
      "La log-normale garantit des multiplicateurs strictement positifs, avec une queue à droite utile pour créer quelques poches très opaques sans valeurs négatives.",
    simulation:
      "`sampleLogNormalCell` redérive un générateur par cellule à partir de `densitySeed`, puis échantillonne `std::lognormal_distribution<double>(mu, sigma)`.",
    parameterChoice:
      "La moyenne géométrique légèrement sous 1 et un sigma modéré donnent surtout des variations fines, ensuite bornées par l'alpha min/max.",
    dependence:
      "Toutes les cellules d'un même brouillard partagent la même graine de densité; le champ n'est donc pas i.i.d. (**indépendant et identiquement distribué**) à l'échelle du brouillard."
  }
];

const betaProcesses = [
  {
    title: "Luminosité de l'herbe",
    system: "Carte",
    lawUse: "Beta transformée par seuil et contraste",
    variable: L`B \sim \mathrm{Beta}(7,2)`,
    phenomenon:
      "Module la luminosité des cellules d'herbe pour casser l'uniformité du tapis vert sans toucher aux autres terrains.",
    parameters: [
      "`alpha = 7`, `beta = 2`",
      "`keep_default_threshold = 0.90`",
      "`min_brightness = 0.68`",
      "`contrast_exponent = 1.8`"
    ],
    why:
      "Avec `alpha > beta`, la Beta concentre la masse près de 1, ce qui laisse la plupart des herbes proches de la luminosité nominale tout en autorisant quelques assombrissements visibles.",
    simulation:
      "Le code échantillonne la Beta via deux Gammas, applique un seuil à 0.90, puis remappe la partie basse vers `[0.68, 1]` avec une puissance 1.8.",
    parameterChoice:
      "Le couple (7, 2) et le seuil 0.90 donnent un plateau majoritairement clair, avec juste assez d'irrégularité pour casser la répétition.",
    dependence:
      "Chaque cellule dérive son seed d'un hachage de `worldSeed` et de sa position, donc le rendu est fixe pour un monde donné."
  }
];

const piecewiseLinearProcesses = [
  {
    title: "Position d'entrée le long du bord d'un brouillard",
    system: "Météo",
    lawUse: "Linéaire par morceaux sur la coordonnée de bord",
    variable: L`X \in [0,M]`,
    phenomenon:
      "Choisit la position continue du centre du brouillard le long du bord d'entrée.",
    parameters: [
      L`\text{noeuds } (0, \tfrac14 M, \tfrac12 M, \tfrac34 M, M)`,
      "poids `(0.7, 1.8, 1.98, 1.8, 0.7)` dans la config active",
      "avec `1.98 = 1.1 * 1.8` pour le point median"
    ],
    why:
      "Le jeu veut privilégier les entrées centrales tout en conservant une probabilité non nulle de départ par les coins; la linéaire par morceaux est idéale pour cette densité dessinée à la main.",
    simulation:
      "`sampleEdgePosition` construit les bornes et les hauteurs puis utilise `std::piecewise_linear_distribution<double>`.",
    parameterChoice:
      "Le centre est volontairement surpondéré par rapport aux quarts et aux coins pour produire des brouillards plus lisibles visuellement.",
    dependence:
      "Dépend ensuite du bord retenu et de la direction du brouillard pour se convertir en coordonnées 2D."
  }
];

const proceduralProcesses = [
  {
    title: "Champ spatial de la terre",
    system: "Carte",
    lawUse: "Champ procédural corrélé dérivé de bruit",
    variable: L`X_{terre}(c) = g_{S_{terre}}(c)`,
    phenomenon:
      "Produit des zones de terre connexes plutôt que des cellules i.i.d. indépendantes.",
    parameters: [
      "`terrain_noise_scale = 14`",
      "`terrain_octaves = 3`",
      "couverture cible terre = 14 %",
      "post-traitement par composantes et amas: 6 amas, rayon 2 a 5"
    ],
    why:
      "Une loi usuelle scalaire ne suffit pas ici: il faut un champ spatial corrélé pour faire émerger des tâches organiques.",
    simulation:
      "Le générateur évalue `valueNoise` puis `fractalNoise`, applique des seuils, conserve les composantes cohérentes et ajoute des amas locaux de terre.",
    parameterChoice:
      "Les trois octaves donnent déjà un relief suffisant sans rendre le calcul coûteux sur tout le plateau.",
    dependence:
      "Forte corrélation spatiale: des cellules voisines partagent la même graine et des fréquences proches."
  },
  {
    title: "Champ spatial de l'eau",
    system: "Carte",
    lawUse: "Champ procédural corrélé dérivé de bruit",
    variable: L`X_{eau}(c) = h_{S_{eau}}(c)`,
    phenomenon:
      "Construit les poches d'eau et les petits lacs sans casser la jouabilite du plateau.",
    parameters: [
      "couverture cible eau = 4 %",
      "post-traitement par 3 lacs de rayon 2 a 3",
      "même échelle et même nombre d'octaves que la terre"
    ],
    why:
      "Comme pour la terre, on veut des zones spatialement cohérentes, pas une Bernoulli par cellule qui gribouillerait le plateau.",
    simulation:
      "Le pipeline rederive un score de bruit, applique un seuil propre à l'eau, filtre par composantes puis injecte quelques lacs complementaires.",
    parameterChoice:
      "La faible couverture 4 % evite de couper brutalement les couloirs de circulation du jeu.",
    dependence:
      "Corrélation spatiale importante et dépendance indirecte au champ de la terre via les contraintes d'assemblage du plateau final."
  },
  {
    title: "Masque de retournement des textures de terrain",
    system: "Carte",
    lawUse: "Pseudo-uniforme par hachage de position",
    variable: L`F(c) = \mathrm{hash}(worldSeed, type, c) \bmod 4`,
    phenomenon:
      "Retourne horizontalement et/ou verticalement les textures de terrain pour casser les repetitions visibles.",
    parameters: ["4 états de retournement", "hachage positionnel avec `worldSeed` et `CellType`"],
    why:
      "Le besoin principal est la reproductibilite locale, pas un échantillonnage i.i.d. complet à chaque frame.",
    simulation:
      "`terrainFlipMaskFor` rederive un seed mélange, hache la position puis conserve les deux bits de retournement utiles.",
    parameterChoice:
      "Conserver uniquement deux bits suffit pour coder aucun retournement, horizontal, vertical ou double retournement.",
    dependence:
      "Déterminisme strict par cellule; dependence quasi nulle a longue distance mais pas modelisee comme une loi scalaire autonome."
  },
  {
    title: "Bruit de contour du brouillard",
    system: "Météo",
    lawUse: "Champ procédurale de bord via value noise",
    variable: L`B(c) = 1 + (U(c)-0.5)\,a`,
    phenomenon:
      "Déforme la frontière théorique du brouillard pour obtenir un contour nuageux irrégulier.",
    parameters: [
      "`shape_noise_cell_span = 6`",
      "`shape_noise_amplitude_percent = 100`, donc `a = 1`",
      "`edge_softness_percent = 18`"
    ],
    why:
      "Un brouillard sans bruit aurait une silhouette trop analytique. Ici encore, il faut une fonction spatiale corrélée plutôt qu'une suite i.i.d. de variables.",
    simulation:
      "Le code évalue `valueNoise(shapeSeed, x, y, span)`, déforme la limite effective du brouillard, puis applique un fondu par `edgeSoftness`.",
    parameterChoice:
      "Une amplitude de 100 % autorise des bosses visibles, ensuite lisses par là grande échelle `span = 6` et le fondu de bord.",
    dependence:
      "Toutes les cellules du même brouillard partagent la même graine de forme, donc la corrélation spatiale est intentionnellement forte."
  }
];

function createTheory({ support, law, expectation, variance, note = "" }) {
  return { support, law, expectation, variance, note };
}

const NOMINAL_EXPECTATION = L`\text{Pas d'espérance canonique sans score auxiliaire } g`;
const NOMINAL_VARIANCE = L`\text{Pas de variance canonique sans score auxiliaire } g`;
const POSITION_EXPECTATION = L`\text{Pas d'espérance scalaire canonique sur une position 2D}`;
const POSITION_VARIANCE = L`\text{Pas de variance scalaire canonique sur une position 2D}`;
const FIELD_EXPECTATION = L`\text{Le résumé pertinent est spatial ou agrégé, pas cellule par cellule}`;
const FIELD_VARIANCE = L`\text{La variance utile est estimée sur la couverture ou la rugosité du champ}`;

function makeUniformFiniteTheory({ support, law, expectation, variance, note = "" }) {
  return createTheory({ support, law, expectation, variance, note });
}

function makePositionTheory({ support, law, note = "" }) {
  return createTheory({
    support,
    law,
    expectation: POSITION_EXPECTATION,
    variance: POSITION_VARIANCE,
    note
  });
}

function makeNominalTheory({ support, law, note = "" }) {
  return createTheory({
    support,
    law,
    expectation: NOMINAL_EXPECTATION,
    variance: NOMINAL_VARIANCE,
    note
  });
}

function makeUniform32Theory(symbol, note) {
  return makeUniformFiniteTheory({
    support: L`\{0,\dots,2^{32}-1\}`,
    law: L`\mathbb{P}(${symbol}=k)=\frac{1}{2^{32}}`,
    expectation: L`\mathbb{E}[${symbol}]=\frac{2^{32}-1}{2}`,
    variance: L`\mathrm{Var}(${symbol})=\frac{2^{64}-1}{12}`,
    note
  });
}

const processTheoryByTitle = {
  "Graine globale de la terre": makeUniform32Theory(
    L`S_{terre}`,
    "Les moments ci-dessus sont surtout formels: l'enjeu gameplay réel est l'uniformite de la seed et sa reproductibilite."
  ),
  "Graine globale de l'eau": makeUniform32Theory(
    L`S_{eau}`,
    "Comme pour la terre, la moyenne n'est pas interprétée directement en jeu; la seed sert à indexer un monde procédural reproductible."
  ),
  "Rotation des mines et fermes neutres": makeUniformFiniteTheory({
    support: L`\{0,1,2,3\}`,
    law: L`\mathbb{P}(R=r)=\frac{1}{4}`,
    expectation: L`\mathbb{E}[R]=\frac{3}{2}`,
    variance: L`\mathrm{Var}(R)=\frac{5}{4}`,
    note: "Le codage 0,1,2,3 represente simplement les quatre quarts de tour équiprobables."
  }),
  "Retournement des mines et fermes neutres": makeUniformFiniteTheory({
    support: L`\{0,1,2,3\}`,
    law: L`\mathbb{P}(F=f)=\frac{1}{4}`,
    expectation: L`\mathbb{E}[F]=\frac{3}{2}`,
    variance: L`\mathrm{Var}(F)=\frac{5}{4}`,
    note: "On réutilisé un codage à deux bits pour les quatre états de retournement possibles."
  }),
  "Choix de position des bâtiments publics": makePositionTheory({
    support: L`A_{top}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P=p\mid p\in A_{top})=\frac{1}{|A_{top}|}`,
    note:
      "La loi est uniforme sur les cellules admissibles du plateau courant, mais une position 2D n'a pas de moment scalaire canonique tant qu'on n'introduit pas une distance ou un score."
  }),
  "Apparition des royaumes": makePositionTheory({
    support: L`A_W\cup A_B\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P_K=p\mid p\in A_K)=\frac{1}{|A_K|}`,
    note:
      "Chaque royaume échantillonne uniformément une case d'apparition compatible avec sa zone. Le bon objet mathématique est une position, pas un scalaire."
  }),
  "Bord diagonal d'entrée du brouillard": makeNominalTheory({
    support: L`\{e_1,e_2\}`,
    law: L`\mathbb{P}(E=e_i)=\frac{1}{2}`,
    note: "Les deux diagonales admissibles sont symétriques et équiprobables."
  }),
  "Couverture cible du brouillard": makeUniformFiniteTheory({
    support: L`C\in[0.05,0.20]`,
    law: L`f_C(c)=\frac{1}{0.15}\,\mathbf{1}_{[0.05,0.20]}(c)`,
    expectation: L`\mathbb{E}[C]=\frac{0.05+0.20}{2}=0.125`,
    variance: L`\mathrm{Var}(C)=\frac{(0.20-0.05)^2}{12}`,
    note: "Cette variable fixe la part de plateau que le brouillard cherche a occuper avant discretisation sur la grille."
  }),
  "Allongement du brouillard": makeUniformFiniteTheory({
    support: L`A\in[1.80,2.60]`,
    law: L`f_A(a)=\frac{1}{0.80}\,\mathbf{1}_{[1.80,2.60]}(a)`,
    expectation: L`\mathbb{E}[A]=\frac{1.80+2.60}{2}=2.20`,
    variance: L`\mathrm{Var}(A)=\frac{(2.60-1.80)^2}{12}`,
    note: "La valeur echantillonnée est ensuite transformée en ellipse et trajectoire discrètes."
  }),
  "Graine de forme du brouillard": makeUniform32Theory(
    L`S_{forme}`,
    "La seed n'est pas interprétée seule: elle alimente le bruit de contour et doit surtout être uniformément répartie."
  ),
  "Graine de densité du brouillard": makeUniform32Theory(
    L`S_{densité}`,
    "Cette seed conditionne tout le champ d'opacité local du brouillard."
  ),
  "Apparition de secours sur la frontière pour une pièce du diable": makePositionTheory({
    support: L`A_{bord}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P=p\mid p\in A_{bord})=\frac{1}{|A_{bord}|}`,
    note:
      "Le support dépend des cases frontière encore libres au moment de l'apparition de secours."
  }),
  "Choix d'un mouvement aléatoire en phase Searching": makeNominalTheory({
    support: L`\mathcal{M}_{adm}(t)`,
    law: L`\mathbb{P}(M=m\mid m\in \mathcal{M}_{adm}(t))=\frac{1}{|\mathcal{M}_{adm}(t)|}`,
    note:
      "Le support est l'ensemble des coups légalement atteignables pour la pièce au tour courant, donc il change avec l'état du plateau."
  }),
  "Tie-break de retour vers le bord": makeNominalTheory({
    support: L`\mathcal{D}_{eq}(t)`,
    law: L`\mathbb{P}(D=d\mid d\in \mathcal{D}_{eq}(t))=\frac{1}{|\mathcal{D}_{eq}(t)|}`,
    note:
      "Cette uniforme ne sert qu'à trancher entre plusieurs directions équivalentes lors du retour vers le bord."
  }),
  "Ordre de placement des mines et fermes neutres": createTheory({
    support: L`\mathfrak{S}_5`,
    law: L`\mathbb{P}(\Pi=\pi)=\frac{1}{5!}`,
    expectation: L`\mathbb{E}[R_i]=\frac{5+1}{2}=3`,
    variance: L`\mathrm{Var}(R_i)=\frac{5^2-1}{12}=2`,
    note:
      "Une permutation n'a pas de moment canonique en tant qu'objet global; les moments affiches sont ceux du rang d'un bâtiment fixe dans l'ordre mélange."
  }),
  "Case d'apparition d'un coffre": makePositionTheory({
    support: L`A_{coffre}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(C=c_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Les poids favorisent certaines cellules admissibles, mais l'analyse de moyenne/variance ne devient pertinente qu'après choix d'un score spatial auxiliaire."
  }),
  "Type de récompense du coffre": makeNominalTheory({
    support: L`\{\text{or},\text{mouvement},\text{construction}\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "La variable est nominale: on ne prend pas la moyenne d'indices arbitraires, on étudie les probabilités de chaque catégorie."
  }),
  "Direction du brouillard": makeNominalTheory({
    support: L`\{N,S,E,O,NE,NO,SE,SO\}`,
    law: L`\mathbb{P}(D=d_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "La direction est une catégorie orientée; l'objet statistique central est la fréquence de chaque orientation."
  }),
  "Type de cible primaire d'une pièce du diable": makeNominalTheory({
    support: L`\{t_1,\dots,t_m\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le code choisit une famille de cible prioritaire plutôt qu'une grandeur numérique."
  }),
  "Option d'apparition ciblée d'une pièce du diable": makeNominalTheory({
    support: L`\{o_1,\dots,o_m\}`,
    law: L`\mathbb{P}(O=o_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Cette catégorielle arbitre entre plusieurs heuristiques d'apparition cible. Les poids varient avec l'état tactique."
  }),
  "Type de remplacement d'une pièce du diable": makeNominalTheory({
    support: L`\{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le tirage porte sur une catégorie de pièce, pas sur une valeur numérique ordonnée."
  }),
  "Cible de remplacement d'une pièce du diable": makeNominalTheory({
    support: L`\{c_1,\dots,c_m\}`,
    law: L`\mathbb{P}(C=c_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le support regroupe les cibles admissibles pour le remplacement. On compare donc des poids et non des moyennes."
  }),
  "Royaume cible d'une pièce du diable": createTheory({
    support: L`K_t\in\{0,1\}`,
    law: L`K_t\sim\mathrm{Bernoulli}(p_t)`,
    expectation: L`\mathbb{E}[K_t]=p_t`,
    variance: L`\mathrm{Var}(K_t)=p_t(1-p_t)`,
    note:
      "Le paramètre dynamique `p_t` est dérivé de la dette de sang normalisée; 1 peut être codé comme 'royaume blanc cible'."
  }),
  "Activation d'un mouvement aléatoire en phase Searching": createTheory({
    support: L`A_t\in\{0,1\}`,
    law: L`A_t\sim\mathrm{Bernoulli}(p),\qquad p=0.333`,
    expectation: L`\mathbb{E}[A_t]=p=0.333`,
    variance: L`\mathrm{Var}(A_t)=p(1-p)`,
    note:
      "Cette Bernoulli décide si la pièce infernale abandonne sa trajectoire guidée pour un coup aléatoire au tour courant."
  }),
  "éclenchement d'apparition d'une pièce du diable": createTheory({
    support: L`N_t\in\mathbb{N}`,
    law: L`N_t\sim\mathrm{Poisson}(\lambda_t)`,
    expectation: L`\mathbb{E}[N_t]=\lambda_t`,
    variance: L`\mathrm{Var}(N_t)=\lambda_t`,
    note:
      "Le runtime transforme ensuite ce comptage en événement booléen via `N_t \\ge 1`, mais la loi parente reste bien une Poisson."
  }),
  "Récompenses d'XP": createTheory({
    support: L`Y\in\{m,m+1,\dots,b\}`,
    law: L`Y=\max\!\bigl(m,\mathrm{round}(\mathrm{clip}(X,[a,b]))\bigr),\quad X\sim\mathcal{N}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\,\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}`,
    variance: L`\mathrm{Var}(X\mid a\le X\le b)=\sigma^2\!\left[1+\frac{\alpha\varphi(\alpha)-\beta\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}-\left(\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}\right)^2\right]`,
    note:
      "Les formules affichent les moments de la normale tronquée parente, avec `\\alpha=(a-\\mu)/\\sigma` et `\\beta=(b-\\mu)/\\sigma`. L'arrondi et le minimum du runtime déplacent légèrement la moyenne finale."
  }),
  "Montant d'or d'un coffre": createTheory({
    support: L`G\in\{1,2,\dots\}`,
    law: L`G=\max\!\bigl(1,\mathrm{round}(\mathrm{clip}(X,[\mu-2\sigma,\mu+2\sigma]))\bigr),\quad X\sim\mathcal{N}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\,\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}`,
    variance: L`\mathrm{Var}(X\mid a\le X\le b)=\sigma^2\!\left[1+\frac{\alpha\varphi(\alpha)-\beta\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}-\left(\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}\right)^2\right]`,
    note:
      "On note ici `a=\\mu-2\\sigma` et `b=\\mu+2\\sigma`. Comme pour l'XP, le runtime applique ensuite un arrondi et un plancher à 1."
  }),
  "Délai de réapparition d'un coffre": createTheory({
    support: L`D\in\{c,c+1,\dots\}`,
    law: L`D=\max\!\bigl(c,\mathrm{round}(T)\bigr),\quad T\sim\mathrm{Weibull}(k,\lambda)`,
    expectation: L`\mathbb{E}[T]=\lambda\,\Gamma\!\left(1+\frac{1}{k}\right),\qquad \mathbb{E}[D]\approx \max\!\left(c,\mathbb{E}[T]\right)`,
    variance: L`\mathrm{Var}(T)=\lambda^2\!\left[\Gamma\!\left(1+\frac{2}{k}\right)-\Gamma\!\left(1+\frac{1}{k}\right)^2\right]`,
    note:
      "Les moments fermes ci-dessus sont ceux de la Weibull continue; l'effet exact du `round` puis du plancher `c` est traité empiriquement dans les histogrammes."
  }),
  "Délai entre deux brouillards": createTheory({
    support: L`D\in\{m,m+1,\dots\}`,
    law: L`D=m+\lceil T\rceil,\quad T\sim\Gamma(k,\theta)`,
    expectation: L`\mathbb{E}[T]=k\theta,\qquad \mathbb{E}[D]\approx m+k\theta`,
    variance: L`\mathrm{Var}(T)=k\theta^2`,
    note:
      "Le plafond discretise la variable continue. Les moments affichés sont donc des références théoriques autour desquelles le runtime se concentre."
  }),
  "Durée visible d'un brouillard": createTheory({
    support: L`V\in\{1,2,\dots\}`,
    law: L`V=\max(1,\lceil T\rceil),\quad T\sim\Gamma(k,\theta)`,
    expectation: L`\mathbb{E}[T]=k\theta,\qquad \mathbb{E}[V]\approx \max(1,k\theta)`,
    variance: L`\mathrm{Var}(T)=k\theta^2`,
    note:
      "La Gamma fournit la durée cible continue, puis le runtime la convertit en nombre entier de tours visibles."
  }),
  "Densité locale d'un brouillard": createTheory({
    support: L`A(c)\in[0.22,0.82]`,
    law: L`A(c)=\mathrm{clip}(0.48\,X(c),0.22,0.82),\quad X(c)\sim\mathrm{LogNormal}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X(c)]=e^{\mu+\sigma^2/2}`,
    variance: L`\mathrm{Var}(X(c))=(e^{\sigma^2}-1)e^{2\mu+\sigma^2}`,
    note:
      "Les moments affichés sont ceux de la log-normale parente. Le `clip` d'alpha et le partage d'une même seed de densité déforment ensuite la loi finale observée."
  }),
  "Luminosité de l'herbe": createTheory({
    support: L`Y\in[0.68,1]`,
    law: L`Y=h(B),\quad B\sim\mathrm{Beta}(7,2)`,
    expectation: L`\mathbb{E}[Y]=\int_0^1 h(b)\,f_{\mathrm{Beta}(7,2)}(b)\,db`,
    variance: L`\mathrm{Var}(Y)=\int_0^1 h(b)^2 f_{\mathrm{Beta}(7,2)}(b)\,db-\mathbb{E}[Y]^2`,
    note:
      "La transformation déterministe `h` encode le seuil à 0.90, le remappage vers `[0.68,1]` et l'exposant de contraste 1.8."
  }),
  "Position d'entrée le long du bord d'un brouillard": createTheory({
    support: L`X\in[0,M]`,
    law: L`f_X(x)=\mathrm{piecewiseLinear}(x;\,0,\tfrac14M,\tfrac12M,\tfrac34M,M;\,0.7,1.8,1.98,1.8,0.7)`,
    expectation: L`\mathbb{E}[X]=\int_0^M x\,f_X(x)\,dx`,
    variance: L`\mathrm{Var}(X)=\int_0^M x^2 f_X(x)\,dx-\mathbb{E}[X]^2`,
    note:
      "Les intégrales sont calculées numériquement par la bibliothèque de distribution linéaire par morceaux, ce qui colle exactement à la méthode de simulation du runtime."
  }),
  "Champ spatial de la terre": createTheory({
    support: L`X_{terre}:\mathcal{G}\to\{0,1\}`,
    law: L`X_{terre}(c)=\mathbf{1}\{n_{terre}(c;S_{terre})+p_{terre}(c)>\tau_{terre}\}`,
    expectation: L`\mathbb{E}[\bar X_{terre}]\approx 0.14,\qquad \bar X_{terre}=\frac{1}{|\mathcal{G}|}\sum_{c\in\mathcal{G}} X_{terre}(c)`,
    variance: L`\mathrm{Var}(\bar X_{terre})\text{ est estimée par simulation car les cellules sont corrélées}`,
    note:
      "Pour un champ procédural, on ne présente pas la moyenne d'une cellule abstraite mais celle d'un résumé interprétable: ici la couverture totale en terre."
  }),
  "Champ spatial de l'eau": createTheory({
    support: L`X_{eau}:\mathcal{G}\to\{0,1\}`,
    law: L`X_{eau}(c)=\mathbf{1}\{n_{eau}(c;S_{eau})+p_{eau}(c)>\tau_{eau}\}`,
    expectation: L`\mathbb{E}[\bar X_{eau}]\approx 0.04,\qquad \bar X_{eau}=\frac{1}{|\mathcal{G}|}\sum_{c\in\mathcal{G}} X_{eau}(c)`,
    variance: L`\mathrm{Var}(\bar X_{eau})\text{ est estimée par simulation car la structure spatiale n'est pas i.i.d.}`,
    note:
      "Comme pour la terre, la quantité suivie est la couverture d'eau du plateau et non un pseudo-tirage indépendant cellule par cellule."
  }),
  "Masque de retournement des textures de terrain": createTheory({
    support: L`F(c)\in\{0,1,2,3\}`,
    law: L`\mathbb{P}(F(c)=f)\approx \frac{1}{4}`,
    expectation: L`\mathbb{E}[F(c)]\approx \frac{3}{2}`,
    variance: L`\mathrm{Var}(F(c))\approx \frac{5}{4}`,
    note:
      "La loi est seulement pseudo-uniforme car elle provient d'un hachage déterministe de la position. Les moments sont ceux du codage entier des quatre flips."
  }),
  "Bruit de contour du brouillard": createTheory({
    support: L`B(c)\in[0.5,1.5]`,
    law: L`B(c)=1+(U(c)-0.5)\,a,\qquad a=1`,
    expectation: L`\mathbb{E}[B(c)]\approx 1`,
    variance: L`\mathrm{Var}(B(c))=a^2\,\mathrm{Var}(U(c))\quad \text{(estimée empiriquement)}`,
    note:
      "`U(c)` vient d'un value noise spatialement corrélé, pas d'une uniforme i.i.d.; la variance vraiment utile est donc mesurée sur la rugosité de contour observée."
  })
};

const illustratedUniformProcesses = uniformProcesses.map(withProcessIllustration);
const illustratedPermutationProcesses = permutationProcesses.map(withProcessIllustration);
const illustratedCategoricalProcesses = categoricalProcesses.map(withProcessIllustration);
const illustratedBernoulliProcesses = bernoulliProcesses.map(withProcessIllustration);
const illustratedPoissonProcesses = poissonProcesses.map(withProcessIllustration);
const illustratedTruncatedNormalProcesses = truncatedNormalProcesses.map(withProcessIllustration);
const illustratedWeibullProcesses = weibullProcesses.map(withProcessIllustration);
const illustratedGammaProcesses = gammaProcesses.map(withProcessIllustration);
const illustratedLogNormalProcesses = logNormalProcesses.map(withProcessIllustration);
const illustratedBetaProcesses = betaProcesses.map(withProcessIllustration);
const illustratedPiecewiseLinearProcesses = piecewiseLinearProcesses.map(withProcessIllustration);
const illustratedProceduralProcesses = proceduralProcesses.map(withProcessIllustration);

export const randomnessReport = {
  hero: {
    kicker: "",
    title: "A Normal Chess Game",
    lead:
      "Un jeu d'échecs de conquête où le territoire, l'économie, la météo et le brouillard recomposent chaque partie.",
    source: ""
  },
  gameIntroduction: {
    blocks: [
      {
        title: "1. Deux royaumes, un roi à protéger",
        vignetteId: "kingdoms",
        paragraphs: [
          "Chaque joueur contrôle un royaume blanc ou noir avec un roi, des pièces et ses propres bâtiments. Les règles de déplacement sont les mêmes que dans un jeu d'échecs classique (quelques ajustements ont été nécessaires pour le pion afin qu'il puisse bouger dans toutes les directions). La condition centrale reste simple: protéger son roi et mettre le royaume adverse en échec.",
          "La différence avec un échiquier classique, c'est que les pièces jouent sur une vraie carte. Les distances, les obstacles, les ressources et les événements du terrain comptent autant que les mouvements eux-mêmes."
        ]
      },
      {
        title: "2. Chaque tour donne des points de mouvement et des points de construction",
        vignetteId: "turnBudget",
        paragraphs: [
          "Un tour ne correspond pas à un seul coup. Chaque royaume reçoit un stock de **points de mouvement** pour déplacer ses pièces et un stock de **points de construction** pour poser, réparer ou lancer des bâtiments.",
          "**Par exemple, bouger une tour coûte 4 points de mouvement, alors que bouger un pion ne coûte qu'1 point de mouvement.** Ces deux budgets imposent un arbitrage permanent. Si vous dépensez vos points pour avancer vos pièces, vous construisez moins. Si vous investissez dans les bâtiments, vous ralentissez votre pression immédiate sur la carte."
        ]
      },
      {
        title: "3. Le terrain et l'eau changent les chemins possibles",
        vignetteId: "terrain",
        paragraphs: [
          "La carte est régénérée à chaque partie. On y trouve de l'herbe, de la terre et surtout des zones d'eau qui bloquent certaines trajectoires.",
          "L'eau force des détours, ferme des accès et protège parfois un côté du plateau. Avant même de parler d'économie ou d'événements aléatoires, il faut donc lire quels couloirs restent vraiment praticables."
        ]
      },
      {
        title: "4. Les bâtiments publics donnent des ressources et des objectifs de carte",
        vignetteId: "economy",
        paragraphs: [
          "La carte contient aussi des **bâtiments publics**, c'est-à-dire des **structures neutres à capturer**, par exemple des mines, des fermes ou des églises. Ils ne servent pas de décor: ils créent des points à contrôler pour gagner plus de valeur sur la durée.",
          "**Par exemple, si une pièce blanche occupe une mine, elle rapporte 10 d'or par tour.** Conquérir ces zones change donc directement l'économie. On ne joue pas seulement contre le roi adverse; on joue aussi pour tenir les secteurs qui donnent de l'or, de la production ou de la progression."
        ]
      },
      {
        title: "5. Construire et produire des pièces fait partie du tour",
        vignetteId: "production",
        paragraphs: [
          "Les **points de construction** servent notamment a poser ou reparer des structures. Les casernes permettent ensuite de produire de nouvelles pièces au lieu de se contenter de l'armée de départ.",
          "La partie devient donc un jeu de développement en plus d'un jeu tactique. Vous pouvez consolider votre base, ouvrir un nouvel axe d'attaque ou préparer une pièce supplémentaire pour les tours suivants."
        ]
      },
      {
        title: "6. Arène et église permettent d'améliorer une pièce de manière précise",
        vignetteId: "progression",
        paragraphs: [
          "**Une arène fait progresser une pièce en expérience au fil des tours**, et **une église permet certaines transformations spéciales**. Ce système n'est donc pas abstrait: il passe par des bâtiments précis et par des combinaisons précises.",
          "**Par exemple, si on réunit dans une église un roi, un fou et une tour, alors la tour se transforme en reine.** La démonstration à droite montre exactement ce cas, puis recommence en boucle."
        ]
      },
      {
        title: "7. Les coffres donnent de l'or ou augmentent les budgets du tour",
        vignetteId: "chest",
        paragraphs: [
          "Des coffres apparaissent pendant la partie sur des cases visibles et contestables. Une pièce qui atteint un coffre l'ouvre immédiatement et obtient une récompense aléatoire.",
          "Cette récompense peut être de l'**or**, un bonus permanent de **mouvement** maximal par tour, ou un bonus permanent de **construction** maximale par tour. Comme ces gains modifient directement vos budgets, les coffres créent de vraies courses sur la carte."
        ]
      },
      {
        title: "8. Le brouillard peut cacher des pièces et des bâtiments ennemis",
        vignetteId: "weather",
        paragraphs: [
          "Le brouillard ne sert pas seulement d'habillage visuel. Il peut cacher des pièces ou des bâtiments ennemis s'ils sont dans la zone couverte.",
          "**Par exemple, un joueur peut se déplacer sous un brouillard pour mener une embuscade sur la base d'un autre joueur.** Le point important est que ce qui est caché est lié au point de vue. Les Blancs et les Noirs ne voient donc pas toujours les mêmes informations au même moment."
        ]
      },
      {
        title: "9. Les pièces du diable ajoutent une menace autonome en plus des deux royaumes",
        vignetteId: "infernal",
        paragraphs: [
          "Le jeu suit une **dette de sang** pour chaque royaume, c'est-à-dire un **compteur de menace** qui monte quand les captures et les dégâts s'accumulent, puis redescend progressivement avec le temps.",
          "**Dans l'exemple, une pièce du diable de type tour apparaît sur le bord droit, capture d'abord le fou blanc le plus proche, puis le pion blanc.** Plus la dette totale monte, plus une pièce du diable a de chances d'apparaître au bord de la carte. Cette pièce autonome cible un royaume, se déplace seule et ajoute une pression supplémentaire qu'aucun des deux joueurs ne contrôle directement."
        ]
      }
    ]
  },
  randomnessLink: {
    title: "Pourquoi les processus aléatoires sont essentiels au jeu",
    paragraphs: [
      "Ce rapport montre pourquoi les processus aléatoires sont essentiels au jeu: ils renouvellent les parties, enrichissent les choix stratégiques et apportent une vraie plus-value par rapport aux échecs classiques, tout en restant assez lisibles pour être analysés, modélisés et observés."
    ],
    sections: [],
    reportDimensionsTitle: "Les trois dimensions du rapport",
    reportDimensions: [
      {
        title: "1. Inventaire des processus aléatoires",
        text:
          "La première couche du site recense les variables aléatoires actives de la codebase, leur famille de loi, leurs paramètres, leur support, leur méthode de simulation et leur ancrage déterministe dans `worldSeed` et les compteurs RNG sérialisés.",
        showSummaryStats: true
      },
      {
        title: "2. 500 parties simulées",
        sourceKind: "simulated",
        text:
          "La seconde couche repose sur 500 simulations théoriques. Il ne s'agit pas de matchs entre IA ni de parties humaines accélérées: on simule directement les mécanismes concernés, par exemple la génération du terrain, les coffres, la météo ou les apparitions infernales, afin d'estimer leur comportement attendu."
      },
      {
        title: "3. Une partie réelle instrumentée",
        sourceKind: "real",
        text:
          "La troisième couche exploite les données d'une partie complète jouée pendant plusieurs heures avec un ami. Les actions, les tours et les états utiles ont été enregistrés pour confronter la théorie à un runtime réel et montrer des résultats lisibles en situation de jeu."
      }
    ],
    replayTitle: "Replay de la partie réelle",
    replayText:
      "Le viewer ci-dessous donne un aperçu direct de la partie observée, en lecture automatique rapide, avec boucle et point de vue blanc. Il sert d'entrée visuelle avant le détail statistique du rapport."
  },
  summaryStats: [
    {
      value: "38",
      label: "processus actifs",
      detail: "inventoriés dans l'audit runtime et reclassés ici par lois"
    },
    {
      value: "12",
      label: "familles modélisantes",
      detail: "lois usuelles + champs procéduraux corrélés"
    },
    {
      value: "5",
      label: "sous-systèmes jouables",
      detail: "carte, XP, coffres, météo, pièces du diable"
    },
    {
      value: "worldSeed",
      label: "racine déterministe",
      detail: "complétée par des compteurs RNG sérialisés par système"
    }
  ],
  methodology: {
    paragraphs: [
      "Le jeu n'utilise pas l'aléatoire comme une boîte noire. Chaque système stochastique passe par un schéma récurrent: une seed de monde `worldSeed`, un compteur d'événements `rngCounter`, puis une transformation spécifique au système. L'analyse probabiliste correcte doit donc raisonner à deux niveaux: la loi brute tirée par la bibliothèque standard, puis la loi effectivement observée après conditionnement, troncature, arrondi ou filtrage de gameplay.",
      "La classification choisie ici suit les lois plutôt que les fichiers. Cela permet de voir immédiatement que l'XP et l'or des coffres sont deux usages du même schéma de normale tronquée; que la météo combine uniforme, Gamma, log-normale et linéaire par morceaux; et que certains processus de carte ou de contour ne sont pas bien décrits par une variable scalaire classique mais par des champs spatiaux corrélés.",
      "Quand une variable est nominale, par exemple un type de récompense ou une direction, on insiste sur le fait qu'il n'existe pas d'espérance canonique sans choisir au préalable un score numérique auxiliaire. Ce point est essentiel pour ne pas écrire de formules fausses juste parce qu'une API de tirage renvoie un entier d'indice."
    ],
    formulas: [
      {
        label: "Schéma déterministe transverse",
        latex: L`U_t = G\bigl(worldSeed, rngCounter_t\bigr), \qquad X_t = \Phi\bigl(U_t, S_t\bigr)`
      },
      {
        label: "Evolution du compteur",
        latex: L`rngCounter_{t+1} = rngCounter_t + 1 \quad \text{à chaque événement consommatif}`
      },
      {
        label: "Principe de lecture du rapport",
        latex: L`\text{loi observée} = \text{transformation gameplay} \circ \text{loi standard}`
      }
    ],
    highlights: [
      "Les systèmes XP, Coffres, Météo et Pièces du diable possèdent chacun leur compteur RNG sérialisé; le déterminisme persiste donc après sauvegarde/rechargement.",
      "Les seeds auxiliaires de météo et de génération de carte sont elles-mêmes des variables aléatoires uniformes à grand support, mais elles servent ensuite à piloter des champs non i.i.d.",
      "Le rapport distingue toujours la loi théorique continue de la loi runtime réellement observée quand un arrondi, un `ceil` ou un `clamp` est appliqué."
    ]
  },
  outputStats: [
    {
      title: "Variables états sérialisées",
      text:
        "La sauvegarde ne mémorise pas seulement le résultat final; elle mémorise aussi l'état probabiliste nécessaire pour rejouer la suite de la partie sans dérive de seed.",
      bullets: [
        "`worldSeed` fixe le monde de référence.",
        "`rngCounter` de XP, Coffres, Météo et Pièces du diable est sérialisé par système.",
        "Les états dérivés comme `rewardRngCounter`, `currentRewardGeneration` et les descripteurs de brouillard conservent la continuité des lois conditionnelles."
      ]
    },
    {
      title: "Séries statistiques directement extractibles",
      text:
        "Le code et les exports JSON permettent déjà de reconstruire plusieurs séries quantitatives utiles pour la validation empirique du modèle.",
      bullets: [
        "Histogrammes d'XP par source et comparaison à la normale tronquée annoncée.",
        "Retards de reapparition des coffres et repartition des recompenses par regime early/late.",
        "Inter-arrivées météo, durées visibles, couverture, aspect ratio et opacités locales.",
        "Dette de sang, intensité d'apparition induite et types de cibles effectivement sélectionnés."
      ]
    },
    {
      title: "Sorties déjà visibles dans le projet",
      text:
        "Le depot contient déjà plusieurs supports de vérification pratique pour confronter la théorie au runtime.",
      bullets: [
        "`debug_game_state/` contient des historiques de tours utiles pour relire les evenements stochastiques.",
        "`saves/` et `PARTICULAR SAVES/` montrent la persistance des seeds et compteurs dans des etats concrets.",
        "`statistiques-generator/` offre une base naturelle pour automatiser demain des comparaisons entre distributions attendues et distributions observées."
      ]
    }
  ],
  lawSections: [
    {
      id: "uniformes",
      title: "Uniformes discrètes, continues et conditionnelles",
      badge: "15 processus",
      description: [
        "Cette famille couvre les tirages symétriques sur un support fini, les pourcentages uniformes sur un intervalle continu et les choix uniformes conditionnés à un sous-ensemble admissible. Elle regroupe aussi plusieurs seeds intermédiaires, mathématiquement ordinaires mais importantes pour le gameplay parce qu'elles pilotent ensuite des champs procéduraux.",
        "Dans le code, ces lois apparaissent surtout via `std::uniform_int_distribution`, des appels directs à `generator()` sur 32 bits, ou des tirages uniformes après filtrage et tri de candidats."
      ],
      formulaCards: [
        {
          label: "Uniforme discrète",
          latex: L`X \sim \mathcal{U}_d(\{a,\dots,b\}), \qquad \mathbb{P}(X=k)=\frac{1}{b-a+1}`
        },
        {
          label: "Uniforme continue",
          latex: L`X \sim \mathcal{U}([a,b]), \qquad f_X(x)=\frac{1}{b-a}\mathbf{1}_{[a,b]}(x)`
        },
        {
          label: "Moments",
          latex: L`\mathbb{E}[X]=\frac{a+b}{2}, \qquad \mathrm{Var}(X)=\begin{cases}\frac{(b-a+1)^2-1}{12} & \text{discret}\\[4pt]\frac{(b-a)^2}{12} & \text{continu}\end{cases}`
        },
        {
          label: "Uniforme conditionnelle",
          latex: L`\mathbb{P}(X=x\mid X\in A)=\frac{1}{|A|}, \qquad x\in A`
        }
      ],
      notes: [
        "Pour les seeds 32 bits, la formule pertinente est celle de l'uniforme discrète sur un grand espace fini; leur espérance existe mais n'a pas d'intérêt gameplay direct.",
        "Quand le support dépend de filtres géométriques, la loi observable n'est pas la loi brute mais la loi conditionnelle sur l'ensemble admissible courant."
      ],
      processes: illustratedUniformProcesses
    },
    {
      id: "permutation-uniforme",
      title: "Permutation uniforme",
      badge: "1 processus",
      description: [
        "Le placement public ne commence pas par choisir des positions, mais par mélanger l'ordre des objets à poser. La variable naturelle n'est donc pas un entier simple, mais une permutation uniforme sur un ensemble fini.",
        "Pour retrouver des moments scalaires, on peut regarder une variable dérivée comme le rang d'un objet fixe dans la permutation."
      ],
      formulaCards: [
        {
          label: "Loi sur le groupe symétrique",
          latex: L`\Pi \sim \mathrm{Unif}(\mathfrak{S}_n), \qquad \mathbb{P}(\Pi=\pi)=\frac{1}{n!}`
        },
        {
          label: "Rang d'un objet fixe",
          latex: L`R_i \sim \mathcal{U}_d(\{1,\dots,n\}), \qquad \mathbb{E}[R_i]=\frac{n+1}{2}, \quad \mathrm{Var}(R_i)=\frac{n^2-1}{12}`
        }
      ],
      notes: [
        "L'espérance d'une permutation en tant qu'objet du groupe symétrique n'est pas canonique; c'est pourquoi on passe par une statistique dérivée comme le rang.",
        "Cette permutation agit ensuite en amont des tirages uniformes conditionnels de position."
      ],
      processes: illustratedPermutationProcesses
    },
    {
      id: "categorielles",
      title: "Catégorielle pondérée",
      badge: "7 processus",
      description: [
        "Dès qu'il faut choisir entre plusieurs catégories nominales avec des poids relatifs, la bonne famille est la catégorielle pondérée. C'est le cheval de bataille des coffres, de la météo et surtout de la logique des pièces du diable.",
        "Mathématiquement, l'espérance n'est pas définie tant qu'on n'a pas choisi une fonction de score `g` sur les catégories; on donne donc les moments de `g(X)` plutôt que ceux de `X` lui-même."
      ],
      formulaCards: [
        {
          label: "PMF pondérée",
          latex: L`\mathbb{P}(X=x_i)=\frac{w_i}{\sum_{j=1}^n w_j}, \qquad w_i \ge 0`
        },
        {
          label: "Moments via un score",
          latex: L`\mathbb{E}[g(X)] = \sum_{i=1}^n g(x_i)\,\frac{w_i}{\sum_j w_j}, \qquad \mathrm{Var}(g(X)) = \mathbb{E}[g(X)^2] - \mathbb{E}[g(X)]^2`
        }
      ],
      notes: [
        "Des poids égaux redonnent une uniforme discrète, mais l'implémentation reste la même en code via `std::discrete_distribution`.",
        "Quand les poids dépendent du plateau, de la dette ou de la visibilité, la loi devient conditionnelle à l'état courant."
      ],
      processes: illustratedCategoricalProcesses
    },
    {
      id: "bernoulli",
      title: "⭐ Bernoulli",
      badge: "2 processus",
      description: [
        "La Bernoulli intervient pour les décisions binaires: choisir un royaume plutôt que l'autre, ou activer une branche de comportement aléatoire.",
        "Dans ce code, elle apparaît soit explicitement via `std::bernoulli_distribution`, soit implicitement via un tirage uniforme comparé à un seuil entier sur 1000."
      ],
      formulaCards: [
        {
          label: "PMF",
          latex: L`X \sim \mathrm{Bernoulli}(p), \qquad \mathbb{P}(X=1)=p, \quad \mathbb{P}(X=0)=1-p`
        },
        {
          label: "Moments",
          latex: L`\mathbb{E}[X]=p, \qquad \mathrm{Var}(X)=p(1-p)`
        }
      ],
      notes: [
        "La probabilité `p` peut être statique, comme 0.333, ou dependre dynamiquement de l'état du jeu comme la dette de sang.",
        "Une Bernoulli sur un support binaire reste la loi la plus lisible pour decrire ces branchements même quand l'implémentation passe par un entier uniforme."
      ],
      processes: illustratedBernoulliProcesses
    },
    {
      id: "poisson",
      title: "Poisson et déclenchement d'arrivée",
      badge: "1 processus",
      description: [
        "Les pièces du diable ne reposent pas sur une simple probabilité fixe d'apparition, mais sur un comptage d'arrivées potentielles modèle par une Poisson. Le gameplay observé seulement l'événement `N >= 1`, mais la variable latente est bien un nombre entier de tentatives.",
        "Ce choix donne une interpretation propre de l'intensité comme dette de sang convertie en fréquence moyenne d'arrivées."
      ],
      formulaCards: [
        {
          label: "PMF Poisson",
          latex: L`\mathbb{P}(N=k)=e^{-\lambda}\frac{\lambda^k}{k!}, \qquad k \in \mathbb{N}`
        },
        {
          label: "Moments",
          latex: L`\mathbb{E}[N]=\lambda, \qquad \mathrm{Var}(N)=\lambda`
        },
        {
          label: "Probabilité d'apparition observée",
          latex: L`\mathbb{P}(N \ge 1)=1-e^{-\lambda}`
        }
      ],
      notes: [
        "Dans le gameplay courant, seule la classe d'événement `0` contre `>= 1` est exploitée, mais raisonner sur `N` reste plus juste que d'écrire directement une Bernoulli arbitraire.",
        "La dette de sang agit ici comme un paramètre d'intensité, pas comme un poids categoriel."
      ],
      processes: illustratedPoissonProcesses
    },
    {
      id: "normales-tronquees",
      title: "Normales tronquées et discrétisées",
      badge: "2 processus",
      description: [
        "L'XP et l'or des coffres reutilisent le même patron: une normale centree sur une moyenne conception, tronquée a un multiple de son écart-type, arrondie à l'entier puis soumise a un plancher minimal.",
        "La loi effectivement jouee n'est donc pas une gaussienne pure: c'est une gaussienne transformée par clamp et quantification."
      ],
      formulaCards: [
        {
          label: "Normale parente",
          latex: L`X \sim \mathcal{N}(\mu,\sigma^2), \qquad f(x)=\frac{1}{\sigma\sqrt{2\pi}}\exp\left(-\frac{(x-\mu)^2}{2\sigma^2}\right)`
        },
        {
          label: "Troncature sur [a,b]",
          latex: L`f_{[a,b]}(x)=\frac{f(x)}{\Phi\left(\frac{b-\mu}{\sigma}\right)-\Phi\left(\frac{a-\mu}{\sigma}\right)}\mathbf{1}_{[a,b]}(x)`
        },
        {
          label: "Moments de la version tronquée",
          latex: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)},\quad \alpha=\frac{a-\mu}{\sigma},\; \beta=\frac{b-\mu}{\sigma}`
        },
        {
          label: "Transformation runtime",
          latex: L`Y = \max\bigl(m, \mathrm{round}(\mathrm{clip}(X,[a,b]))\bigr)`
        }
      ],
      notes: [
        "L'arrondi et le minimum modifient légèrement l'espérance par rapport à la formule continue; la formule ci-dessus est donc la bonne référence théorique, pas la valeur exacte après discretisation.",
        "Dans ce code, `sigma = max(1, mean * sigmaMultiplier)` et `a,b = mean +/- clampMultiplier * sigma`."
      ],
      processes: illustratedTruncatedNormalProcesses
    },
    {
      id: "weibull",
      title: "Weibull discrétisée",
      badge: "1 processus",
      description: [
        "La Weibull apparaît pour les délais de réapparition des coffres. C'est un choix pertinent dès qu'on veut un temps d'attente positif dont la probabilité de survenue change avec l'ancienneté du délai écoulé.",
        "Le runtime n'utilise pas la variable continue telle quelle: il l'arrondit et la borne inférieurement par un cooldown minimal."
      ],
      formulaCards: [
        {
          label: "Densité Weibull",
          latex: L`f(t)=\frac{k}{\lambda}\left(\frac{t}{\lambda}\right)^{k-1}e^{-(t/\lambda)^k}\mathbf{1}_{t\ge 0}`
        },
        {
          label: "Moments continus",
          latex: L`\mathbb{E}[T]=\lambda\,\Gamma\left(1+\frac{1}{k}\right), \qquad \mathrm{Var}(T)=\lambda^2\left[\Gamma\left(1+\frac{2}{k}\right)-\Gamma\left(1+\frac{1}{k}\right)^2\right]`
        },
        {
          label: "Transformation runtime",
          latex: L`D = \max\bigl(c, \mathrm{round}(T)\bigr)`
        }
      ],
      notes: [
        "Avec `k > 1`, le hazard augmente avec le temps, ce qui correspond bien à l'intuition 'plus le coffre tarde, plus sa réapparition devient plausible'."
      ],
      processes: illustratedWeibullProcesses
    },
    {
      id: "gamma",
      title: "⭐ Gamma discrétisée",
      badge: "2 processus",
      description: [
        "La Gamma pilote les inter-arrivées et certaines durées météo. Son support positif et sa grande souplesse de forme en font un bon compromis entre exponentialité pure et modèle trop rigide.",
        "Le code applique `ceil`, puis convertit en tours ou pas de simulation. La loi observée est donc une version quantifiée de la Gamma continue."
      ],
      formulaCards: [
        {
          label: "Densité Gamma",
          latex: L`f(t)=\frac{1}{\Gamma(k)\theta^k} t^{k-1}e^{-t/\theta}\mathbf{1}_{t\ge 0}`
        },
        {
          label: "Moments continus",
          latex: L`\mathbb{E}[T]=k\theta, \qquad \mathrm{Var}(T)=k\theta^2`
        },
        {
          label: "Transformation runtime",
          latex: L`D = m + \lceil T \rceil`
        }
      ],
      notes: [
        "La météo utilise deux Gammas distinctes: l'une pour l'attente avant apparition, l'autre pour la durée visible cible."
      ],
      processes: illustratedGammaProcesses
    },
    {
      id: "lognormale",
      title: "Log-normale",
      badge: "1 processus",
      description: [
        "La log-normale apparaît dans la texture d'opacité des brouillards. Le choix est mathématiquement naturel dès qu'on veut des multiplicateurs strictement positifs, susceptibles d'être parfois un peu plus grands que 1 sans jamais devenir négatifs.",
        "Le runtime rederive la graine par cellule à partir de `densitySeed`, puis re-borné le résultat via des `alphaMin` et `alphaMax`."
      ],
      formulaCards: [
        {
          label: "Definition",
          latex: L`X \sim \mathrm{LogNormal}(\mu,\sigma^2) \Longleftrightarrow \ln X \sim \mathcal{N}(\mu,\sigma^2)`
        },
        {
          label: "Moments",
          latex: L`\mathbb{E}[X]=e^{\mu+\sigma^2/2}, \qquad \mathrm{Var}(X)=\bigl(e^{\sigma^2}-1\bigr)e^{2\mu+\sigma^2}`
        }
      ],
      notes: [
        "La variable finale vue à l'écran est encore transformée par `alpha = clamp(alphaBase * X, alphaMin, alphaMax)`, donc les moments exacts après clamp doivent être estimés numériquement."
      ],
      processes: illustratedLogNormalProcesses
    },
    {
      id: "beta",
      title: "Beta transformée",
      badge: "1 processus",
      description: [
        "La Beta est utilisée pour la luminosité de l'herbe. C'est une bonne famille pour modéliser une variable naturellement bornée dans `[0,1]` avant remappage visuel.",
        "Le code ne l'obtient pas par API directe, mais via le quotient de deux Gammas, ce qui est mathématiquement standard."
      ],
      formulaCards: [
        {
          label: "Densité Beta",
          latex: L`f(x)=\frac{\Gamma(\alpha+\beta)}{\Gamma(\alpha)\Gamma(\beta)}x^{\alpha-1}(1-x)^{\beta-1}\mathbf{1}_{[0,1]}(x)`
        },
        {
          label: "Moments",
          latex: L`\mathbb{E}[X]=\frac{\alpha}{\alpha+\beta}, \qquad \mathrm{Var}(X)=\frac{\alpha\beta}{(\alpha+\beta)^2(\alpha+\beta+1)}`
        },
        {
          label: "Construction par Gammas",
          latex: L`X = \frac{G_1}{G_1+G_2}, \qquad G_1 \sim \Gamma(\alpha,1),\; G_2 \sim \Gamma(\beta,1)`
        }
      ],
      notes: [
        "Avec `(\\alpha, \\beta) = (7, 2)`, la moyenne brute vaut `7/9`, donc les tirages se concentrent naturellement vers des valeurs élevées avant seuil et remappage."
      ],
      processes: illustratedBetaProcesses
    },
    {
      id: "piecewise-linear",
      title: "Linéaire par morceaux",
      badge: "1 processus",
      description: [
        "La position d'entrée d'un brouillard le long du bord n'est ni uniforme, ni gaussienne. Elle suit une densité dessinée à la main par morceaux linéaires afin de surpondérer les entrées centrales tout en gardant des coins possibles.",
        "C'est un bon exemple de loi standard de la bibliothèque C++ qui n'est pas toujours mobilisée dans les rapports probabilistes classiques, mais qui reste parfaitement légitime ici."
      ],
      formulaCards: [
        {
          label: "Densité linéaire par morceaux",
          latex: L`f(x)=\frac{\ell(x)}{\int_0^M \ell(u)\,du}, \qquad \ell \text{ linéaire sur chaque intervalle de noeuds}`
        },
        {
          label: "Moments généraux",
          latex: L`\mathbb{E}[X]=\int_0^M x f(x)\,dx, \qquad \mathrm{Var}(X)=\int_0^M x^2 f(x)\,dx - \mathbb{E}[X]^2`
        }
      ],
      notes: [
        "Les moments exacts se calculent numériquement à partir des nœuds et poids, ce qui est approprié puisque la densité est entièrement spécifiée par ces données."
      ],
      processes: illustratedPiecewiseLinearProcesses
    },
    {
      id: "procedural-fields",
      title: "⭐ Variables personnalisées et champs procéduraux corrélés",
      badge: "4 processus",
      description: [
        "Tous les processus aléatoires du jeu ne sont pas raisonnablement résumables par une unique variable scalaire. Les champs de terrain et les déformations de contour du brouillard sont des fonctions aléatoires de la cellule et d'une seed, avec forte corrélation spatiale.",
        "Les traiter comme des Bernoulli i.i.d. (**indépendantes et identiquement distribuées**), serait mathématiquement faux et trompeur au niveau du gameplay: on perdrait exactement la structure de régions, de bords et de textures que le code cherche à produire."
      ],
      formulaCards: [
        {
          label: "Champ aléatoire spatial",
          latex: L`X(c)=g_s(c), \qquad c \in \mathcal{G}`
        },
        {
          label: "Dépendance spatiale",
          latex: L`\mathrm{Cov}\bigl(X(c),X(c')\bigr) \neq 0 \quad \text{en général pour des cellules proches}`
        },
        {
          label: "Lecture correcte",
          latex: L`\text{processus observé} = \text{seed uniforme} + \text{fonction de bruit / post-traitement}`
        }
      ],
      notes: [
        "Ici, la bonne unité mathématique n'est plus 'une réalisation d'une loi scalaire', mais 'une réalisation d'un champ spatial'.",
        "Les statistiques pertinentes sont alors la couverture, la taille des composantes, la corrélation spatiale, la rugosité de bord ou la distribution des rayons effectifs."
      ],
      processes: illustratedProceduralProcesses
    }
  ],
  dependenceNotes: [
    "Le cœur du déterminisme est `worldSeed + rngCounter`; cela crée une dépendance structurelle commune à tous les tirages d'un même système, tout en rendant la suite parfaitement rejouable après sauvegarde.",
    "Les lois conditionnelles dominent le gameplay réel: une uniforme ou une catégorielle n'est presque jamais tirée sur un support absolu, mais sur un support déjà filtré par la géométrie, la visibilité, l'occupation ou l'historique des choix précédents.",
    "Le mode de rattrapage des coffres (`current_loot_catch_up_enabled`) signifie que **les deux royaumes partagent temporairement une même récompense courante**; **le tirage suivant n'apparaît que lorsque les deux l'ont déjà collectée**. Les récompenses de coffre ne sont donc **pas indépendantes** entre royaumes quand ce mode est actif.",
    "Les brouillards portent deux graines internes, l'une pour la forme et l'autre pour la densité, qui induisent de fortes corrélations spatiales intra-brouillard, puis une dépendance temporelle via la durée gamma et le prochain délai d'apparition.",
    "Les pièces du diable ne reposent pas sur un système à paramètres fixes: leur Bernoulli de royaume cible et leur Poisson d'apparition dépendent directement d'un état dynamique, la dette de sang."
  ],
  difficulties: [
    {
      title: "Transformer une loi théorique en variable runtime jouable",
      text:
        "Dans ce projet, presque aucune loi standard n'arrive brute à l'écran. Les récompenses d'XP et d'or sont tronquées, arrondies et bornées par un minimum; les délais Weibull et Gamma passent par `round` ou `ceil`; l'opacité du brouillard est encore `clampée` après la log-normale. La difficulté réelle a donc été de documenter à la fois la loi parente et la variable effectivement utilisée par le runtime, sans faire croire qu'un histogramme discret final est exactement une Gaussienne, une Gamma ou une Weibull continues."
    },
    {
      title: "Donner une lecture statistique correcte à des variables non numériques",
      text:
        "Une direction de brouillard, un type de récompense de coffre ou un type de pièce du diable ne portent pas naturellement une moyenne ou une variance. La difficulté n'était pas de calculer un nombre coûte que coûte, mais d'éviter une erreur de modélisation. Pour ces variables, j'ai donc choisi d'expliciter le support, la loi catégorielle et les poids, puis de réserver les moments aux cas où une variable numérique ou un score auxiliaire avaient un sens."
    },
    {
      title: "Mesurer des champs spatiaux corrélés sans les réduire à du i.i.d.",
      text:
        "La terre, l'eau et les contours de brouillard sont produits par du bruit procédural partageant une même seed et un même post-traitement spatial. Une statistique cellule par cellule aurait masqué le vrai phénomène, qui est l'apparition de régions cohérentes, de lacs, de couloirs et de silhouettes. La difficulté a donc été de choisir comme variables observées des résumés adaptés à un champ: couverture totale, cellules refusées, rugosité de bord, durée visible ou nombre de pièces masquées, plutôt qu'une fausse Bernoulli indépendante par cellule."
    },
    {
      title: "Instrumenter le runtime réel sans perdre le déterminisme des parties",
      text:
        "Le rapport ne repose pas seulement sur des simulations hors ligne: il fallait aussi extraire une partie réelle instrumentée, rejouable, et comparer ses trajectoires à des lois théoriques. Cela oblige à sérialiser correctement `worldSeed`, les compteurs RNG, les états de brouillard, la dette de sang et les événements d'apparition pour qu'une sauvegarde et un replay racontent exactement la même histoire statistique. Sans cette instrumentation, les jolies formules du rapport seraient restées découplées du jeu exécuté."
    },
    {
      title: "Relier batch simulé et partie réelle sans surinterpréter",
      text:
        "Les 500 parties simulées donnent des tendances robustes sur les familles de lois, mais une partie réelle unique reste fortement dépendante de son histoire tactique. La difficulté finale a donc été d'assumer cette asymétrie: utiliser le batch pour vérifier les supports, les ordres de grandeur et les histogrammes globaux, puis utiliser la partie instrumentée pour montrer comment les dépendances d'état, la visibilité et la dette de sang déformaient concrètement ces lois pendant une vraie partie."
    }
  ],
  perspectives: [
    "La première perspective est de **mieux ajuster les paramètres des mécanismes aléatoires** à partir d'un volume de parties plus important. Je n'ai pas encore assez de recul statistique pour équilibrer proprement ces variables: par exemple, les apparitions des pièces du diable produisent encore trop souvent des pions, alors que cette pièce est lente et peu impactante, et la loi normale des récompenses d'or des coffres reste trop resserrée autour de sa moyenne, ce qui rend les variations peu perceptibles pour le joueur.",
    "Une deuxième perspective est donc d'**accumuler beaucoup plus de données de partie** afin d'améliorer l'équilibrage général du jeu. L'objectif n'est pas seulement de décrire les lois utilisées, mais de disposer d'assez d'observations pour corriger les déséquilibres réels, ajuster les amplitudes utiles et vérifier que les événements aléatoires enrichissent effectivement la partie au lieu d'aplatir ses situations.",
    "Enfin, un chantier important sera de développer une **intelligence artificielle symbolique** capable d'agir à partir de règles déterministes tout en **anticipant des événements aléatoires probables**. Explorer cette articulation entre raisonnement symbolique et incertitude serait utile à la fois pour mieux jouer, pour mieux tester le jeu et pour mieux exploiter toutes les statistiques produites par ce travail."
  ]
};