const L = String.raw;

import { processIllustrationsByKey } from "./randomnessIllustrations.js";
import { buildReportProcessAnchor } from "../utils/reportAnchors.js";

function withProcessIllustration(process) {
  const theory = processTheoryByTitle[process.title];
  const parameterSections = parameterSectionsByTitle[process.title];
  const enrichedProcess = theory ? { ...process, theory } : process;
  const parameterizedProcess = parameterSections
    ? { ...enrichedProcess, parameterSections }
    : enrichedProcess;
  const illustration = parameterizedProcess.illustrationKey
    ? processIllustrationsByKey[parameterizedProcess.illustrationKey]
    : null;

  if (import.meta.env.DEV && parameterizedProcess.illustrationKey && !illustration) {
    console.warn(
      `[randomness-report] Missing illustration for key "${parameterizedProcess.illustrationKey}" (${parameterizedProcess.title}).`
    );
  }

  return illustration ? { ...parameterizedProcess, illustration } : parameterizedProcess;
}

const uniformProcesses = [
  {
    title: "Graines 32 bits des champs procéduraux",
    system: "Carte / Météo",
    lawUse: "Uniforme discrète sur un espace de 32 bits",
    variable: L`S_i \in \{0,\dots,2^{32}-1\}, \qquad i \in \{\text{terre},\text{eau},\text{forme},\text{densité}\}`,
    phenomenon:
      "Ici, la seed n'est pas un phénomène gameplay en soi : c'est un outil technique pour rendre la génération du terrain et du brouillard à la fois variée et parfaitement reproductible. Le point important est que `worldSeed` reste la racine du monde : les quatre graines synthétisées ici ne sont pas quatre racines indépendantes, mais quatre graines filles dérivées ensuite pour piloter la terre, l'eau, la forme du brouillard et sa densité.",
    parameters: [
      "4 graines 32 bits synthétisées ici : terre, eau, forme du brouillard, densité du brouillard",
      "2 tirages dérivés de `worldSeed` au début de la génération de carte, puis 2 tirages supplémentaires dérivés du générateur météo à chaque apparition de brouillard",
      "On ne change donc pas de principe déterministe : on part toujours de `worldSeed`, puis on spécialise la génération avec des seeds auxiliaires"
    ],
    why:
      "Ces variables ne représentent ni une ressource, ni une position, ni une décision lisible par le joueur : ce sont seulement des clés d'indexation pour des transformations déterministes plus riches. La bonne abstraction est donc la même partout : une uniforme discrète sur 32 bits, suffisamment vaste pour éviter les collisions perceptibles, neutre parce qu'aucune graine n'a de signification stratégique intrinsèque, et nativement compatible avec les sorties de `mt19937`. Les différences intéressantes apparaissent seulement après transformation, quand la seed devient champ de terre, champ d'eau, bruit de contour ou texture d'opacité.",
    simulation:
      "Avant d'aller plus loin, il faut définir `rngCounter` : c'est un entier propre à chaque système (XP, Coffres, Météo, Pièces du diable) qui s'incrémente de 1 à chaque tirage consommatif de ce système. Il est sauvegardé avec la partie. Son rôle est de rendre chaque tirage unique et reproductible : plutôt que d'initialiser un générateur une seule fois au démarrage (ce qui rendrait l'ordre d'exécution important), le système reconstruit à la volée un générateur distinct pour chaque événement en mélangeant `worldSeed` et la valeur courante de `rngCounter`. Résultat : tirer l'XP du 5e kill de la partie donne toujours le même résultat, quel que soit l'ordre dans lequel les autres systèmes ont consommé leurs propres tirages. Le runtime initialise d'abord un générateur parent à partir de `worldSeed` ou du couple `worldSeed + rngCounter`, puis consomme une sortie brute de `mt19937` chaque fois qu'il doit initialiser un champ procédural ou une texture météo. La seed auxiliaire obtenue sert ensuite à un traitement aval déterministe ou semi-déterministe.",
    parameterChoice:
      "Le format **32 bits** correspond exactement à la largeur native d'une sortie de `mt19937` : extraire les bits bruts évite toute conversion biaisée ou troncature qui surviendrait en réduisant une sortie 32 bits à un intervalle plus petit. Chacune des quatre graines filles (terre, eau, forme de brouillard, densité de brouillard) est extraite indépendamment depuis un générateur parent dédié, garantissant leur décorrélation : les champs de terre et d'eau ne sont pas systématiquement superposés. Un espace de `2^32 ≈ 4 milliards` de valeurs par graine laisse une diversité suffisante pour que deux mondes ou deux brouillards successifs soient perceptiblement distincts, sans aucune collision visuelle récurrente.",
    dependence:
      "Les graines de carte sont tirées directement depuis un générateur initialisé par `worldSeed`; les graines de brouillard sont tirées depuis un générateur d'événement lui-même construit à partir de `worldSeed` et du `rngCounter` météo. Dans les deux cas, `worldSeed` reste donc la racine commune, et la structure intéressante est aval, pas la seed fille elle-même.",
    relatedProcesses: [
      {
        seedLabel: "Graine terre",
        title: "Champ spatial de la terre",
        href: `#${buildReportProcessAnchor("procedural-fields", "Champ spatial de la terre")}`,
        summary: "Transforme la seed en couverture de terre corrélée et en amas jouables."
      },
      {
        seedLabel: "Graine eau",
        title: "Champ spatial de l'eau",
        href: `#${buildReportProcessAnchor("procedural-fields", "Champ spatial de l'eau")}`,
        summary: "Produit les poches d'eau et les petits lacs à partir d'un bruit distinct de la terre."
      },
      {
        seedLabel: "Graine de forme",
        title: "Bruit de contour du brouillard",
        href: `#${buildReportProcessAnchor("procedural-fields", "Bruit de contour du brouillard")}`,
        summary: "Déforme la frontière de l'ellipse pour obtenir un contour nuageux irrégulier."
      },
      {
        seedLabel: "Graine de densité",
        title: "Densité locale d'un brouillard",
        href: `#${buildReportProcessAnchor("lognormale", "Densité locale d'un brouillard")}`,
        summary: "Redérive un multiplicateur d'opacité log-normal cellule par cellule."
      }
    ]
  },
  {
    title: "Rotation des mines et fermes neutres",
    illustrationKey: "public-building-rotation",
    system: "Carte",
    lawUse: "Uniforme discrète sur quatre quarts de tour",
    variable: L`R \in \{0,1,2,3\}`,
    phenomenon:
      "Choisit l'orientation sprite des bâtiments publics déjà placés afin d'éviter un rendu trop mécanique.",
    parameters: ["support = {0, 1, 2, 3}"],
    why:
      "Les quatre rotations sont géométriquement symétriques pour ces assets; aucune ne doit être favorisée. Une rotation discrète ne peut prendre que des valeurs dans un ensemble fini de symétries, ici les quatre quarts de tour, ce qui exclut d'emblée toute loi continue. L'uniforme discrète sur quatre valeurs est la seule famille sans biais sur un groupe cyclique d'ordre 4 : choisir une catégorielle pondérée introduirait artificiellement une orientation préférée sans justification visuelle ni stratégique.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` pendant la génération du plateau.",
    codeSnippet:
`// BoardGenerator.cpp
std::uniform_int_distribution<int> rotationDist(0, 3);
std::uniform_int_distribution<int> flipMaskDist(0, 3);
placement.rotationQuarterTurns = rotationDist(random);
placement.flipMask             = flipMaskDist(random);`,
    parameterChoice:
      "Les quatre rotations (0°, 90°, 180°, 270°) épuisent exactement les isométries de rotation sur grille carrée, formant le groupe cyclique Z/4Z. Aucune orientation n'est visuellement ni stratégiquement préférable pour un bâtiment symétrique dont la position est fixée avant la partie, donc `P(R = r) = 1/4 = 25 %` pour tout `r ∈ {0, 1, 2, 3}`. Le support est de taille 4, sans paramètre de pondération : tout biais aurait introduit une orientation structurellement dominante perceptible après quelques parties.",
    dependence:
      "Dépend du flux de génération initial, mais plus du tout après sérialisation de la carte."
  },
  {
    title: "Retournement des mines et fermes neutres",
    illustrationKey: "public-building-flip",
    system: "Carte",
    lawUse: "Uniforme discrète sur les masques de symétrie",
    variable: L`F \in \{0,1,2,3\}`,
    phenomenon:
      "Active ou non le retournement horizontal et vertical des bâtiments publics.",
    parameters: ["0 = aucun retournement", "1 = horizontal", "2 = vertical", "3 = double retournement"],
    why:
      "Les masques de symétrie disponibles sont équiprobables dès lors qu'on ne veut pas marquer de biais visuel. Les quatre états forment un groupe d'isométries planaires, identité, réflexion horizontale, réflexion verticale, double réflexion, qui sont structurellement équivalents sur un plateau de jeu. Tout biais de pondération créerait une asymétrie visuelle injustifiée entre les exemplaires du même bâtiment public sur la carte.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` et transmet le masque à la footprint du bâtiment.",
    parameterChoice:
      "Les deux axes (horizontal, vertical) sont indépendants l'un de l'autre, ce qui génère exactement `2² = 4` combinaisons : aucun retournement, horizontal seul, vertical seul, double retournement. Aucune de ces quatre configurations ne confère d'avantage directionnel sur la carte, donc `P(F = f) = 1/4 = 25 %` pour chaque masque. Le codage en 2 bits est la représentation minimale qui couvre les quatre états sans redondance ni paramètre supplémentaire.",
    dependence:
      "Statiquement dérivé de la génération de carte."
  },
  {
    title: "Choix de position des bâtiments publics",
    illustrationKey: "public-building-position",
    system: "Carte",
    lawUse: "Uniforme conditionnelle sur le haut du classement de dispersion",
    variable: L`X \mid X \in A_{top}`,
    phenomenon:
      "Sélectionne l'origine d'une mine ou d'une ferme dans un sous-ensemble des meilleurs candidats, classés selon leur score de dispersion spatiale.",
    parameters: [
      L`|A_{top}| = \min\left(n, \max\left(3, \left\lceil \frac{n}{6} \right\rceil\right)\right)`,
      "les candidats sont notés puis triés par score de distance"
    ],
    why:
      "Un pur optimum rendrait la carte trop déterministe; un tirage uniforme parmi les meilleurs candidats conserve la qualité géométrique sans figer la même configuration. Choisir systématiquement la meilleure position selon le score de dispersion produirait exactement la même carte pour tout monde donné, ce qui est contraire à l'objectif de renouvellement des parties. La stratégie retenue consiste à ne garder qu'un sous-ensemble des K meilleurs candidats, puis à tirer uniformément à l'intérieur de ce groupe : on préserve ainsi la qualité en n'acceptant que des positions bien dispersées, tout en laissant de la variété à l'intérieur de cet ensemble de qualité.",
    simulation:
      "`selectDispersedCandidate` trie les candidats, calcule `topCount`, puis tire uniformément un index dans ce sous-ensemble.",
    parameterChoice:
      "Le sous-ensemble de qualité `K = min(n, max(3, ⌈n/6⌉))` retient environ un sixième des candidats : pour 60 candidats admissibles cela donne `K = 10`, soit les 10 meilleures positions, ni une seule position (déterministe), ni la moitié (trop de bruit). Le plancher à 3 préserve un minimum de variété même quand peu de candidats sont disponibles, par exemple en fin de génération avec peu de place libre. Le score `3.5 × dist_existants + 2.0 × dist_bord + 0.35 × dist_centre` encode trois objectifs distincts : la dispersion entre bâtiments publics (coefficient 3.5, objectif principal), l'éloignement du bord pour éviter les placements en coin peu accessibles (coefficient 2.0), et une légère attraction vers le centre (coefficient 0.35, rôle secondaire d'équilibrage). Le coefficient de distance au bord est délibérément inférieur à celui de la dispersion pour ne pas sacrifier la répartition spatiale au profit d'une attraction trop forte vers le centre.",
    dependence:
      "Conditionne par les placements déjà retenus, donc fortement dépendante de l'historique de génération."
  },
  {
    title: "Apparition des rois",
    illustrationKey: "kingdom-spawn-zones",
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
      "Les deux royaumes obéissent à la même logique de tirage conditionnel; la bonne lecture est donc une uniforme discrète sur deux supports latéraux symétriques, pas deux mécanismes différents. Les bandes latérales garantissent une distance initiale minimale entre les deux rois, ce qui est une contrainte de jouabilité, pas une préférence esthétique. À l'intérieur de chaque bande, l'uniforme est le seul choix qui n'avantage structurellement ni les coins ni le centre, préservant ainsi l'équité entre parties.",
    simulation:
      "Le générateur collecte les cellules valides de chaque bande latérale, puis tire un index uniforme dans le vecteur de candidats du royaume concerné.",
    parameterChoice:
      "La zone de départ de `25 %` (paramètre `player_spawn_zone_percent`) est un compromis entre deux contraintes : suffisamment large pour offrir de la variété dans les positions initiales, suffisamment restreinte pour garantir que les deux rois démarrent côté opposé et ne soient jamais proches. En dessous de `10 %`, l'ensemble des candidats admissibles est trop petit et les configurations se répètent ; au-dessus de `40 %`, les deux zones se chevauchent et la contrainte de séparation peut ne plus être satisfiable. La symétrie des bandes (`25 %` de chaque côté) est intentionnelle : elle évite d'introduire un avantage structurel entre le camp blanc et le camp noir. L'uniforme sur les cases admissibles de chaque bande donne une probabilité égale à toute position valide, ni attraction vers les coins, ni vers le centre.",
    dependence:
      "Dépend du terrain déjà généré, donc du couple `worldSeed` + champs procéduraux, et de la contrainte de séparation entre royaumes."
  },
  {
    title: "Bord diagonal d'entrée du brouillard",
    illustrationKey: "weather-front-diagonal-entry",
    system: "Météo",
    lawUse: "Uniforme discrète sur les deux bords compatibles avec la diagonale",
    variable: L`E \in \{e_1,e_2\}`,
    phenomenon:
      "Pour les directions diagonales, choisit lequel des deux bords du plateau sert de bord d'entrée effectif.",
    parameters: ["2 bords admissibles par direction diagonale"],
    why:
      "Les deux constructions géométriques possibles sont symétriques; une loi uniforme conserve cette symétrie. Pour une direction diagonale donnée, les deux bords d'entrée compatibles produisent des brouillards géométriquement équivalents par réflexion : favoriser l'un plutôt que l'autre introduirait un biais directionnel injustifiable. La loi choisie est ici une Bernoulli uniforme (p = 0,5), qui est le cas minimal d'une catégorielle à deux poids égaux.",
    simulation:
      "La fonction `randomElement` appelle `std::uniform_int_distribution<int>(0, 1)`.",
    parameterChoice:
      "Pour une direction diagonale, les deux bords compatibles produisent des trajectoires géométriquement symétriques : choisir l'un plutôt que l'autre sans justification de gameplay biaiserait les entrées de brouillard vers un quadrant du plateau. La loi est ici une Bernoulli à `p = 0.5`, cas dégénéré d'une catégorielle à deux poids égaux, et seule distribution sans biais sur un choix binaire symétrique. Le support est de taille 2 et les deux états ont des probabilités égales : aucun paramètre supplémentaire n'est justifiable.",
    dependence:
      "Dépend de la direction du brouillard, elle-même tirée juste avant."
  },
  {
    title: "Couverture cible du brouillard",
    illustrationKey: "weather-front-coverage",
    randomnessKind: "density",
    system: "Météo",
    lawUse: "Uniforme continue sur un intervalle de pourcentage",
    variable: L`C \sim \mathcal{U}([0.05, 0.20])`,
    phenomenon:
      "Fixe la proportion de cellules visibles que le nouveau brouillard doit recouvrir à sa naissance.",
    parameters: ["`coverage_min_percent = 5`", "`coverage_max_percent = 20`"],
    why:
      "Aucune taille privilégiée n'est imposée entre les bornes retenues; l'uniforme donne un éventail large mais lisible. Une Beta aurait permis de concentrer la masse vers une taille typique, mais l'objectif de design est justement d'éviter un gabarit répétitif : chaque brouillard doit pouvoir couvrir entre 5 % et 20 % du plateau avec autant de chances pour toute taille dans cet intervalle. Les bornes elles-mêmes sont les vraies contraintes de gameplay : en dessous de 5 %, le brouillard ne joue aucun rôle tactique ; au-dessus de 20 %, il occulterait trop de pièces pour rester lisible.",
    simulation:
      "Le runtime tire un entier uniforme entre 5 et 20, puis convertit ce pourcentage en aire cible.",
    codeSnippet:
`// WeatherSystem.cpp
std::uniform_int_distribution<int> coverageDist(
    config.getWeatherCoverageMinPercent(),
    config.getWeatherCoverageMaxPercent());
int coveragePercent = coverageDist(generator); // ∈ [5, 20]`,
    simulationFromUniform:
      "La transformation affine `x = a + (b − a) · U` où `U ∈ [0, 1]` donne directement une uniforme continue sur `[a, b]`. Ici la STL tire un entier dans `[5, 20]` par la même idée discrétisée : `k = a + floor((b − a + 1) · U)`. Pas de méthode d'inversion complexe nécessaire, c'est la définition même de la loi uniforme.",
    parameterChoice:
      "La borne basse `5 %` correspond au seuil en dessous duquel le brouillard n'occulte plus suffisamment de pièces pour avoir un effet tactique : sur un plateau de 8×8, moins de 3–4 cases masquées est trop anecdotique pour influencer les décisions. La borne haute `20 %` correspond à environ 10 cases sur 64 ; au-delà, le brouillard commence à masquer plus d'un tiers de l'espace visible d'un joueur, nuisant à la lisibilité sans apport tactique supplémentaire. L'uniforme sur `[5, 20]` est cohérente avec l'intention de ne pas avoir de taille de brouillard typique récurrente : chaque valeur dans cet intervalle est aussi désirable qu'une autre du point de vue du design, sans taille préférée à concentrer.",
    dependence:
      "Se combine ensuite avec le rapport d'aspect et la direction pour construire la géométrie finale."
  },
  {
    title: "Allongement du brouillard",
    illustrationKey: "weather-front-aspect-ratio",
    randomnessKind: "density",
    system: "Météo",
    lawUse: "Uniforme continue sur un intervalle borné",
    variable: L`A \sim \mathcal{U}([1.80, 2.60])`,
    phenomenon:
      "Fixe l'allongement principal du brouillard avant déformation par le bruit de contour.",
    parameters: ["`aspect_ratio_min_times_100 = 180`", "`aspect_ratio_max_times_100 = 260`"],
    why:
      "Le brouillard doit rester anisotrope sans toujours avoir la même excentricité; une plage uniforme contrôle cette variété. Une normale tronquée aurait concentré la masse vers un rapport d'aspect central, produisant un profil de bande standard trop reconnaissable. L'uniforme est ici plus appropriée car elle ne favorise aucun gabarit particulier à l'intérieur des bornes retenues, ce qui est cohérent avec l'absence de forme préférée dans le design météo.",
    simulation:
      "Le code tire un entier uniforme sur [180, 260], divise par 100, puis dérive `radiusAlong` et `radiusAcross` à aire préservée.",
    parameterChoice:
      "La borne basse `1.8` assure que le brouillard a une forme de bande reconnaissable : un rapport 1.8:1 est suffisamment allongé pour signaler visuellement une direction de déplacement. En dessous de `1.5`, le brouillard tend vers un disque, effaçant l'anisotropie intentionnelle. La borne haute `2.6` maintient une forme de bande sans basculer vers un corridor quasi-linéaire : au-dessus de `3.0`, la largeur transversale devient si faible que le brouillard ressemble à une ligne, ce qui serait graphiquement trop rigide. L'uniforme sur `[1.80, 2.60]` évite un rapport d'aspect typique répété : la variété de gabarits reste large sans introduire de forme canonique reconnaissable à chaque partie.",
    dependence:
      "Partage la même seed d'événement que la couverture et les graines de contour/densité du brouillard courant."
  },
  {
    title: "Choix d'un mouvement aléatoire en phase Searching",
    illustrationKey: "infernal-searching-random-move",
    system: "Pièces du diable",
    lawUse: "Uniforme discrète sur les coups admissibles",
    variable: L`M \sim \mathcal{U}_d(A_{moves})`,
    phenomenon:
      "Chaque pièce du diable choisit d'abord un royaume cible et garde ce royaume comme cible globale. Tant qu'elle voit des pièces de ce royaume, elle les chasse normalement. Si toutes ces pièces deviennent temporairement invisibles, par exemple parce qu'elles sont sous un brouillard, la pièce du diable perd sa cible précise sans pouvoir en reprendre une autre tout de suite. Elle passe alors en phase `Searching` : elle continue de viser le même royaume, mais comme elle ne voit plus personne, elle erre provisoirement avec des déplacements aléatoires. Dès qu'une pièce du royaume réapparaît à sa vue, elle repasse en `Hunting`; s'il n'existe plus de proie valable, elle bascule en `Returning`.",
    parameters: ["support = coups générés, puis filtres par la visibilité locale et les collisions interdites"],
    why:
      "Dans cet état, le code a explicitement renoncé à toute cible individuelle: la pièce n'optimise plus une trajectoire vers une proie identifiée, puisqu'elle n'en a temporairement plus. Introduire une pondération supplémentaire vers certaines directions serait contradictoire avec cette perte de contact et rendrait la phase moins lisible. Une uniforme discrète est donc la lecture correcte: une fois l'errance autorisée, tous les coups encore admissibles doivent être traités à égalité, sinon on réintroduit en cachette une heuristique de chasse dans une phase qui n'est plus de la chasse.",
    simulation:
      "Le code génère tous les coups, filtre ceux qui violent la logique de visibilité, puis tire un index uniforme.",
    parameterChoice:
      "L'ensemble des coups admissibles est filtré en amont (visibilité, collisions) : tous les mouvements restants ont franchi les mêmes contraintes et aucun n'est structurellement préférable pendant la phase d'errance. Introduire une pondération directionnelle dans cet état reviendrait à implémenter subrepticement une heuristique de chasse dans une phase qui a explicitement renoncé à toute cible individuelle. La taille du support `|A_moves|` varie chaque tour selon la position et la visibilité, mais l'uniformité sur ce support est maintenue quelle que soit cette taille : si un seul coup est admissible, il est choisi avec probabilité 1.",
    dependence:
      "Conditionné par l'entrée préalable en phase `Searching`, par la Bernoulli qui autorise ou non l'errance ce tour-ci, et par l'ensemble de coups encore admissibles après filtrage."
  },
];

const permutationProcesses = [
  {
    title: "Ordre de placement des mines et fermes neutres",
    illustrationKey: "public-building-order",
    system: "Carte",
    lawUse: "Permutation uniforme sur les objets a placer",
    variable: L`\Pi \in \mathfrak{S}_n`,
    phenomenon:
      "Mélange l'ordre dans lequel les fermes et les mines sont insérées sur la carte avant choix de position.",
    parameters: ["`n = num_mines + num_farms = 5` dans la config actuelle"],
    why:
      "Une permutation uniforme supprime un biais systématique du type ‘les mines sont toujours placées avant les fermes’. Le placement est séquentiel et glouton : chaque objet occupe la meilleure position disponible au moment de son insertion, ce qui signifie que les premiers objets placés ont accès à un plus grand nombre de positions de qualité. Sans mélange, un ordre fixe avantagerait structurellement les mines à chaque génération. La permutation uniforme redistribue cet avantage de façon équitable sur l'ensemble des parties générées.",
    simulation:
      "Le générateur appelle `std::shuffle(placements.begin(), placements.end(), random)` avant la boucle de placement.",
    codeSnippet:
`// BoardGenerator.cpp
addPlacements(BuildingType::Mine, config.getNumMines());
addPlacements(BuildingType::Farm, config.getNumFarms());
// Mélange uniforme de Fischer-Yates (O(n)) :
std::shuffle(placements.begin(), placements.end(), random);`,
    parameterChoice:
      "La permutation porte sur l'ensemble complet des `n = 5` objets (`num_mines = 2 + num_farms = 3`) : toutes les ordinations possibles ont la même probabilité `1/5! = 1/120`. Une permutation partielle aurait laissé les derniers objets toujours défavorisés dans le même ordre relatif ; le mélange complet est la seule garantie d'équité absolue entre mines et fermes sur l'ensemble des parties générées. La valeur `n = 5` découle directement de la config (`num_mines + num_farms`) et se répercute automatiquement si l'un des deux comptes change. L'algorithme de Fisher-Yates utilisé par `std::shuffle` produit cette permutation uniforme en `O(n)` sans biais.",
    dependence:
      "Influe indirectement sur tout le reste du placement public car les meilleurs candidats changent après chaque insertion."
  }
];

const categoricalProcesses = [
  {
    title: "Case d'apparition d'un coffre",
    illustrationKey: "chest-spawn-cell",
    system: "Coffres",
    lawUse: "Catégorielle pondérée sur les cellules admissibles",
    variable: L`C \in \{c_1,\dots,c_n\}`,
    phenomenon:
      "Choisit la cellule de réapparition d'un coffre parmi les cases libres et suffisamment éloignées des deux rois.",
    parameters: [
      L`w(c)=1+\mathrm{centrality}(c)+\mathrm{contestation}(c)`,
      "`min_distance_from_kings = 6`",
      "centralité mesurée par la distance au centre, contestation par l'équilibre des distances aux deux rois"
    ],
    why:
      "Le système veut privilégier les zones contestables et centrales, pas seulement tirer une case uniforme. Une uniforme sur les cases admissibles placerait les coffres avec autant de probabilité dans les coins isolés que sur les cases stratégiquement disputées, ce qui réduirait leur impact sur la dynamique de jeu. La catégorielle pondérée permet d'encoder explicitement la valeur tactique d'une position sans rendre le résultat déterministe : même les cases faibles ont une probabilité non nulle d'être sélectionnées, ce qui conserve une part de surprise.",
    simulation:
      "Le runtime calcule un poids entier pour chaque candidat puis utilise `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Le terme `+1` dans le poids `1 + centrality(c) + contestation(c)` est un plancher inconditionnel : il garantit qu'une case admissible à centralité et contestation nulles (coin éloigné) conserve toujours une probabilité positive d'être choisie, préservant la surprise. La distance minimale `min_distance = 6` évite que le coffre apparaisse à portée immédiate d'un roi ; à 6 cases, les deux rois doivent déjà parcourir plusieurs mouvements pour l'atteindre, créant une fenêtre de compétition. La centralité incline la distribution vers le centre du plateau, là où les chemins des deux royaumes se croisent le plus souvent. La contestation poids sur les cases équidistantes des deux rois, amplifiant la dynamique de compétition pour la collecte. L'addition des deux termes (plutôt que leur produit) préserve un signal positif même quand l'un est nul : une case centrale mais non-contestée reste attractrice.",
    dependence:
      "Dépend de la position instantanée des rois et de l'occupation du plateau au moment de l'apparition."
  },
  {
    title: "Type de récompense du coffre",
    illustrationKey: "chest-reward-type",
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
      "Le système favorise **plus d'or très tôt**, puis **davantage de capacité d'action** ensuite; une catégorielle pondérée est la loi naturelle pour ce genre de choix nominal. Le type de récompense est une variable nominale : or, mouvement ou construction ne sont pas des valeurs numériques ordonnées mais des catégories aux effets radicalement différents sur le gameplay. Imposer une récompense fixe aurait rendu les coffres prévisibles et stratégiquement triviaux ; l'uniforme aurait ignoré la progression temporelle voulue par le design. Le changement de poids au tour 10 modélise explicitement la transition de phase : en début de partie, l'or est la ressource la plus flexible ; passé le milieu de partie, augmenter les budgets d'action est plus impactant.",
    simulation:
      "`sampleReward` construit le vecteur de poids selon le tour courant, puis appelle `std::discrete_distribution<int>`.",
    codeSnippet:
`// ChestLootProgression.cpp
const std::array<int, 3> weights{
    lateGame ? config.getChestLateGoldWeight()
             : config.getChestEarlyGoldWeight(),       // [0] or
    lateGame ? config.getChestLateMovementBonusWeight()
             : config.getChestEarlyMovementBonusWeight(), // [1] mouvement
    lateGame ? config.getChestLateBuildBonusWeight()
             : config.getChestEarlyBuildBonusWeight()};  // [2] construction
std::discrete_distribution<int> dist(weights.begin(), weights.end());
switch (dist(generator)) {
    case 1: return { MovementPointsMaxBonus, ... };
    case 2: return { BuildPointsMaxBonus, ... };
    default: return { Gold, sampleGoldRewardAmount(...) };
}`,
    parameterChoice:
      "En début de partie (tours < 10), le rapport `8:3:3` donne à l'or environ `57 %` de probabilité contre `21,5 %` pour chacun des deux bonus d'action. L'or est plus utile tôt car les joueurs n'ont pas encore de budget d'action suffisant pour rentabiliser immédiatement un bonus de mouvement ou de construction. Après le tour 10, le rapport `4:6:6` inverse cette logique : chaque bonus d'action passe à `37,5 %` contre `25 %` pour l'or, accélérant la montée en puissance des royaumes. Le seuil `late_game_turn = 10` correspond empiriquement à la phase où les deux royaumes sont déployés et où les actions supplémentaires ont un impact immédiat. Le mode de rattrapage (`catch_up_enabled = true`) partage la même récompense courante entre les deux royaumes jusqu'à ce qu'ils l'aient tous deux récoltée, évitant qu'un joueur rapide accumule systématiquement un avantage de type de récompense.",
    dependence:
        "Si le mode de rattrapage est actif, **le tirage suivant n'apparaît que lorsque les deux royaumes ont déjà pris la récompense courante**; les ouvertures de coffres des deux camps restent donc liées par une même récompense partagée tant qu'elle n'a pas été consommée des deux côtés."
  },
  {
    title: "Direction du brouillard",
    illustrationKey: "weather-front-direction",
    system: "Météo",
    lawUse: "Catégorielle pondérée sur huit directions",
    variable: L`D \in \{N,S,E,W,NE,NW,SE,SW\}`,
    phenomenon:
      "Choisit la direction cardinale ou diagonale du prochain brouillard.",
    parameters: ["les huit poids valent actuellement 1."],
    why:
      "Le système est écrit de façon générique pour pouvoir biaiser certaines directions plus tard, mais la configuration active réalise une équiprobabilité via une catégorielle à poids égaux. Utiliser `std::discrete_distribution` avec des poids unitaires plutôt qu'une simple uniforme discrète est un choix d'extensibilité : il suffit de modifier les valeurs dans la config pour favoriser certains axes sans toucher au code. Dans l'état actuel, toutes les directions sont équiprobables parce qu'aucun déséquilibre cardinal n'a été identifié comme désirable dans le gameplay.",
    simulation:
      "Le runtime lit `direction_weights`, puis passe le tableau d'entiers a `std::discrete_distribution<int>`.",
    parameterChoice:
      "Les huit poids unitaires font de cette catégorielle une uniforme déguisée : toutes les directions cardinales et diagonales sont équiprobables car aucun déséquilibre cardinal n'a été identifié comme désirable dans le gameplay. L'utilisation de `std::discrete_distribution` avec des poids entièrement configurables est un choix d'extensibilité : modifier les valeurs dans la config suffit pour biaiser certains axes à l'avenir sans toucher au code. Le support à 8 états couvre la totalité des directions admissibles sur grille avec diagonales ; aucune direction n'est exclue a priori.",
    dependence:
      "La direction pilote ensuite le bord d'entrée, la trajectoire et les rayons du brouillard."
  },
  {
    title: "Type de cible primaire d'une pièce du diable",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée sur les types de pièces visibles",
    variable: L`T \in \{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    phenomenon:
      "Quand une pièce du diable repère plusieurs types de pièces dans le royaume qu'elle poursuit, cette loi détermine quelle famille elle considère comme sa proie prioritaire pour la suite de la chasse. Elle ne choisit donc pas encore une pièce précise ni une case d'apparition, mais d'abord la catégorie de cible à privilégier, par exemple pion, cavalier ou reine.",
    parameters: [
      "poids actifs: pawn 8, knight 14, bishop 14, rook 26, queen 38",
      "les types absents du champ visible recoivent le poids 0"
    ],
    why:
      "Le comportement cherche une priorisation stratégique explicite: les pièces majeures doivent être attirées plus souvent que les pions. Un choix déterministe (toujours cibler la pièce de plus haute valeur visible) rendrait la pièce du diable triviale à contrer : il suffirait de masquer ou de déplacer la reine. La catégorielle pondérée introduit une variabilité contrôlée : la pièce du diable cible le plus souvent les pièces majeures, mais peut aussi s'en prendre à un cavalier ou un pion, ce qui complique l'anticipation du joueur. Les poids sont calibrés pour que la hiérarchie tactique soit respectée en espérance sans être garantie à chaque tour.",
    simulation:
      "Le système construit `typeWeights`, met à zéro les types indisponibles, puis tire via `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Les poids `(8, 14, 14, 26, 38)` reproduisent approximativement la hiérarchie de valeur aux échecs (pion ≈ 1, cavalier/fou ≈ 3, tour ≈ 5, reine ≈ 9) en les resserrant : la reine est `38/8 ≈ 4.75×` plus probable que le pion, mais reste non certaine, ce qui empêche une contre-stratégie triviale. Les poids du cavalier et du fou sont identiques (`14`) car ils ont la même valeur tactique dans ce contexte. Les types absents du champ visible reçoivent le poids `0`, rendant la distribution adaptive à la composition visible du plateau : si la reine n'est pas visible, son poids disparaît et les pièces mineures absorbent la probabilité restante.",
    dependence:
      "Dépend de la visibilité courante et des types réellement présents chez le royaume cible."
  },
  {
    title: "Option d'apparition ciblée d'une pièce du diable",
    illustrationKey: "infernal-targeted-spawn-option",
    system: "Pièces du diable",
    lawUse: "Catégorielle pondérée par proximité de chemin",
    variable: L`O \in \{o_1,\dots,o_m\}`,
    phenomenon:
      "Une fois la cible repérée, le système recense plusieurs points d'entrée possibles sur le bord du plateau pour faire apparaître la pièce du diable. Cette loi choisit laquelle de ces options d'apparition sera utilisée, en privilégiant celles qui mènent plus vite vers la cible sans rendre l'entrée complètement prévisible.",
    parameters: [
      L`w(o)=\max\bigl(1, 2D-\mathrm{dist}(o)+1\bigr)`,
      "`D = board.getDiameter()`"
    ],
    why:
      "On veut favoriser les options qui approchent vite la cible sans pour autant annuler complètement les autres entrées valides. Une apparition déterministe sur la case de bord la plus proche de la cible rendrait la trajectoire de la pièce du diable entièrement prévisible après identification de la cible. À l'inverse, une uniforme sur toutes les entrées produirait des apparitions incohérentes avec la cible annoncée. La catégorielle par proximité de chemin est le compromis : l'entrée optimale est la plus probable, mais une entrée sous-optimale peut être sélectionnée, ce qui introduit une variabilité tactique sans dénaturer l'intention du comportement.",
    simulation:
      "Le runtime calcule un poids entier inversement lié à la distance de plus court chemin, puis tire avec `std::discrete_distribution`.",
    parameterChoice:
      "La formule `2D − dist(o) + 1` est une décroissance linéaire de la distance : quand `dist(o) = 0`, le poids vaut `2D + 1`; quand `dist(o) = 2D`, le poids vaut `1`. Le facteur `2` dans `2D` choisit un gradient modéré : l'option optimale est environ `2D + 1` fois plus probable que la pire, soit un rapport d'environ 13 pour un diamètre de 6, fort mais pas absolu. Ce gradient favorise les entrées proches sans annuler les options éloignées, préservant la variabilité tactique. Le plancher `max(1, ...)` est indispensable pour garantir une probabilité non nulle à chaque option admissible restante : un poids nul exclurait définitivement les entrées lointaines et rendrait la pièce du diable trop prévisible. `D = board.getDiameter()` sert d'échelle naturelle : en normalisant la distance par `D`, le gradient reste visuellement comparable quelle que soit la taille du plateau.",
    dependence:
      "Conditionné par le type de pièce infernale manifestée et par le graphe de déplacements accessible."
  },
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
      "Une Bernoulli suffit dès qu'il n'y a que deux royaumes éligibles; l'état du jeu déforme directement la probabilité. Avec deux issues possibles, la Bernoulli est la famille minimale et canonique : aucune loi plus complexe ne serait justifiable ici. Le paramètre `p_t` n'est pas fixe mais calculé à partir des dettes de sang normalisées, ce qui transforme ce tirage binaire en mécanisme actif de rééquilibrage : le royaume le plus fragilité attire davantage les pièces du diable, amplifiant la pression sur le perdant tout en préservant une chance de surprise pour le gagnant. Ce mécanisme est intentionnellement non stationnaire : `p_t` évolue avec les pertes.",
    simulation:
      "Le code calcule `whiteProbability`, la borne dans `[0,1]`, puis appelle `std::bernoulli_distribution`.",
    codeSnippet:
`// InfernalSystem.cpp
const int totalDebt = state.whiteBloodDebt + state.blackBloodDebt;
const double p = (totalDebt > 0)
    ? (double)state.whiteBloodDebt / totalDebt
    : 0.5;
std::bernoulli_distribution dist(std::clamp(p, 0.0, 1.0));
return dist(generator) ? KingdomId::White : KingdomId::Black;`,
    parameterChoice:
      "La proportionnalité directe à la dette (`p_t = dette_blanche / (dette_blanche + dette_noire)`) est le choix le plus réactif : dès qu'un camp subit plus de pertes, la pression des pièces du diable s'oriente en proportion vers lui, sans paramètre de sensibilité à calibrer. Une fonction saturée (logistique ou sigmoïde) aurait atténué les extrêmes, maintenant une pression plus équilibrée en cas de fort déséquilibre ; ici le design préfère amplifier les écarts pour accentuer la dramaturgie de fin de partie. Le cas de secours `p_t = 0.5` quand la dette totale est nulle préserve la symétrie initiale : sans information sur le déséquilibre, les deux royaumes restent équiprobables.",
    dependence:
      "Très dépendante de l'historique des destructions, donc non stationnaire sur une partie."
  },
  {
    title: "Activation d'un mouvement aléatoire en phase Searching",
    system: "Pièces du diable",
    lawUse: "Bernoulli simple",
    variable: L`B \sim \mathrm{Bernoulli}(0.333)`,
    phenomenon:
      "Pendant l'état `Searching` décrit ci-dessus, décide à chaque tour si la pièce du diable effectue réellement un déplacement d'errance aléatoire ce tour-ci, ou si elle reste sur place en attendant de retrouver une cible visible qui la ferait repasser en `Hunting`. Cette Bernoulli n'intervient donc ni pendant la chasse normale, ni pendant le retour au bord, mais seulement pendant cette phase intermédiaire de perte de contact avec la proie.",
    parameters: ["probabilité de 33,3 % (`searching_random_move_chance_times_1000 = 333`)"],
    why:
      "Il s'agit d'un interrupteur oui/non sur une branche comportementale unique; la Bernoulli est donc la loi minimale adéquate. La décision est binaire par construction : soit la pièce erre ce tour-ci, soit elle n'erre pas et laisse la priorité à une éventuelle reacquisition de cible au tour suivant. Une probabilité d'environ 1/3 a été retenue pour calibrer l'erraticité perçue : trop haute, la pièce devient chaotique et perd sa lisibilité ; trop basse, la phase `Searching` devient presque invisible dans la partie. L'implémentation via un entier uniforme sur [0, 999] comparé à un seuil de 333 est mathématiquement équivalente à `std::bernoulli_distribution(0.333)` mais plus pratique à régler depuis la config.",
    simulation:
      "Le système tire un entier uniforme sur `[0, 999]` et compare au seuil 333, ce qui réalise une Bernoulli discrétisée.",
    parameterChoice:
      "Avec `p = 1/3`, la pièce effectue en espérance `1 mouvement aléatoire tous les 3 tours` en phase `Searching`, ce qui maintient une pression perceptible sans que l'errance domine le retour potentiel en `Hunting`. Une probabilité plus haute (1/2) rendrait la phase trop chaotique et masquerait la lisibilité du comportement ; une probabilité plus basse (1/5) ferait de la phase `Searching` un non-événement presque invisible dans la partie. La discrétisation via un entier uniforme sur `[0, 999]` comparé au seuil `333` est mathématiquement équivalente à `Bernoulli(0.333)` mais plus pratique à configurer.",
    dependence:
      "Conditionné par l'entrée préalable en phase `Searching` et par l'existence d'au moins un coup admissible après filtrage; si une cible visible est retrouvée avant cela, la pièce repasse en `Hunting` et cette Bernoulli ne s'applique plus."
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
      "La conception veut un compteur d'arrivées rares dont l'intensité croît avec la dette de sang; la Poisson est le modèle naturel de comptage d'événements. Une simple Bernoulli à paramètre fixe aurait suffi pour déclencher des apparitions, mais elle ne permettrait pas d'encoder que l'intensité doit croître avec les destructions accumulees : il faudrait recalculer `p` à chaque tour de façon ad hoc. La Poisson formalise directement ce mécanisme : son paramètre λ est interprétable comme une fréquence d'arrivée, et faire dépendre λ de la dette de sang donne un modèle cohérent où les apparitions restent rares en début de partie et s'accélèrent avec l'escalade du conflit. Une loi géométrique (sans mémoire, discrète) aurait été envisageable pour les inter-arrivées, mais elle n'aurait pas permis de borner aussi proprement la probabilité d'apparition par tour via le cap sur λ.",
    simulation:
      "Le runtime échantillonne `std::poisson_distribution<int>(lambda)` puis déclenche l'apparition si le résultat est au moins 1.",
    codeSnippet:
`// InfernalSystem.cpp
const double lambda = std::clamp(
    lambdaBase + lambdaPerDebt * (double)(whiteDebt + blackDebt),
    0.0, lambdaCap);
std::poisson_distribution<int> dist(lambda);
if (dist(generator) < 1)
    return std::nullopt;  // N = 0 : aucune apparition ce tour`,
    parameterChoice:
      "La base `lambda_base = 0.02` correspond à une probabilité d'apparition d'environ `2 %` en tout début de partie (dette nulle) : les pièces du diable sont rares dès le premier tour mais jamais impossibles. La pente `lambda_debt = 0.012` par point de dette fixe l'escalade : 10 unités de dette portent `λ` à `0.14`, soit `P(N ≥ 1) ≈ 13 %`, la partie devient sensiblement dangereuse sans être encore saturée. Le cap `lambda_max = 0.25` borne `P(N ≥ 1) = 1 − exp(−0.25) ≈ 22.1 %` : même en fin de partie très déséquilibrée, moins d'un tour sur quatre déclenche une apparition, laissant les joueurs respirer. L'escalade linéaire `0.02 + 0.012 × dette` est intentionnellement simple : elle est lue directement dans la config et garantit une progression prévisible et réglable sans courbe cachée.",
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
      "Attribue l'XP pour les kills, la destruction de bloc et le gain passif d'arène (une pièce gagne de l'XP chaque tour passé dans une arène).",
    parameters: [
      "kill_pawn: mean 20, sigma = 3.6, clamp = +/- 7.2, minimum = 1",
      "kill_knight / kill_bishop: mean 50, sigma = 8, clamp = +/- 16, minimum = 1",
      "kill_rook: mean 100, sigma = 12, clamp = +/- 24, minimum = 1",
      "kill_queen: mean 300, sigma = 30, clamp = +/- 60, minimum = 1",
      "destroy_block / arena_per_turn: mean 10, sigma = 1.5, clamp = +/- 3, minimum = 1"
    ],
    why:
      "Une variable gaussienne tronquée préserve une moyenne intuitive tout en autorisant une dispersion contrôlée autour de chaque source d'XP. La loi normale est choisie parce que son paramétrage en (μ, σ) traduit directement le langage du design : « cette action rapporte en moyenne μ points, avec une variabilité de σ ». Une loi exponentielle ou log-normale introduirait une asymétrie non voulue, des valeurs très hautes plus probables que très basses, inappropriée pour une récompense centrée sur une valeur cible précise. La **troncature** est indispensable pour deux raisons : éliminer les valeurs négatives ou nulles, incohérentes avec une récompense, et éviter les pics extrêmes qui déstabiliseraient l'économie d'XP. Le minimum final (plancher à 1) garantit qu'un kill rapporte toujours quelque chose même après arrondi.",
    simulation:
      "`RewardProfileSampling::sampleTruncatedNormal` tire une normale, la tronque sur `[mean - delta, mean + delta]`, puis arrondit et applique le minimum.",
    codeSnippet:
`// RewardProfileSampling.hpp
const double sigma = std::max(1.0, mean * sigmaMultiplier);
const double delta = sigma * clampMultiplier;
// Normale centrée sur mean, tronquée dans [mean-delta, mean+delta] :
std::normal_distribution<double> dist(mean, sigma);
const double x = std::clamp(dist(gen), mean - delta, mean + delta);
return std::max(minimum, static_cast<int>(std::lround(x)));`,
    simulationFromUniform:
      "La STL implémente la normale par l'algorithme de **Box-Muller** : à partir de deux uniformes `U1, U2 ∈ (0,1)`, on calcule `Z = sqrt(−2 ln U1) · cos(2π U2)`, puis `X = μ + σZ`. Si X sort de `[a, b]` (troncature), on recommence (`rejection sampling`). Cette méthode tire deux uniformes pour produire deux valeurs gaussiennes indépendantes simultanément.",
    parameterChoice:
      "La moyenne `μ` de chaque profil encode directement la valeur cible du design : 20 pour un kill de pion, 50 pour un cavalier ou un fou, 100 pour une tour, 300 pour une reine, 10 pour les sources passives. La dispersion est calibrée de façon inversement proportionnelle à la valeur : le coefficient `σ/μ` est plus élevé pour les petites sources (pion et passifs ≈ 18 %, cavalier/fou ≈ 16 %) que pour les grosses (reine ≈ 10 %), ce qui maintient une variabilité perçue similaire à toutes les échelles sans créer d'écarts absolus déstabilisants. La troncature à `±2σ` autour de la moyenne évite les valeurs extrêmes sans recourir à un reject total. Le plancher à `1` garantit qu'un kill rapporte toujours au moins 1 XP après arrondi, même pour les profils à faible `μ`.",
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
      "La même famille que pour l'XP permet d'obtenir une valeur centrale stable, avec un peu de volatilité sans valeurs aberrantes gigantesques. Réutiliser le même moteur de normale tronquée assure la cohérence du système économique : les récompenses d'or et d'XP obéissent à la même philosophie de design, une valeur centrale de référence avec une dispersion contrôlée. La **troncature** est ici aussi nécessaire pour éviter des montants d'or négatifs ou des gains excessifs qui rompraient l'équilibre économique : un coffre donnant 0 ou 200 d'or produirait des effets de bord trop importants sur la stratégie du tour.",
    simulation:
      "Le chemin `sampleGoldRewardAmount -> sampleTruncatedNormal` réutilisé exactement le moteur commun de profils de récompense.",
    parameterChoice:
      "La moyenne `μ = 35` est calée sur l'économie de début de partie : un coffre rapporte en moyenne 35 d'or, ce qui est significatif mais non déstabilisant. Avec `σ = 6.3` (18 % de `μ`) et la troncature à `±2σ`, l'intervalle effectif est `[22.4, 47.6]`, soit une variabilité de ±36 % autour de la valeur centrale, perceptible par le joueur sans être déroutante. Le plancher `minimum = 1` est une sécurité formelle : le minimum de l'intervalle de troncature est 22.4 et l'arrondi donne au moins 22, donc ce plancher n'est jamais actif en pratique mais préserve la sémantique de 'toujours quelque chose'.",
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
      "La Weibull est adaptée aux temps d'attente flexibles: avec `k > 1`, le taux de risque croissant rend les réapparitions plus plausibles après plusieurs tours sans coffre. Une loi géométrique, l'équivalent discret naturel d'un temps d'attente, est sans mémoire : elle traite chaque tour comme un essai indépendant, sans que l'absence prolongée de coffre augmente la probabilité de réapparition. Ce comportement sans mémoire est contraire à l'intention de design : le joueur doit pouvoir percevoir qu'un coffre est attendu après une longue absence. La Gamma offre le même taux de risque croissant, mais la Weibull est ici préférable car sa paramétrisation (k, λ) sépare plus naturellement la forme (via k) de l'échelle temporelle (via λ). Le **plancher discrétisé** à c = 4 tours garantit qu'un coffre ne peut pas réapparaître immédiatement après avoir été collecté.",
    simulation:
      "`sampleSpawnDelay` échantillonne `std::weibull_distribution<double>(shape, scale)`, arrondit, puis applique `max(respawnCooldown, randomDelay)`.",
    codeSnippet:
`// ChestSystem.cpp
std::weibull_distribution<double> dist(shape, scale);
// shape = k = 1.8,  scale = λ = 6 tours
int delay = std::max(0, static_cast<int>(std::lround(dist(generator))));
return std::max(config.getChestRespawnCooldownTurns(), delay);`,
    simulationFromUniform:
      "La Weibull admet une **CDF inversible** en forme close : `F(t) = 1 − e^{−(t/λ)^k}`. La méthode de la transformée inverse donne directement `T = λ · (−ln U)^{1/k}` à partir d'une seule uniforme `U ∈ (0,1)`. C'est l'une des lois les plus simples à simuler par inversion exacte.",
    parameterChoice:
      "Le paramètre de forme `k = 1.8 > 1` donne à la Weibull un taux de risque croissant : plus le coffre tarde à réapparaître, plus la réapparition devient probable au tour suivant. C'est exactement l'inverse de la loi géométrique (sans mémoire) qui traiterait chaque tour de manière identique indépendamment de l'attente écoulée : ici l'absence prolongée d'un coffre augmente progressivement la pression de réapparition, ce qui est cohérent avec l'intention de gameplay. Le paramètre d'échelle `λ = 6` fixe `E[T] = 6 × Γ(1 + 1/1.8) ≈ 5.3 tours`, calibré pour que le coffre revienne en moyenne après 5 tours : assez vite pour rester central dans la stratégie, assez lentement pour qu'il ne soit pas trivial à planifier. Le plancher `c = 4` tours protège contre les réapparitions quasi immédiates générées par la queue gauche de la Weibull et donne aux joueurs le temps de s'éloigner avant la prochaine fenêtre.",
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
      "Une Gamma contrôle naturellement des temps d'attente positifs et asymétriques, plus souples qu'une exponentielle simple. Une loi exponentielle (cas k = 1 de la Gamma) est sans mémoire, ce qui produit des inter-arrivées irrégulières et potentiellement très courtes : deux brouillards pourraient s'enchaîner en quelques tours. Avec k > 1, la Gamma concentre la masse autour de sa moyenne tout en maintenant une queue droite : les délais typiques sont regroupés, mais des pauses plus longues restent possibles. Le paramètre θ encode l'échelle temporelle absolue et peut être ajusté dans la config pour accélérer ou ralentir toute la cadence météo sans modifier la forme de la distribution.",
    simulation:
      "`scheduleNextSpawn` appelle `sampleGammaTurns`, qui échantillonne `std::gamma_distribution`, prend le plafond puis convertit en pas de temps.",
    codeSnippet:
`// WeatherSystem.cpp
float sampleGammaTurns(std::mt19937& gen,
                       int minTurns, int shapeTimes100, int scaleTimes100) {
    const double k = shapeTimes100 / 100.0;  // k = 4.0
    const double θ = scaleTimes100 / 100.0;  // θ = 10.0
    std::gamma_distribution<double> dist(k, θ);
    return minTurns + (int)std::ceil(dist(gen));
}`,
    simulationFromUniform:
      "La STL utilise l'algorithme de **Marsaglia-Tsang** (2000) : pour `k ≥ 1`, on pose `d = k − 1/3`, `c = 1/√(9d)`, puis on tire `Z ~ N(0,1)` (via Box-Muller) et on forme `x = d(1 + cZ)³`. Le candidat est accepté avec probabilité `exp(x/d − 1 − ln(x/d)) · exp(−Z²/2)`. Ce test d'accept/reject donne un taux d'acceptation proche de 1 pour les paramètres courants.",
    parameterChoice:
      "Le paramètre de forme `k = 4` est le choix structurel clé : avec `k > 1`, la Gamma a un taux de risque croissant et une masse concentrée loin de zéro, ce qui évite les séquences de deux brouillards dos-à-dos fréquentes avec une exponentielle simple (`k = 1`). L'échelle `θ = 10` fixe directement les trois caractéristiques temporelles : espérance `E[T] = kθ = 40 tours`, mode `(k−1)θ = 30 tours`, écart-type `θ√k = 20 tours`. Cette cadence gravite autour de 40 tours, ce qui correspond à un brouillard environ toutes les 4–5 séquences typiques : assez rare pour être un événement notable, assez fréquent pour influencer la stratégie météo. Le minimum `m = 0` laisse la Gamma entière sans plancher : la queue gauche de `Γ(4, 10)` a déjà une masse très faible près de zéro, rendant un plancher superflu. La config expose `θ` directement pour permettre d'ajuster l'échelle temporelle sans modifier la forme de la distribution.",
    dependence:
      "La tentative suivante reste aussi bloquée tant qu'un brouillard actif occupe déjà la carte."
  }
];

const logNormalProcesses = [
  {
    title: "Densité locale d'un brouillard",
    illustrationKey: "weather-front-density-seed",
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
      "La log-normale garantit des multiplicateurs strictement positifs, avec une queue à droite utile pour créer quelques poches très opaques sans valeurs négatives. L'opacité locale est conçue comme un multiplicateur appliqué à une base : elle doit donc être strictement positive. Une Beta, bornée dans [0, 1], aurait été envisageable si l'opacité devait toujours rester inférieure à la base, mais la conception autorise des zones légèrement plus opaques que la valeur nominale (multiplicateur > 1), ce qui exclut la Beta au profit de la log-normale. La **troncature finale** via clamp d'alpha n'est pas une correction d'urgence mais une décision de design explicite : elle définit les plages d'opacité visuellement acceptables indépendamment des valeurs extrêmes que la log-normale peut générer.",
    simulation:
      "`sampleLogNormalCell` redérive un générateur par cellule à partir de `densitySeed`, puis échantillonne `std::lognormal_distribution<double>(mu, sigma)`.",
    codeSnippet:
`// WeatherSystem.cpp
std::mt19937 gen(mixSeed(seed,
    (uint32_t)(cellX + 0x5000) * 2246822519u));
gen.seed(mixSeed(gen(), (uint32_t)(cellY + 0x5000) * 3266489917u));
std::lognormal_distribution<double> dist(mu, sigma);
// mu = -0.12,  sigma = 0.35
float multiplier = (float)dist(gen);
// alpha = clamp(0.48 * multiplier, alphaMin, alphaMax)`,
    simulationFromUniform:
      "Si `Z ~ N(0,1)` est obtenu via Box-Muller à partir de deux uniformes, alors `X = e^{μ + σZ} ~ LogNormal(μ, σ²)`. La transformation `exp` garantit `X > 0` sans aucun accept/reject. La STL compose directement Box-Muller et l'exponentielle en une seule passe.",
    parameterChoice:
      "La valeur `μ = −0.12` place la médiane à `exp(−0.12) ≈ 0.89`, légèrement sous 1 : la plupart des cellules reçoivent un multiplicateur proche mais légèrement inférieur à la valeur nominale, évitant que le brouillard soit en moyenne plus opaque que prévu. La valeur `σ = 0.35` donne une dispersion modérée : l'intervalle `[exp(μ − σ), exp(μ + σ)] ≈ [0.63, 1.32]` couvre la majorité des multiplicateurs, avec une queue droite permettant quelques poches très opaques. Le clamp d'alpha `[0.22, 0.82]` traduit les multiplicateurs en opakités visuellement admissibles : `0.22` correspond à un brouillard fin (presque transparent), `0.82` à un brouillard épais sans être complètement solide. Ce clamp est une décision de design visuel indépendante de la loi statistique : son double rôle est d'éviter les cellules quasi-transparentes (inutiles tactiquement) et quasi-solides (non lisibles).",
    dependence:
      "Toutes les cellules d'un même brouillard partagent la même graine de densité; le champ n'est donc pas i.i.d. (**indépendant et identiquement distribué**) à l'échelle du brouillard."
  }
];

const betaProcesses = [
  {
    title: "Luminosité de l'herbe",
    illustrationKey: "grass-brightness-beta",
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
      "Avec `alpha > beta`, la Beta concentre la masse près de 1, ce qui laisse la plupart des herbes proches de la luminosité nominale tout en autorisant quelques assombrissements visibles. La luminosité d'une cellule d'herbe est naturellement une proportion dans [0, 1] avant remappage, ce qui fait de la Beta la famille de référence : son support coïncide exactement avec l'espace des valeurs utiles, sans nécessiter de troncature. Une normale tronquée aurait fonctionné, mais les paramètres (μ, σ) sont moins intuitifs pour décrire une répartition majoritairement haute avec une queue d'assombrissements. La combinaison seuil/contraste en aval transforme la Beta brute en variation perceptible : les valeurs au-dessus du seuil 0,90 conservent la luminosité nominale, les valeurs en dessous sont ramenées dans un intervalle visible via la puissance de contraste.",
    simulation:
      "Le code échantillonne la Beta via deux Gammas, applique un seuil à 0.90, puis remappe la partie basse vers `[0.68, 1]` avec une puissance 1.8.",
    codeSnippet:
`// BoardGenerator.cpp
float sampleBeta(std::mt19937& rng, float alpha, float beta) {
    std::gamma_distribution<float> g1(alpha, 1.0f);  // α = 7
    std::gamma_distribution<float> g2(beta,  1.0f);  // β = 2
    float a = g1(rng), b = g2(rng);
    return (a + b > 0.0f) ? a / (a + b) : 1.0f;
}
// Appel : sampleBeta(rng, 7.0f, 2.0f)`,
    simulationFromUniform:
      "La **représentation de normalisation** de la Beta : si `G1 ~ Γ(α, 1)` et `G2 ~ Γ(β, 1)` sont indépendantes (chacune simulée par Marsaglia-Tsang), alors `X = G1/(G1 + G2) ~ Beta(α, β)`. Le dénominateur `G1 + G2 ~ Γ(α+β, 1)` assure la normalisation. Le code implémente exactement cette construction avec deux `std::gamma_distribution`.",
    parameterChoice:
      "Le choix `α = 7, β = 2` place la masse dans la partie haute de `[0, 1]` : `E[B] = 7/9 ≈ 0.78` et mode `= 6/7 ≈ 0.86`, ce qui signifie que la plupart des cellules d'herbe conservent une luminosité proche de la valeur nominale. Le rapport `α/β = 3.5` est suffisant pour que la queue gauche (herbes sombres) soit visible mais rare, produisant l'impression de variation naturelle sans assombrir l'ensemble du plateau. Le seuil `0.90` sépare les cellules qui gardent la luminosité nominale (celles dont `B > 0.90`, les plus nombreuses) de celles qui entrent dans le pipeline de remappage vers `[0.68, 1]` : ce remappage concentre les valeurs sombres dans un intervalle perceptible sans créer de noirs francs. L'exposant de contraste `1.8` compresse légèrement les valeurs basses après remappage pour adoucir les transitions, évitant les cellules trop uniformes qui résulteraient d'un remappage linéaire pur.",
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
      L`\text{nœuds } (0, \tfrac14 M, \tfrac12 M, \tfrac34 M, M)`,
      "poids `(0.7, 1.8, 1.98, 1.8, 0.7)` dans la config active",
      "avec `1.98 = 1.1 * 1.8` pour le point median"
    ],
    why:
      "Le jeu veut privilégier les entrées centrales tout en conservant une probabilité non nulle de départ par les coins; la linéaire par morceaux est idéale pour cette densité dessinée à la main. Une uniforme donnerait autant de chances aux coins qu'au centre, ne respectant pas l'intention visuelle d'un brouillard qui entre plutôt par le milieu. Une distribution triangulaire concentrerait la masse au centre mais sans permettre le réglage fin des poids aux différents points de contrôle. La linéaire par morceaux est la seule famille standard qui permet de spécifier la densité point par point, ici cinq nœuds répartis régulièrement, et de l'ajuster directement depuis la config sans changer de modèle. C'est une densité entièrement dessinée à la main, ce qui est la formulation honnête de décisions de design qui ne découlent pas d'un modèle mathématique préexistant.",
    simulation:
      "`sampleEdgePosition` construit les bornes et les hauteurs puis utilise `std::piecewise_linear_distribution<double>`.",
    codeSnippet:
`// WeatherSystem.cpp
const std::array<double, 5> bounds{
    0.0, M*0.25, M*0.5, M*0.75, M};
const std::array<double, 5> weights{
    cornerW, centerW, centerW*1.1, centerW, cornerW};
// cornerW = 0.7,  centerW = 1.8
std::piecewise_linear_distribution<double> dist(
    bounds.begin(), bounds.end(), weights.begin());
float position = (float)dist(generator);`,
    simulationFromUniform:
      "La STL implémente la **transformée inverse segmentée** : on calcule la CDF cumulative par tranche, on tire `U ~ U[0,1]`, on localise le segment `[x_i, x_{i+1}]` tel que `F(x_i) ≤ U < F(x_{i+1})`, puis on résout l'équation quadratique sur ce segment. Tout découle d'un unique tirage uniforme.",
    parameterChoice:
      "Les cinq nœuds répartissent le bord d'entrée en quatre segments égaux. Les poids `(0.7, 1.8, 1.98, 1.8, 0.7)` reflètent trois niveaux de densité : les coins (`0.7`), les quarts (`1.8`) et le milieu (`1.98`). Le rapport `centre/coin = 1.98/0.7 ≈ 2.83` signifie que le milieu est presque trois fois plus probable que les coins : les brouillards entrent principalement par le centre du bord, ce qui les rend lisibles (ils traversent le plateau plutôt que de longer un coin). Le rapport `quart/coin = 1.8/0.7 ≈ 2.57` assure une transition douce entre les coins sous-pondérés et le centre surpondéré, évitant un saut brutal de densité. La valeur `1.98 = 1.1 × 1.8` pour le point médian est une légère surpondération par rapport aux deux quarts adjacents, renforçant l'attraction centrale sans créer un pic trop visible. Les coins conservent `0.7 > 0` pour que les entrées par les bords du plateau restent possibles.",
    dependence:
      "Dépend ensuite du bord retenu et de la direction du brouillard pour se convertir en coordonnées 2D."
  }
];

const proceduralProcesses = [
  {
    title: "Champ spatial de la terre",
    illustrationKey: "dirt-field",
    system: "Carte",
    lawUse: "Champ procédural corrélé dérivé de bruit",
    variable: L`X_{terre}(c) = g_{S_{terre}}(c)`,
    phenomenon:
      "Produit des zones de terre connexes plutôt que des cellules i.i.d. indépendantes.",
    parameters: [
      "`terrain_noise_scale = 14`",
      "`terrain_octaves = 3`",
      "couverture cible terre = 14 %",
      "post-traitement par composantes et amas: 6 amas, rayon 2 à 5"
    ],
    why:
      "Une loi usuelle scalaire ne suffit pas ici: il faut un champ spatial corrélé pour faire émerger des zones organiques. Traiter chaque cellule comme un tirage de Bernoulli indépendant produirait un bruit pur sans structure : pas de continents, pas de couloirs, pas de bords lisibles. Le bruit de valeur (value noise) génère précisément cette cohérence spatiale : les cellules proches tendent à avoir des valeurs proches, ce qui fait émerger des régions. L'empilement fractal d'octaves affine les bords et ajoute du détail à petite échelle sans recourir à une modélisation physique ou à un algorithme de croissance cellulaire plus coûteux.",
    simulation:
      "Le générateur évalue `valueNoise` puis `fractalNoise`, applique des seuils, conserve les composantes cohérentes et ajoute des amas locaux de terre.",
    codeSnippet:
`// BoardGenerator.cpp, évaluation du champ procédural
float dirtScore = fractalNoise(dirtNoiseSeed, localX, localY, octaves);
// fractalNoise empile valueNoise à plusieurs octaves depuis worldSeed
if (dirtScore > dirtThreshold)
    cell.type = CellType::Dirt;
// Le valueNoise est une interpolation bilinéaire de hashs de coins :
//   float v = lerp(lerp(hash(x0,y0), hash(x1,y0), tx),
//                  lerp(hash(x0,y1), hash(x1,y1), tx), ty)`,
    parameterChoice:
      "L'échelle `terrain_noise_scale = 14` contrôle la fréquence spatiale du bruit : une valeur de 14 sur un plateau de taille typique génère des zones de terre de quelques cases de rayon, assez larges pour être traversables mais pas assez pour dominer la carte. Trois octaves superposent basse, moyenne et haute fréquences : la première octave donne la structure générale des continents, les deux suivantes ajoutent des détails de bord et des pointes irrégulières sans multiplier le coût de calcul. La couverture cible de `14 %` laisse la majorité du plateau en herbe (terrain libre de base) tout en garantissant suffisamment de terre pour bloquer des passages et créer de la texture spatiale. Les 6 amas post-traitement (rayon 2 à 5) renforcent localement les zones déjà denses, évitant les fragments isolés d'une seule case qui seraient stratégiquement négligeables et visuellement incohérents.",
    dependence:
      "Forte corrélation spatiale: des cellules voisines partagent la même graine et des fréquences proches."
  },
  {
    title: "Champ spatial de l'eau",
    illustrationKey: "water-field",
    system: "Carte",
    lawUse: "Champ procédural corrélé dérivé de bruit",
    variable: L`X_{eau}(c) = h_{S_{eau}}(c)`,
    phenomenon:
      "Construit les poches d'eau et les petits lacs sans casser la jouabilité du plateau.",
    parameters: [
      "couverture cible eau = 4 %",
      "post-traitement par 3 lacs de rayon 2 à 3",
      "même échelle et même nombre d'octaves que la terre"
    ],
    why:
      "Comme pour la terre, on veut des zones spatialement cohérentes, pas une Bernoulli par cellule qui gribouillerait le plateau. L'eau doit former des poches et des lacs reconnaissables, pas une distribution aléatoire de cellules isolées qui ne bloquerait aucun couloir de façon stratégique. En réutilisant la même architecture de bruit procédural que la terre mais avec une graine différente, on garantit que les deux champs sont décorrélés : une poche d'eau n'apparaît pas systématiquement là où se trouve de la terre, ce qui donnerait une carte illisible.",
    simulation:
      "Le pipeline redérive un score de bruit, applique un seuil propre à l'eau, filtre par composantes puis injecte quelques lacs complémentaires.",
    parameterChoice:
      "La faible couverture 4 % évite de couper brutalement les couloirs de circulation du jeu.",
    dependence:
      "Corrélation spatiale importante et dépendance indirecte au champ de la terre via les contraintes d'assemblage du plateau final."
  },
  {
    title: "Masque de retournement des textures de terrain",
    illustrationKey: "terrain-flip-mask",
    system: "Carte",
    lawUse: "Pseudo-uniforme par hachage de position",
    variable: L`F(c) = \mathrm{hash}(worldSeed, type, c) \bmod 4`,
    phenomenon:
      "Retourne horizontalement et/ou verticalement les textures de terrain pour casser les répétitions visibles.",
    parameters: ["4 états de retournement", "hachage positionnel avec `worldSeed` et `CellType`"],
    why:
      "Le besoin principal est la reproductibilité locale, pas un échantillonnage i.i.d. complet à chaque frame. Utiliser un générateur d'état comme `mt19937` pour assigner un masque à chaque cellule obligerait soit à regénérer toute la séquence depuis le début à chaque accès, soit à stocker le masque de chaque cellule en mémoire. Un hachage positionnel évite ces deux écueils : il calcule le masque de n'importe quelle cellule en O(1) à partir de sa position et de `worldSeed`, sans état intermédiaire. Ce n'est pas un tirage aléatoire au sens strict mais une fonction déterministe pseudo-aléatoire dont la distribution empirique est suffisamment uniforme sur les quatre états pour que l'approximation soit correcte visuellement.",
    simulation:
      "`terrainFlipMaskFor` redérive un seed mélange, hache la position puis conserve les deux bits de retournement utiles.",
    parameterChoice:
      "Les quatre états (aucun, horizontal, vertical, double) couvrent exactement le groupe des symétries planaires de la texture, encodé en 2 bits, la représentation minimale sans redondance. Le hachage positionnel est préféré à un générateur d'état (`mt19937`) pour des raisons d'accès aléatoire : calculer le masque d'une cellule quelconque se fait en O(1) à partir de sa position et de `worldSeed`, sans parcourir toute la séquence depuis le début. L'uniformité des 4 états est approximée par la propriété pseudo-aléatoire du hachage et non garantie exactement, mais l'écart à l'uniforme est statistiquement imperceptible visuellement. La dépendance à `CellType` dans la graine garantit que deux types de terrain différents sur la même case produisent des masques indépendants.",
    dependence:
      "Déterminisme strict par cellule; dépendance quasi nulle à longue distance mais pas modélisée comme une loi scalaire autonome."
  },
  {
    title: "Bruit de contour du brouillard",
    illustrationKey: "weather-front-contour-noise",
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
      "Un brouillard sans bruit aurait une silhouette parfaitement elliptique, trop analytique et immédiatement reconnaissable comme artificielle. Perturber le bord point par point avec des variables indépendantes (i.i.d.) produirait un contour en dents de scie haute fréquence, sans forme nuageuse naturelle. Le value noise avec un grand span (6 cellules) génère une déformation de bord à basse fréquence : les bosses sont larges et douces, ce qui imite la texture d'un vrai nuage. Le paramètre d'amplitude à 100 % autorise des déformations significatives, ensuite atténuées par le fondu de bord (edgeSoftness) pour éviter les transitions brutales.",
    simulation:
      "Le code évalue `valueNoise(shapeSeed, x, y, span)`, déforme la limite effective du brouillard, puis applique un fondu par `edgeSoftness`.",
    parameterChoice:
      "Le paramètre `span = 6` contrôle la fréquence spatiale des déformations de contour : avec 6 cellules par période, les bosses du bord sont larges et douces, imitant la texture organique d'un vrai nuage. Une valeur plus petite (`span = 2`) produirait un contour en dents de scie à haute fréquence, visuellement artificiel. L'amplitude `a = 1` (`amplitude_percent = 100 %`) autorise des déformations allant jusqu'à ±50 % de la demi-largeur transversale du brouillard : les bosses sont significatives et visibles, justifiant l'existence de ce module de bruit plutôt qu'une ellipse pure. Le fondu `edge_softness_percent = 18 %` atténue progressivement l'opakité sur les 18 % extérieurs du rayon du brouillard : il absorbe les transitions brutales que les déformations de contour créeraient sinon, maintenant l'impression de bords doux et naturels.",
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

const processTheoryByTitle = {
  "Graines 32 bits des champs procéduraux": createTheory({
    support: L`S_i\in\{0,\dots,2^{32}-1\},\qquad i\in\{\text{terre},\text{eau},\text{forme},\text{densité}\}`,
    law: L`\mathbb{P}(S_i=k)=\frac{1}{2^{32}}`,
    expectation: L`\mathbb{E}[S_i]=\frac{2^{32}-1}{2}`,
    variance: L`\mathrm{Var}(S_i)=\frac{2^{64}-1}{12}`,
    note:
      "Ces moments sont formels : la seed n'est jamais interprétée directement, elle sert seulement d'entrée uniforme à un processus aval spatial ou météo."
  }),
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
  "Apparition des rois": makePositionTheory({
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
    note: "Cette variable fixe la part de plateau que le brouillard cherche à occuper avant discrétisation sur la grille."
  }),
  "Allongement du brouillard": makeUniformFiniteTheory({
    support: L`A\in[1.80,2.60]`,
    law: L`f_A(a)=\frac{1}{0.80}\,\mathbf{1}_{[1.80,2.60]}(a)`,
    expectation: L`\mathbb{E}[A]=\frac{1.80+2.60}{2}=2.20`,
    variance: L`\mathrm{Var}(A)=\frac{(2.60-1.80)^2}{12}`,
    note: "La valeur échantillonnée est ensuite transformée en ellipse et trajectoire discrètes."
  }),
  "Choix d'un mouvement aléatoire en phase Searching": makeNominalTheory({
    support: L`\mathcal{M}_{adm}(t)`,
    law: L`\mathbb{P}(M=m\mid m\in \mathcal{M}_{adm}(t))=\frac{1}{|\mathcal{M}_{adm}(t)|}`,
    note:
      "Le support est l'ensemble des coups légalement atteignables pour la pièce au tour courant, donc il change avec l'état du plateau."
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
  "Royaume cible d'une pièce du diable": createTheory({
    support: L`K_t\in\{0,1\}`,
    law: L`K_t\sim\mathrm{Bernoulli}(p_t)`,
    expectation: L`\mathbb{E}[K_t]=p_t`,
    variance: L`\mathrm{Var}(K_t)=p_t(1-p_t)`,
    note:
      "Le paramètre dynamique `p_t` est dérivé de la dette de sang normalisée; 1 peut être codé comme \"royaume blanc cible\"."
  }),
  "Activation d'un mouvement aléatoire en phase Searching": createTheory({
    support: L`A_t\in\{0,1\}`,
    law: L`A_t\sim\mathrm{Bernoulli}(p),\qquad p=0.333`,
    expectation: L`\mathbb{E}[A_t]=p=0.333`,
    variance: L`\mathrm{Var}(A_t)=p(1-p)`,
    note:
      "Cette Bernoulli décide si la pièce infernale abandonne sa trajectoire guidée pour un coup aléatoire au tour courant."
  }),
  "Déclenchement d'apparition d'une pièce du diable": createTheory({
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
      "Le plafond discrétise la variable continue. Les moments affichés sont donc des références théoriques autour desquelles le runtime se concentre."
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

function createParameterSection(label, entries) {
  return { label, entries };
}

const parameterSectionsByTitle = {
  "Graines 32 bits des champs procéduraux": [
    createParameterSection("Paramètres de loi", [
      "largeur de mot = `32` bits",
      "cardinal du support par variable = `2^32`",
      "`4` variables uniformes regroupées ici : `S_terre`, `S_eau`, `S_forme`, `S_densité`"
    ]),
    createParameterSection("Paramètres runtime", [
      "carte : `2` tirages filles dérivés de `worldSeed` au début de génération",
      "météo : `2` tirages filles supplémentaires par apparition via le générateur d'événement"
    ])
  ],
  "Rotation des mines et fermes neutres": [
    createParameterSection("Paramètres de loi", [
      "`a = 0`, `b = 3`, donc `n = 4` états équiprobables",
      "codage des quarts de tour : `R ∈ {0, 1, 2, 3}`"
    ])
  ],
  "Retournement des mines et fermes neutres": [
    createParameterSection("Paramètres de loi", [
      "`a = 0`, `b = 3`, donc `n = 4` états équiprobables",
      "codage des masques : `0 = aucun`, `1 = horizontal`, `2 = vertical`, `3 = double`"
    ]),
    createParameterSection("Codage runtime", [
      "bits conservés : `kFlipHorizontalMask = 1`, `kFlipVerticalMask = 2`"
    ])
  ],
  "Choix de position des bâtiments publics": [
    createParameterSection("Paramètres de sélection", [
      L`K = |A_{top}| = \min\!\left(n,\max\!\left(3,\left\lceil \frac{n}{6}\right\rceil\right)\right)`,
      L`s(p)=3.5\,d_{any}(p)+2.0\,d_{same}(p)+0.35\,\bar d(p)`,
      "`d_any(p)` = distance au bâtiment existant le plus proche (tout type)",
      "`d_same(p)` = distance au bâtiment du même type le plus proche",
      "`d̄(p)` = distance moyenne à tous les bâtiments existants"
    ]),
    createParameterSection("Support / contraintes", [
      "support courant = origines de footprint géométriquement admissibles pour le bâtiment considéré",
      "le tirage final est uniforme sur les `K` meilleurs candidats après tri par score"
    ])
  ],
  "Apparition des rois": [
    createParameterSection("Paramètres de support", [
      "bandes latérales de largeur `25 %` pour chacun des deux royaumes",
      "supports conditionnels symétriques `A_W` et `A_B`"
    ]),
    createParameterSection("Contraintes conditionnelles", [
      "cellules admissibles et non bloquées par le terrain",
      "séparation stratégique initiale conservée entre les deux royaumes"
    ])
  ],
  "Bord diagonal d'entrée du brouillard": [
    createParameterSection("Paramètres de loi", [
      "`n = 2` bords compatibles pour une diagonale donnée",
      "`p(e_1) = p(e_2) = 1/2`"
    ])
  ],
  "Couverture cible du brouillard": [
    createParameterSection("Paramètres de loi (modélisation théorique)", [
      "borne basse `a = 0.05`",
      "borne haute `b = 0.20`"
    ]),
    createParameterSection("Paramètres runtime (tirage effectif)", [
      "`coverage_min_percent = 5`",
      "`coverage_max_percent = 20`",
      "pas de discrétisation = `1 %`"
    ])
  ],
  "Allongement du brouillard": [
    createParameterSection("Paramètres de loi (modélisation théorique)", [
      "borne basse `a = 1.80`",
      "borne haute `b = 2.60`"
    ]),
    createParameterSection("Paramètres runtime (tirage effectif)", [
      "`aspect_ratio_min_times_100 = 180`",
      "`aspect_ratio_max_times_100 = 260`",
      "pas de discrétisation = `0.01`"
    ])
  ],
  "Choix d'un mouvement aléatoire en phase Searching": [
    createParameterSection("Paramètres de loi", [
      "aucun paramètre libre hors support courant : la loi est uniforme sur `A_moves(t)`"
    ]),
    createParameterSection("Support / contraintes", [
      "support courant = coups légaux générés pour la pièce au tour `t`",
      "filtrage supplémentaire par visibilité locale et collisions interdites"
    ])
  ],
  "Ordre de placement des mines et fermes neutres": [
    createParameterSection("Paramètres de loi", [
      "taille de la permutation `n = num_mines + num_farms = 5`",
      "décomposition active : `2` mines et `3` fermes"
    ])
  ],
  "Case d'apparition d'un coffre": [
    createParameterSection("Poids / score", [
      L`w(c)=1+\mathrm{centrality}(c)+\mathrm{contestation}(c)`,
      "`centrality(c) = max(0, 2R − dist(c, centre))` où `R = board.getRadius()`",
      "`contestation(c) = max(0, 2R − |dist(c, roi_B) − dist(c, roi_N)|)` (distances Manhattan)",
      "toute case admissible conserve un poids strictement positif"
    ]),
    createParameterSection("Support / contraintes", [
      "cellules libres et admissibles sur le plateau courant",
      "distance minimale aux rois : `min_distance_from_kings = 6`"
    ])
  ],
  "Type de récompense du coffre": [
    createParameterSection("Paramètres de loi", [
      "début de partie : poids `(8, 3, 3)` pour `(or, mouvement, construction)`, somme = `14`",
      "fin de partie : poids `(4, 6, 6)` pour `(or, mouvement, construction)`, somme = `16`",
      "bascule de régime au tour `late_game_turn = 10`"
    ]),
    createParameterSection("Probabilités associées", [
      "early : `P(or) = 8/14 ≈ 57.1 %`, `P(mouvement) = 3/14 ≈ 21.4 %`, `P(construction) = 3/14 ≈ 21.4 %`",
      "late : `P(or) = 4/16 = 25.0 %`, `P(mouvement) = 6/16 = 37.5 %`, `P(construction) = 6/16 = 37.5 %`"
    ]),
    createParameterSection("Payoffs associés", [
      "branche or : montant tiré par la normale tronquée dédiée de moyenne `35`",
      "branche mouvement : `movement_bonus_amount = 1` point max par tour",
      "branche construction : `build_bonus_amount = 1` point max par tour"
    ]),
    createParameterSection("Couplage runtime", [
      "mode de rattrapage actif : `current_loot_catch_up_enabled = true`"
    ])
  ],
  "Direction du brouillard": [
    createParameterSection("Paramètres de loi", [
      "ordre des catégories : `(N, S, E, W, NE, NW, SE, SW)`",
      "`direction_weights = (1, 1, 1, 1, 1, 1, 1, 1)`, poids égaux = uniforme",
      "probabilité par direction : `P(D = d) = 1/8 = 12.5 %`"
    ])
  ],
  "Type de cible primaire d'une pièce du diable": [
    createParameterSection("Paramètres de loi", [
      "poids actifs : `pawn = 8`, `knight = 14`, `bishop = 14`, `rook = 26`, `queen = 38`",
      "somme des poids = `100` quand tous les types sont visibles",
      "probabilités quand tous visibles : `P(pion) = 8 %`, `P(cavalier) = 14 %`, `P(fou) = 14 %`, `P(tour) = 26 %`, `P(reine) = 38 %`",
      "les types non visibles reçoivent le poids `0` (support réduit dynamiquement)"
    ])
  ],
  "Option d'apparition ciblée d'une pièce du diable": [
    createParameterSection("Poids / score", [
      L`w(o)=\max\!\bigl(1, 2D-\mathrm{dist}(o)+1\bigr)`,
      "diamètre de référence : `D = board.getDiameter()`",
      "`dist(o)` = longueur du plus court chemin de l'option d'entrée `o` vers la cible"
    ])
  ],
  "Royaume cible d'une pièce du diable": [
    createParameterSection("Paramètres de loi", [
      L`p_t = \frac{\mathrm{debt}_{white}}{\mathrm{debt}_{white}+\mathrm{debt}_{black}}`,
      L`p_t = 0.5\text{ si }\mathrm{debt}_{white}+\mathrm{debt}_{black}=0`
    ])
  ],
  "Activation d'un mouvement aléatoire en phase Searching": [
    createParameterSection("Paramètres de loi", [
      "probabilité active `p = 333 / 1000 = 0.333`",
      "implémentation discrétisée sur une grille de `1000` valeurs"
    ])
  ],
  "Déclenchement d'apparition d'une pièce du diable": [
    createParameterSection("Paramètres de loi", [
      L`\lambda_t = \min\!\bigl(\lambda_{max},\; \lambda_{base} + \lambda_{debt}\cdot d_t\bigr),\qquad d_t = \text{dette}_{blanc}+\text{dette}_{noir}`,
      "`poisson_lambda_base_times_1000 = 20`, soit `λ_{base} = 0.020`",
      "`poisson_lambda_per_debt_times_1000 = 12`, soit `λ_{debt} = 0.012` par point de dette",
      "`poisson_lambda_cap_times_1000 = 250`, soit `λ_{max} = 0.250`",
      L`P(N\ge 1)\big|_{\lambda_{max}} = 1-e^{-0.25}\approx 22.1\,\%`
    ]),
    createParameterSection("Calendrier runtime", [
      "tour minimal de première apparition : `min_spawn_turn = 3`",
      "cooldown de réapparition : `respawn_cooldown_turns = 4`",
      "nouvelle tentative après échec : `spawn_retry_turns = 1`"
    ])
  ],
  "Récompenses d'XP": [
    createParameterSection("Transformation commune", [
      L`\sigma = \max\!\left(1, \mu\cdot \frac{\text{sigmaMultiplierTimes100}}{100}\right)`,
      "`clamp_sigma_multiplier_times_100 = 200` pour tous les profils",
      L`\delta = \sigma\cdot\frac{200}{100} = 2\sigma\quad\Rightarrow\quad\text{troncature à }[\mu-2\sigma,\,\mu+2\sigma]`,
      "puis `round`, puis plancher `minimum = 1`"
    ]),
    createParameterSection("Profils par source", [
      "`kill_pawn` : `μ = 20`, `sigma_multiplier = 18 %`, `σ = 3.6`, intervalle `[12.8, 27.2]`, `minimum = 1`",
      "`kill_knight` / `kill_bishop` : `μ = 50`, `sigma_multiplier = 16 %`, `σ = 8`, intervalle `[34, 66]`, `minimum = 1`",
      "`kill_rook` : `μ = 100`, `sigma_multiplier = 12 %`, `σ = 12`, intervalle `[76, 124]`, `minimum = 1`",
      "`kill_queen` : `μ = 300`, `sigma_multiplier = 10 %`, `σ = 30`, intervalle `[240, 360]`, `minimum = 1`",
      "`destroy_block` / `arena_per_turn` : `μ = 10`, `sigma_multiplier = 15 %`, `σ = 1.5`, intervalle `[7, 13]`, `minimum = 1`"
    ])
  ],
  "Montant d'or d'un coffre": [
    createParameterSection("Paramètres de loi", [
      "`μ = 35`",
      "`sigma_multiplier_times_100 = 18`, donc `σ = 6.3`",
      "`clamp_sigma_multiplier_times_100 = 200`, donc intervalle tronqué `[22.4, 47.6]`",
      "`minimum = 1`"
    ])
  ],
  "Délai de réapparition d'un coffre": [
    createParameterSection("Paramètres de loi", [
      "forme `k = 1.80` via `weibull_shape_times_100 = 180`",
      "échelle `λ = 6` tours via `weibull_scale_turns = 6`"
    ]),
    createParameterSection("Calendrier runtime", [
      "cooldown plancher `c = 4` tours",
      "tour minimal de première apparition : `min_spawn_turn = 4`",
      "nouvelle tentative après échec : `spawn_retry_turns = 1`"
    ])
  ],
  "Délai entre deux brouillards": [
    createParameterSection("Paramètres de loi", [
      "forme `k = 4.00` via `arrival_gamma_shape_times_100 = 400`",
      "échelle `θ = 10.00` via `arrival_gamma_scale_times_100 = 1000`",
      "minimum additif `m = 0` via `cooldown_min_turns = 0`"
    ]),
    createParameterSection("Transformation runtime", [
      "discrétisation par plafond : `D = m + ceil(T)`"
    ])
  ],
  "Densité locale d'un brouillard": [
    createParameterSection("Paramètres de loi", [
      "`μ = -0.12` via `density_mu_times_100 = -12`",
      "`σ = 0.35` via `density_sigma_times_100 = 35`"
    ]),
    createParameterSection("Transformation runtime", [
      "alpha de base `alpha_base_percent = 48`, donc facteur `0.48`",
      "clamp visuel : `alpha_min_percent = 22`, `alpha_max_percent = 82`"
    ])
  ],
  "Luminosité de l'herbe": [
    createParameterSection("Paramètres de loi", [
      "`α = 7`, `β = 2`"
    ]),
    createParameterSection("Transformation runtime", [
      "seuil de conservation de la luminosité par défaut : `keep_default_threshold = 0.90`",
      "borne basse de remappage : `min_brightness = 0.68`",
      "exposant de contraste : `contrast_exponent = 1.8`"
    ])
  ],
  "Position d'entrée le long du bord d'un brouillard": [
    createParameterSection("Paramètres de loi", [
      L`\text{nœuds } (0, \tfrac14 M, \tfrac12 M, \tfrac34 M, M)`,
      "poids actifs `(0.7, 1.8, 1.98, 1.8, 0.7)`"
    ]),
    createParameterSection("Paramètres runtime", [
      "`entry_corner_weight_times_100 = 70`, donc `cornerW = 0.7`",
      "`entry_center_weight_times_100 = 180`, donc `centerW = 1.8`",
      "surpondération médiane fixée à `1.1 × centerW`, donc `1.98` au centre"
    ])
  ],
  "Champ spatial de la terre": [
    createParameterSection("Paramètres du générateur", [
      "`terrain_noise_scale = 14`",
      "`terrain_octaves = 3`",
      "graine dédiée `S_terre`"
    ]),
    createParameterSection("Post-traitements", [
      "couverture cible terre `dirt_coverage_percent = 14`",
      "ajout de `6` amas locaux de rayon compris entre `2` et `5`"
    ])
  ],
  "Champ spatial de l'eau": [
    createParameterSection("Paramètres du générateur", [
      "`terrain_noise_scale = 14`",
      "`terrain_octaves = 3`",
      "graine dédiée `S_eau`"
    ]),
    createParameterSection("Post-traitements", [
      "couverture cible eau `water_coverage_percent = 4`",
      "ajout de `3` lacs de rayon compris entre `2` et `3`"
    ])
  ],
  "Masque de retournement des textures de terrain": [
    createParameterSection("Paramètres du générateur", [
      "support fini à `4` états de retournement",
      "calcul local `F(c) = hash(worldSeed, CellType, c) mod 4`"
    ]),
    createParameterSection("Codage runtime", [
      "deux bits conservés : horizontal `1`, vertical `2`"
    ])
  ],
  "Bruit de contour du brouillard": [
    createParameterSection("Paramètres du générateur", [
      "portée spatiale `shape_noise_cell_span = 6` cellules",
      "amplitude `shape_noise_amplitude_percent = 100`, donc `a = 1.0`"
    ]),
    createParameterSection("Lissage runtime", [
      "fondu de bord `edge_softness_percent = 18`, donc `edgeSoftness = 0.18`"
    ])
  ]
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
          "**Par exemple, bouger une tour coûte 4 points de mouvement, alors que bouger un pion ne coûte qu'1 point de mouvement.** Ces deux budgets imposent des arbitrages permanents. Quelle pièce déplacer ? Quel bâtiment construire ? Faut-il privilégier les troupes à faible coût de déplacement mais moins puissantes dans sa stratégie ?"
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
          "Les **points de construction** servent notamment à poser ou réparer des structures. Les casernes permettent ensuite de produire de nouvelles pièces au lieu de se contenter de l'armée de départ.",
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
    reportDimensionsTitle: "Les trois volets du rapport",
    reportDimensions: [
      {
        title: "1. Inventaire des processus aléatoires",
        text:
          "Le premier volet du rapport recense les variables aléatoires actives de la codebase, leur famille de loi, leurs paramètres, leur support, leur méthode de simulation et leur ancrage déterministe dans `worldSeed` et les compteurs RNG sérialisés.",
        showSummaryStats: true
      },
      {
        title: "2. 500 parties simulées",
        sourceKind: "simulated",
        sourceTag: "500 parties simulées",
        sourceTagText:
          "Quand ce tag apparaît dans le rapport, les données et visualisations qui suivent proviennent du batch de 500 parties simulées.",
        text:
          "Le deuxième volet s'appuie sur 500 simulations théoriques. Il ne s'agit pas de matchs entre IA ni de parties humaines accélérées: on simule directement les mécanismes concernés, par exemple la génération du terrain, les coffres, la météo ou les apparitions infernales, afin d'estimer leur comportement attendu."
      },
      {
        title: "3. Une partie réelle instrumentée",
        sourceKind: "real",
        sourceTag: "Partie réelle avec joueur",
        sourceTagText:
          "Quand ce tag apparaît dans le rapport, les données et visualisations qui suivent proviennent de la partie réelle instrumentée rejouée dans le viewer.",
        text:
          "Le troisième volet exploite les données d'une partie complète jouée pendant plusieurs heures avec un ami. Les actions, les tours et les états utiles ont été enregistrés pour confronter la théorie à un runtime réel et montrer des résultats lisibles en situation de jeu."
      }
    ],
    replayTitle: "Replay de la partie réelle",
    replayText:
      "Le viewer ci-dessous donne un aperçu direct de la partie observée, en lecture automatique rapide, avec boucle et point de vue blanc. Il sert d'entrée visuelle avant le détail statistique du rapport."
  },
  summaryStats: [
    {
      value: "29",
      label: "Cartes de processus",
      detail: "visibles dans le rapport et regroupées par familles de lois"
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
      detail: "seed mère du monde, complétée par des compteurs RNG sérialisés puis déclinée en seeds auxiliaires selon le système"
    }
  ],
  methodology: {
    paragraphs: [
      "Le jeu n'utilise pas l'aléatoire comme une boîte noire. Chaque système stochastique passe par un schéma récurrent: une seed de monde `worldSeed`, puis soit un dérivé direct de cette seed, soit un générateur d'événement construit à partir de `worldSeed` et d'un compteur `rngCounter`, puis enfin une transformation spécifique au système. Autrement dit, la carte et la météo peuvent bien manipuler plusieurs seeds 32 bits locales, mais elles restent toutes des dérivées d'une même racine `worldSeed`. L'analyse probabiliste correcte doit donc raisonner à deux niveaux: la loi brute tirée par la bibliothèque standard, puis la loi effectivement observée après conditionnement, troncature, arrondi ou filtrage de gameplay.",
      "La classification choisie ici suit les lois plutôt que les fichiers. Cela permet de voir immédiatement que l'XP et l'or des coffres sont deux usages du même schéma de normale tronquée; que la météo combine uniforme, Gamma, log-normale et linéaire par morceaux; et que certains processus de carte ou de contour ne sont pas bien décrits par une variable scalaire classique mais par des champs spatiaux corrélés.",
      "Quand une variable est nominale, par exemple un type de récompense ou une direction, on insiste sur le fait qu'il n'existe pas d'espérance canonique sans choisir au préalable un score numérique auxiliaire. Ce point est essentiel pour ne pas écrire de formules fausses juste parce qu'une API de tirage renvoie un entier d'indice."
    ],
    formulas: [
      {
        label: "Schéma déterministe transverse",
        latex: L`U_t = G\bigl(worldSeed, rngCounter_t\bigr), \qquad X_t = \psi\bigl(U_t, S_t\bigr)`
      },
      {
        label: "Évolution du compteur",
        latex: L`rngCounter_{t+1} = rngCounter_t + 1 \quad \text{à chaque événement consommatif}`
      },
      {
        label: "Principe de lecture du rapport",
        latex: L`\text{loi observée} = \text{transformation gameplay} \circ \text{loi standard}`
      }
    ],
    highlights: [
      "Les systèmes XP, Coffres, Météo et Pièces du diable possèdent chacun leur compteur RNG sérialisé; le déterminisme persiste donc après sauvegarde/rechargement.",
      "Les seeds auxiliaires de météo et de génération de carte sont elles-mêmes des variables aléatoires uniformes à grand support, mais ce sont des seeds filles dérivées de `worldSeed`, pas des racines concurrentes; elles servent ensuite à piloter des champs non i.i.d.",
      "Le rapport distingue toujours la loi théorique continue de la loi runtime réellement observée quand un arrondi, un `ceil` ou un `clamp` est appliqué."
    ],
    conformityRows: [
      { law: "Uniforme continue", kind: "densité", isDensity: true, e: String.raw`\frac{a+b}{2}`, v: String.raw`\frac{(b-a)^2}{12}`, example: "Couverture et allongement du brouillard" },
      { law: "Uniforme discrète", kind: "discrète", isDensity: false, e: String.raw`\frac{a+b}{2}`, v: String.raw`\frac{(b-a+1)^2-1}{12}`, example: "Rotation/retournement bâtiments, seeds" },
      { law: "Permutation uniforme", kind: "discrète", isDensity: false, e: String.raw`\mathbb{E}[R_i]=\frac{n+1}{2}`, v: String.raw`\mathrm{Var}(R_i)=\frac{n^2-1}{12}`, example: "Ordre de placement mines/fermes" },
      { law: "Catégorielle pondérée", kind: "discrète", isDensity: false, e: "\\text{dépend du score auxiliaire } g", v: "\\text{dépend du score auxiliaire } g", example: "Type récompense coffre, cible pièce du diable" },
      { law: "Bernoulli", kind: "discrète", isDensity: false, e: "p", v: "p(1-p)", example: "Royaume cible pièce du diable" },
      { law: "Poisson", kind: "discrète", isDensity: false, e: "\\lambda", v: "\\lambda", example: "Déclenchement apparition pièce du diable" },
      { law: "Normale tronquée", kind: "densité", isDensity: true, e: String.raw`\mu+\sigma\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}`, v: String.raw`\sigma^2\!\left[1+\frac{\alpha\varphi(\alpha)-\beta\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}-\left(\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}\right)^2\right]`, example: "Récompenses XP, or des coffres" },
      { law: "Weibull", kind: "densité", isDensity: true, e: String.raw`\lambda\,\Gamma\!\left(1+\tfrac{1}{k}\right)`, v: String.raw`\lambda^2\!\left[\Gamma\!\left(1+\tfrac{2}{k}\right)-\Gamma\!\left(1+\tfrac{1}{k}\right)^2\right]`, example: "Délai réapparition coffre" },
      { law: "Gamma", kind: "densité", isDensity: true, e: "k\\theta", v: "k\\theta^2", example: "Délai inter-brouillards" },
      { law: "Log-normale", kind: "densité", isDensity: true, e: "e^{\\mu+\\sigma^2/2}", v: "(e^{\\sigma^2}-1)e^{2\\mu+\\sigma^2}", example: "Densité locale brouillard" },
      { law: "Beta", kind: "densité", isDensity: true, e: String.raw`\frac{\alpha}{\alpha+\beta}`, v: String.raw`\frac{\alpha\beta}{(\alpha+\beta)^2(\alpha+\beta+1)}`, example: "Luminosité cellules d'herbe" },
      { law: "Linéaire par morceaux", kind: "densité", isDensity: true, e: "\\int_0^M x\\,f(x)\\,dx", v: "\\int_0^M x^2 f(x)\\,dx - \\mathbb{E}[X]^2", example: "Position d'entrée du brouillard sur le bord" }
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
          "Les états dérivés comme le compteur interne des tirages de récompense, la récompense de coffre actuellement partagée et les descripteurs de brouillard conservent la continuité des lois conditionnelles."
      ]
    },
    {
      title: "Séries statistiques directement exploitables",
      text:
        "Le code et les exports JSON permettent déjà de reconstruire plusieurs séries quantitatives utiles pour la validation empirique du modèle.",
      bullets: [
        "Histogrammes d'XP par source et comparaison à la normale tronquée annoncée.",
        "Retards de réapparition des coffres et répartition des récompenses par régime initial/tardif.",
        "Inter-arrivées météo, couverture, rapport d'aspect et opacités locales.",
        "Dette de sang, intensité d'apparition induite et types de cibles effectivement sélectionnés."
      ]
    },
    {
      title: "Sorties déjà visibles dans le projet",
      text:
        "Le dépôt contient déjà plusieurs supports de vérification pratique pour confronter la théorie au runtime.",
      bullets: [
        "`debug_game_state/` contient des historiques de tours utiles pour relire les événements stochastiques.",
        "`saves/` et `PARTICULAR SAVES/` montrent la persistance des seeds et compteurs dans des états concrets.",
        "`statistiques-generator/` offre une base naturelle pour automatiser demain des comparaisons entre distributions attendues et distributions observées."
      ]
    }
  ],
  lawSections: [
    {
      id: "uniformes",
      title: "Uniformes : discrètes, continues et conditionnelles",
      badge: "9 cartes",
      description: [
        "Cette section réunit neuf cartes qui couvrent trois variantes de la loi uniforme. Sept cartes présentent des usages discrets ou conditionnels de l'uniforme sur un support fini ou filtré, et deux cartes présentent des uniformes continues pour la météo : la couverture cible et l'allongement du brouillard. Ce sont ces deux dernières qui correspondent à la ligne « Uniforme continue ★ » du tableau de conformité.",
        "Les quatre seeds 32 bits de la carte et du brouillard sont volontairement regroupées dans une seule carte de synthèse parce qu'elles partagent exactement le même cadre théorique : une uniforme discrète sur 32 bits, suivie d'une transformation aval plus informative dans les sections procédurales ou log-normales. Le début du rapport compte donc ici les cartes réellement affichées, pas chaque processus runtime fusionné à l'intérieur de cette carte de synthèse."
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
      badge: "1 carte",
      randomnessKind: "discrete",
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
      badge: "5 cartes",
      randomnessKind: "discrete",
      description: [
        "Dès qu'il faut choisir entre plusieurs catégories nominales avec des poids relatifs, la bonne famille est la catégorielle pondérée. Elle apparaît ici dans cinq cartes : deux pour les coffres, une pour la météo et deux pour la logique des pièces du diable.",
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
      title: "Bernoulli",
      badge: "2 cartes",
      randomnessKind: "discrete",
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
        "La probabilité `p` peut être statique, comme 0.333, ou dépendre dynamiquement de l'état du jeu comme la dette de sang.",
        "Une Bernoulli sur un support binaire reste la loi la plus lisible pour décrire ces branchements même quand l'implémentation passe par un entier uniforme."
      ],
      processes: illustratedBernoulliProcesses
    },
    {
      id: "poisson",
      title: "Poisson et déclenchement d'arrivée",
      badge: "1 carte",
      randomnessKind: "discrete",
      description: [
        "Les pièces du diable ne reposent pas sur une simple probabilité fixe d'apparition, mais sur un comptage d'arrivées potentielles modélisé par une Poisson. Le gameplay n'observe que l'événement `N >= 1`, mais la variable latente est bien un nombre entier de tentatives.",
        "Ce choix donne une interprétation propre de l'intensité comme dette de sang convertie en fréquence moyenne d'arrivées."
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
        "La dette de sang agit ici comme un paramètre d'intensité, pas comme un poids catégoriel."
      ],
      processes: illustratedPoissonProcesses
    },
    {
      id: "normales-tronquees",
      title: "Normales tronquées et discrétisées",
      badge: "2 cartes",
      randomnessKind: "density",
      isDensity: true,
      description: [
        "L'XP et l'or des coffres réutilisent le même patron: une normale centrée sur une moyenne conception, tronquée à un multiple de son écart-type, arrondie à l'entier puis soumise à un plancher minimal.",
        "La loi effectivement jouée n'est donc pas une gaussienne pure: c'est une gaussienne transformée par clamp et quantification."
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
        "L'arrondi et le minimum modifient légèrement l'espérance par rapport à la formule continue; la formule ci-dessus est donc la bonne référence théorique, pas la valeur exacte après discrétisation.",
        "Dans ce code, `sigma = max(1, mean * sigmaMultiplier)` et `a,b = mean +/- clampMultiplier * sigma`."
      ],
      processes: illustratedTruncatedNormalProcesses
    },
    {
      id: "weibull",
      title: "Weibull discrétisée",
      badge: "1 carte",
      randomnessKind: "density",
      isDensity: true,
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
        "Avec `k > 1`, le taux de risque augmente avec le temps, ce qui correspond bien à l'intuition \"plus le coffre tarde, plus sa réapparition devient plausible\"."
      ],
      processes: illustratedWeibullProcesses
    },
    {
      id: "gamma",
      title: "Gamma discrétisée",
      badge: "1 carte",
      randomnessKind: "density",
      isDensity: true,
      description: [
        "La Gamma pilote ici surtout les inter-arrivées météo. Son support positif et sa grande souplesse de forme en font un bon compromis entre exponentialité pure et modèle trop rigide.",
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
        "Dans la lecture retenue par ce rapport, la Gamma documentée côté météo sert avant tout à modéliser l'attente avant l'apparition d'un nouveau brouillard."
      ],
      processes: illustratedGammaProcesses
    },
    {
      id: "lognormale",
      title: "Log-normale",
      randomnessKind: "density",
      isDensity: true,
      badge: "1 carte",
      description: [
        "La log-normale apparaît dans la texture d'opacité des brouillards. Le choix est mathématiquement naturel dès qu'on veut des multiplicateurs strictement positifs, susceptibles d'être parfois un peu plus grands que 1 sans jamais devenir négatifs.",
        "Le runtime redérive la graine par cellule à partir de `densitySeed`, puis re-borne le résultat via des `alphaMin` et `alphaMax`."
      ],
      formulaCards: [
        {
          label: "Définition",
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
      badge: "1 carte",
      randomnessKind: "density",
      isDensity: true,
      description: [
        "La Beta est utilisée pour la luminosité de l'herbe. C'est une bonne famille pour modéliser une variable naturellement bornée dans `[0,1]` avant transformation visuelle.",
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
      badge: "1 carte",
      randomnessKind: "density",
      isDensity: true,
      description: [
        "La position d'entrée d'un brouillard le long du bord n'est ni uniforme, ni gaussienne. Elle suit une densité dessinée à la main par morceaux linéaires afin de surpondérer les entrées centrales tout en gardant des coins possibles.",
        "C'est un bon exemple de loi standard de la bibliothèque C++ qui n'est pas toujours mobilisée dans les rapports probabilistes classiques, mais qui reste parfaitement légitime ici."
      ],
      formulaCards: [
        {
          label: "Densité linéaire par morceaux",
          latex: L`f(x)=\frac{\ell(x)}{\int_0^M \ell(u)\,du}, \qquad \ell \text{ linéaire sur chaque intervalle de nœuds}`
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
      title: "Variables personnalisées et champs procéduraux corrélés",
      badge: "4 cartes",
      description: [
        "Tous les processus aléatoires du jeu ne sont pas raisonnablement résumables par une unique variable scalaire. Les champs de terrain et les déformations de contour du brouillard sont des fonctions aléatoires de la cellule et d'une seed, avec forte corrélation spatiale.",
        "**Du point de vue des contraintes (lois connues)**: ces processus reposent entièrement sur une **seed uniforme discrète sur 32 bits**, loi connue, pilotant ensuite un **post-traitement déterministe** (interpolation de hash, empilement d'octaves, seuillage). La VA de base est bien une uniforme connue; les structures spatiales émergent d'un calcul déterministe à partir de cette seed. Ils ne constituent donc pas des « lois inconnues » mais des transformations déterministes d'uniformes.",
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
    "Les brouillards portent deux graines internes, l'une pour la forme et l'autre pour la densité, qui induisent de fortes corrélations spatiales intra-brouillard, puis une dépendance temporelle via le prochain délai d'apparition et les possibles chevauchements de fronts.",
    "Les pièces du diable ne reposent pas sur un système à paramètres fixes: leur Bernoulli de royaume cible et leur Poisson d'apparition dépendent directement d'un état dynamique, la dette de sang.",
    "**Chaîne de Markov**: la dette de sang (`bloodDebt`) est un processus markovien naturel, elle dépend de l'état précédent (dettes accumulées) et évolue selon des transitions probabilistes (apparitions, destructions). Une modélisation explicite en chaîne de Markov à états discrets aurait pu formaliser la dynamique de la dette, mais le gain de précision ne justifiait pas la complexité supplémentaire face au modèle Poisson déjà en place. Ce point reste une piste d'approfondissement."
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
        "La terre, l'eau et les contours de brouillard sont produits par du bruit procédural partageant une même seed et un même post-traitement spatial. Une statistique cellule par cellule aurait masqué le vrai phénomène, qui est l'apparition de régions cohérentes, de lacs, de couloirs et de silhouettes. La difficulté a donc été de choisir comme variables observées des résumés adaptés à un champ: couverture totale, cellules refusées, rugosité de bord ou nombre de pièces masquées, plutôt qu'une fausse Bernoulli indépendante par cellule."
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
  ],
  criticalReview: [
    {
      title: "La Weibull pour les coffres : bon choix, mais paramètres sous-optimaux",
      text: "Le paramètre k = 1.8 produit un taux de risque croissant, cohérent avec l'idée qu'un coffre absent depuis longtemps est « attendu », mais la calibration reste empirique : je n'ai pas assez de données de parties pour vérifier que les joueurs ressentent réellement cet effet. Une Gamma aurait été mathématiquement équivalente avec une paramétrisation (k, θ) peut-être plus intuitive. Le choix de la Weibull est défendable mais pas exclusivement motivé par les données."
    },
    {
      title: "Chaîne de Markov : envisagée mais non retenue",
      text: "La dette de sang (`bloodDebt`) aurait pu être modélisée comme une chaîne à états discrets (dette faible / modérée / élevée / critique), mais le mécanisme réel est un compteur continu qui décroît progressivement. L'enchaînement des états n'est pas strictement markovien car la transition dépend de l'historique des combats, pas uniquement de l'état courant. Nous avons donc préféré un modèle Poisson à intensité variable qui capture mieux la non-stationnarité observée. Une vraie chaîne de Markov resterait une piste d'approfondissement."
    }
  ]
};