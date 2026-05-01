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
    lawUse: "Uniforme discrete sur un espace de 32 bits",
    variable: L`S_{terre} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Produit la graine intermediaire qui alimente ensuite le champ procedural de la terre.",
    parameters: [
      "support de taille 2^32",
      "1 tirage au debut de la generation du plateau"
    ],
    why:
      "Une graine intermediaire n'a aucune direction strategique a privilegier; on cherche seulement une repartition uniforme des mondes possibles.",
    simulation:
      "Le generateur `std::mt19937(worldSeed)` est interroge une fois, puis la valeur tiree nourrit `valueNoise` et `fractalNoise`.",
    parameterChoice:
      "Le format 32 bits est exactement celui des sorties natives de `mt19937`, donc pas de biais de conversion supplementaire.",
    dependence:
      "Depend completement de `worldSeed`; aucun tirage en cours de partie."
  },
  {
    title: "Graine globale de l'eau",
    system: "Carte",
    lawUse: "Uniforme discrete sur un espace de 32 bits",
    variable: L`S_{eau} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Produit la graine auxiliaire du champ des lacs et poches d'eau.",
    parameters: [
      "support de taille 2^32",
      "1 tirage au debut de la generation du plateau"
    ],
    why:
      "Le systeme d'eau doit etre decorrele de la terre tout en restant reproductible a seed fixe.",
    simulation:
      "Une sortie brute du generateur initialise la branche d'eau avant evaluation des champs spatiaux.",
    parameterChoice:
      "Le grand support evite des repetitions perceptibles lorsque plusieurs graines auxiliaires sont derivees du meme monde.",
    dependence:
      "Couple a `worldSeed`, mais distinct de `S_{terre}` par l'ordre d'appel du generateur."
  },
  {
    title: "Rotation des mines et fermes neutres",
    system: "Carte",
    lawUse: "Uniforme discrete sur quatre quarts de tour",
    variable: L`R \in \{0,1,2,3\}`,
    phenomenon:
      "Choisit l'orientation sprite des batiments publics deja places afin d'eviter un rendu trop mecanique.",
    parameters: ["support = {0, 1, 2, 3}"],
    why:
      "Les quatre rotations sont geometriquement symetriques pour ces assets; aucune ne doit etre favorisee.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` pendant la generation du plateau.",
    parameterChoice:
      "Quatre etats correspondent exactement aux orientations de 90 degres disponibles.",
    dependence:
      "Depend du flux de generation initial, mais plus du tout apres serialisation de la carte."
  },
  {
    title: "Flip des mines et fermes neutres",
    system: "Carte",
    lawUse: "Uniforme discrete sur les masques de symetrie",
    variable: L`F \in \{0,1,2,3\}`,
    phenomenon:
      "Active ou non le retournement horizontal et vertical des batiments publics.",
    parameters: ["0 = aucun flip", "1 = horizontal", "2 = vertical", "3 = double flip"],
    why:
      "Les masques de symetrie disponibles sont equiprobables des lors qu'on ne veut pas marquer de biais visuel.",
    simulation:
      "Le code utilise `std::uniform_int_distribution<int>(0, 3)` et transmet le masque a la footprint du batiment.",
    parameterChoice:
      "La cardinalite 4 vient de `kFlipHorizontalMask | kFlipVerticalMask`.",
    dependence:
      "Statiquement derive de la generation de carte."
  },
  {
    title: "Choix de position des batiments publics",
    system: "Carte",
    lawUse: "Uniforme conditionnelle sur le haut du classement de dispersion",
    variable: L`X \mid X \in A_{top}`,
    phenomenon:
      "Selectionne l'origine d'une mine ou d'une ferme parmi les meilleurs candidats au regard de la dispersion spatiale.",
    parameters: [
      L`|A_{top}| = \min\left(n, \max\left(3, \left\lceil \frac{n}{6} \right\rceil\right)\right)`,
      "les candidats sont notes puis tries par score de distance"
    ],
    why:
      "Un pur optimum rendrait la carte trop deterministe; un tirage uniforme dans le top conserve la qualite geometrique sans figer la meme configuration.",
    simulation:
      "`selectDispersedCandidate` trie les candidats, calcule `topCount`, puis tire uniformement un index dans ce sous-ensemble.",
    parameterChoice:
      "Le minimum de 3 laisse toujours un peu de variete meme quand l'ensemble admissible est petit.",
    dependence:
      "Conditionne par les placements deja retenus, donc fortement dependante de l'historique de generation."
  },
  {
    title: "Spawn des royaumes",
    system: "Carte",
    lawUse: "Uniforme discrete sur les zones de depart des royaumes",
    variable: L`P_K \sim \mathcal{U}_d(A_K),\; K \in \{W,B\}`,
    phenomenon:
      "Choisit les cellules de depart des rois blanc et noir dans leurs bandes de depart respectives, avec les memes contraintes de terrain et de separation.",
    parameters: [
      "zone de spawn = 25 % du plateau sur chaque cote",
      "admissibilite geometrique, terrain non bloque et separation strategique initiale"
    ],
    why:
      "Les deux royaumes obeissent a la meme logique de tirage conditionnel; la bonne lecture est donc une uniforme discrete sur deux supports lateraux symetriques, pas deux mecanismes differents.",
    simulation:
      "Le generateur collecte les cellules valides de chaque bande laterale, puis tire un index uniforme dans le vecteur de candidats du royaume concerne.",
    parameterChoice:
      "Le pourcentage 25 % vient de `player_spawn_zone_percent` et `ai_spawn_zone_percent`, gardes egaux pour ne pas introduire d'avantage structurel.",
    dependence:
      "Depend du terrain deja genere, donc du couple `worldSeed` + champs proceduraux, et de la contrainte de separation entre royaumes."
  },
  {
    title: "Bord diagonal d'entree du brouillard",
    system: "Meteo",
    lawUse: "Uniforme discrete sur les deux bords compatibles avec la diagonale",
    variable: L`E \in \{e_1,e_2\}`,
    phenomenon:
      "Pour les directions diagonales, choisit lequel des deux bords du plateau sert de bord d'entree effectif.",
    parameters: ["2 bords admissibles par direction diagonale"],
    why:
      "Les deux constructions geometriques possibles sont symetriques; une loi uniforme conserve cette symetrie.",
    simulation:
      "La fonction `randomElement` appelle `std::uniform_int_distribution<int>(0, 1)`.",
    parameterChoice:
      "Deux etats seulement car une diagonale entre toujours soit par un bord soit par l'autre cote compatible.",
    dependence:
      "Depend de la direction du brouillard, elle-meme tiree juste avant."
  },
  {
    title: "Couverture cible du brouillard",
    system: "Meteo",
    lawUse: "Uniforme continue sur un intervalle de pourcentage",
    variable: L`C \sim \mathcal{U}([0.05, 0.20])`,
    phenomenon:
      "Fixe la proportion de cellules visibles que le nouveau brouillard doit recouvrir a sa naissance.",
    parameters: ["`coverage_min_percent = 5`", "`coverage_max_percent = 20`"],
    why:
      "Aucune taille privilegiee n'est imposee entre les bornes retenues; l'uniforme donne un eventail large mais lisible.",
    simulation:
      "Le runtime tire un entier uniforme entre 5 et 20, puis convertit ce pourcentage en aire cible.",
    parameterChoice:
      "La borne basse garde des brouillards non triviaux; la borne haute evite une occultation presque totale du plateau.",
    dependence:
      "Se combine ensuite avec l'aspect ratio et la direction pour construire la geometrie finale."
  },
  {
    title: "Allongement du brouillard",
    system: "Meteo",
    lawUse: "Uniforme continue sur un intervalle borne",
    variable: L`A \sim \mathcal{U}([1.80, 2.60])`,
    phenomenon:
      "Fixe l'allongement principal du brouillard avant deformation par le bruit de contour.",
    parameters: ["`aspect_ratio_min_times_100 = 180`", "`aspect_ratio_max_times_100 = 260`"],
    why:
      "Le brouillard doit rester anisotrope sans toujours avoir la meme excentricite; une plage uniforme controle cette variete.",
    simulation:
      "Le code tire un entier uniforme sur [180, 260], divise par 100, puis derive `radiusAlong` et `radiusAcross` a aire preservee.",
    parameterChoice:
      "Des ratios entre 1.8 et 2.6 donnent des bandes visibles sans tomber dans la ligne presque degenerate.",
    dependence:
      "Partage la meme seed d'evenement que la couverture et les graines de contour/densite du brouillard courant."
  },
  {
    title: "Graine de forme du brouillard",
    system: "Meteo",
    lawUse: "Uniforme discrete sur 32 bits",
    variable: L`S_{shape} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Fournit la graine du bruit qui perturbe le bord du brouillard.",
    parameters: ["1 tirage `generator()` par brouillard"],
    why:
      "Le brouillard doit posseder une signature spatiale propre sans collision visuelle trop frequente.",
    simulation:
      "Une sortie brute du `mt19937` de l'evenement est recopiee dans le descripteur du brouillard.",
    parameterChoice:
      "Le support de 32 bits est suffisant pour differencier des milliers de brouillards sans repetition perceptible.",
    dependence:
      "Depend du meme generateur d'evenement que la direction, l'aire et l'allongement du brouillard."
  },
  {
    title: "Graine de densite du brouillard",
    system: "Meteo",
    lawUse: "Uniforme discrete sur 32 bits",
    variable: L`S_{dens} \in \{0,\dots,2^{32}-1\}`,
    phenomenon:
      "Fournit la graine qui module localement l'opacite du brouillard via une loi log-normale.",
    parameters: ["1 tirage `generator()` par brouillard"],
    why:
      "La texture d'opacite doit etre reproductible mais differente du contour; il faut donc une graine propre.",
    simulation:
      "Le `mt19937` du brouillard produit une seconde sortie brute stockee dans le descripteur.",
    parameterChoice:
      "La separation entre la graine de forme et la graine de densite evite de coupler rigidement contour et densite locale.",
    dependence:
      "Couplee au meme evenement de spawn que la graine de forme, elle reste exploitee dans une chaine de hachage distincte par cellule."
  },
  {
    title: "Apparition de secours sur la frontiere pour une piece du diable",
    system: "Pieces du diable",
    lawUse: "Uniforme discrete sur les cases de bord encore admissibles",
    variable: L`B \sim \mathcal{U}_d(A_{bord})`,
    phenomenon:
      "Quand aucune apparition ciblee n'est valide, choisit une case de frontiere parmi celles encore autorisees.",
    parameters: ["ensemble conditionne par le type de piece, le relief et l'occupation"],
    why:
      "En situation de repli, toutes les issues de bord restantes jouent le meme role logique.",
    simulation:
      "Le systeme construit la liste `candidates`, puis appelle `std::uniform_int_distribution<std::size_t>` sur sa taille.",
    parameterChoice:
      "La fallback policy minimale reduit les heuristiques supplementaires quand la cible principale est impossible.",
    dependence:
      "Depend fortement du plateau courant, des pieces visibles et du type manifeste de la piece du diable."
  },
  {
    title: "Choix d'un mouvement aleatoire en phase Searching",
    system: "Pieces du diable",
    lawUse: "Uniforme discrete sur les coups admissibles",
    variable: L`M \sim \mathcal{U}_d(A_{moves})`,
    phenomenon:
      "Selectionne un coup quand la piece du diable entre dans sa **phase de recherche** (`Searching`), c'est-a-dire le moment ou elle explore les coups encore possibles au lieu de poursuivre une cible deja fixee.",
    parameters: ["support = coups generes, puis filtres par la visibilite locale et les collisions interdites"],
    why:
      "Une fois le mode aleatoire active, aucun coup restant n'est prioritaire dans cette branche specifique du comportement.",
    simulation:
      "Le code genere tous les coups, filtre ceux qui violent la logique de visibilite, puis tire un index uniforme.",
    parameterChoice:
      "L'uniformite limite l'introduction d'une seconde heuristique dans un chemin deja declenche de facon probabiliste.",
    dependence:
      "Conditionne par le tirage bernoulli d'entree dans cette branche et par l'ensemble de coups restants."
  },
  {
    title: "Tie-break de retour vers le bord",
    system: "Pieces du diable",
    lawUse: "Uniforme discrete sur les sorties equivalentes",
    variable: L`R_{edge} \sim \mathcal{U}_d(A_{eq})`,
    phenomenon:
      "Departage plusieurs trajectoires de repli equivalentes quand la piece du diable veut revenir vers un bord.",
    parameters: ["support = directions a meme cout de chemin"],
    why:
      "Les options de meme cout ne doivent pas etre ordonnees arbitrairement par l'ordre de parcours du code.",
    simulation:
      "Le tie-break se fait par tirage uniforme sur le sous-ensemble des directions ex aequo.",
    parameterChoice:
      "Un tirage conditionnel est suffisant car seules les directions de cout minimal entrent dans le support.",
    dependence:
      "Depend d'une analyse de plus court chemin prealable, donc du terrain et des obstacles courants."
  }
];

const permutationProcesses = [
  {
    title: "Ordre de placement des mines et fermes neutres",
    system: "Carte",
    lawUse: "Permutation uniforme sur les objets a placer",
    variable: L`\Pi \in \mathfrak{S}_n`,
    phenomenon:
      "Melange l'ordre dans lequel les fermes et les mines sont inserees sur la carte avant choix de position.",
    parameters: ["`n = num_mines + num_farms = 5` dans la config actuelle"],
    why:
      "Une permutation uniforme supprime un biais systematique du type 'les mines sont toujours placees avant les fermes'.",
    simulation:
      "Le generateur appelle `std::shuffle(placements.begin(), placements.end(), random)` avant la boucle de placement.",
    parameterChoice:
      "La taille `n = 5` vient directement de `num_mines = 2` et `num_farms = 3`.",
    dependence:
      "Influe indirectement sur tout le reste du placement public car les meilleurs candidats changent apres chaque insertion."
  }
];

const categoricalProcesses = [
  {
    title: "Case de spawn d'un coffre",
    system: "Coffres",
    lawUse: "Categorielle ponderee sur les cellules admissibles",
    variable: L`C \in \{c_1,\dots,c_n\}`,
    phenomenon:
      "Choisit la cellule de reapparition d'un coffre parmi les cases libres et suffisamment eloignees des deux rois.",
    parameters: [
      L`w(c)=1+\mathrm{centrality}(c)+\mathrm{contestation}(c)`,
      "`min_distance_from_kings = 6`",
      "centralite mesuree par la distance au centre, contestation par l'equilibre des distances aux deux rois"
    ],
    why:
      "Le systeme veut privilegier les zones contestables et centrales, pas seulement tirer une case uniforme.",
    simulation:
      "Le runtime calcule un poids entier pour chaque candidat puis utilise `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Le poids `1 + centrality + contestation` garantit une probabilite strictement positive pour toute case admissible.",
    dependence:
      "Depend de la position instantanee des rois et de l'occupation du plateau au moment du spawn."
  },
  {
    title: "Type de recompense du coffre",
    system: "Coffres",
    lawUse: "Categorielle ponderee a deux regimes temporels",
    variable: L`R \in \{\text{gold},\text{move},\text{build}\}`,
    phenomenon:
      "Choisit si le coffre donne de l'or, un bonus permanent du **budget de mouvement par tour** ou un bonus permanent du **budget de construction par tour**.",
    parameters: [
      "debut de partie: poids (8, 3, 3)",
      "fin de partie: poids (4, 6, 6)",
      "**Bascule a partir du tour 10** (`late_game_turn = 10`)",
      "Mode de rattrapage actif (`current_loot_catch_up_enabled = true`) : **les deux royaumes partagent la meme recompense courante** tant qu'ils ne l'ont pas tous les deux recueillie"
    ],
    why:
      "Le systeme favorise **plus d'or tres tot**, puis **davantage de capacite d'action** ensuite; une categorielle ponderee est la loi naturelle pour ce genre de choix nominal.",
    simulation:
      "`sampleReward` construit le vecteur de poids selon le tour courant, puis appelle `std::discrete_distribution<int>`.",
    parameterChoice:
      "**A partir du tour 10**, les bonus d'action prennent plus de poids afin d'accelerer le milieu de partie.",
    dependence:
      "Si le mode de rattrapage est actif, **le tirage suivant n'apparait que lorsque les deux royaumes ont deja pris la recompense courante**; l'etat `currentRewardGeneration` lie donc directement les ouvertures de coffres des deux camps."
  },
  {
    title: "Direction du brouillard",
    system: "Meteo",
    lawUse: "Categorielle ponderee sur huit directions",
    variable: L`D \in \{N,S,E,W,NE,NW,SE,SW\}`,
    phenomenon:
      "Choisit la direction cardinale ou diagonale du prochain brouillard.",
    parameters: ["les huit poids valent actuellement 1.`"],
    why:
      "Le systeme est ecrit de facon generique pour pouvoir biaiser certaines directions plus tard, mais la configuration active realise une equiprobabilite via une categorielle a poids egaux.",
    simulation:
      "Le runtime lit `direction_weights`, puis passe le tableau d'entiers a `std::discrete_distribution<int>`.",
    parameterChoice:
      "Les huit poids unitaires font de cette categorielle une uniforme deguisement, tout en gardant un point d'extension clair.",
    dependence:
      "La direction pilote ensuite le bord d'entree, la trajectoire et les rayons du brouillard."
  },
  {
    title: "Type de cible primaire d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Categorielle ponderee sur les types de pieces visibles",
    variable: L`T \in \{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    phenomenon:
      "Choisit quel type de piece ennemie la piece du diable va chercher en priorite.",
    parameters: [
      "poids actifs: pawn 8, knight 14, bishop 14, rook 26, queen 38",
      "les types absents du champ visible recoivent le poids 0"
    ],
    why:
      "Le comportement cherche une priorisation strategicue explicite: les pieces majeures doivent etre attirees plus souvent que les pions.",
    simulation:
      "Le systeme construit `typeWeights`, met a zero les types indisponibles, puis tire via `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Les poids croissants codent un ordre de valeur tactique sans rendre le choix deterministe.",
    dependence:
      "Depend de la visibilite courante et des types reellement presents chez le royaume cible."
  },
  {
    title: "Option d'apparition ciblee d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Categorielle ponderee par proximite de chemin",
    variable: L`O \in \{o_1,\dots,o_m\}`,
    phenomenon:
      "Choisit, parmi plusieurs options de projection vers une cible visible, celle qui sera retenue au moment du spawn.",
    parameters: [
      L`w(o)=\max\bigl(1, 2D-\mathrm{dist}(o)+1\bigr)`,
      "`D = board.getDiameter()`"
    ],
    why:
      "On veut favoriser les options qui approchent vite la cible sans pour autant annuler completement les autres entrees valides.",
    simulation:
      "Le runtime calcule un poids entier inversement lie a la distance de plus court chemin, puis tire avec `std::discrete_distribution`.",
    parameterChoice:
      "Le plancher a 1 conserve une queue de probabilite sur toute option encore jouable.",
    dependence:
      "Conditionne par le type de piece infernale manifestee et par le graphe de deplacements accessible."
  },
  {
    title: "Type de remplacement d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Categorielle ponderee avec bonus de persistance",
    variable: L`T' \in \{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    phenomenon:
      "Quand la cible initiale devient impossible, choisit un nouveau type de cible visible.",
    parameters: [
      "meme base de poids (8, 14, 14, 26, 38)",
      "si le type correspond a `preferredTargetType`, son poids est double"
    ],
    why:
      "Le remplacement doit rester coherent avec la priorite precedente tout en laissant une vraie possibilite de redirection.",
    simulation:
      "La boucle de remplacement reconstruit les poids restants puis applique `std::discrete_distribution<std::size_t>`.",
    parameterChoice:
      "Le doublement du poids memorise une inertie comportementale simple a expliquer et facile a regler.",
    dependence:
      "Depend du type precedemment poursuivi et des types encore visibles."
  },
  {
    title: "Cible de remplacement d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Categorielle ponderee parmi les cibles atteignables du type retenu",
    variable: L`Y \in \{y_1,\dots,y_r\}`,
    phenomenon:
      "Choisit la cible concrete une fois le type de remplacement fixe.",
    parameters: [
      L`w(y)=\max\bigl(1, 2D-\mathrm{dist}(y)+1\bigr)`,
      "seules les cibles atteignables sont conservees"
    ],
    why:
      "La priorite reste donnee aux cibles rapides d'acces, mais le tirage conserve de la diversite tactique.",
    simulation:
      "Le code reconstruit `reachableTargets` et `reachableWeights`, puis echantillonne une cible par `std::discrete_distribution`.",
    parameterChoice:
      "Le meme schema de poids que pour les options d'apparition maintient une logique unique de proximite pour les pieces du diable.",
    dependence:
      "Fortement couple a l'etat du plateau, au type retenu juste avant et au masque de visibilite meteo."
  }
];

const bernoulliProcesses = [
  {
    title: "Royaume cible d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Bernoulli a probabilite d'etat",
    variable: L`K \sim \mathrm{Bernoulli}(p_t)`,
    phenomenon:
      "Choisit si la piece du diable cible le royaume blanc ou noir quand les deux sont eligibles.",
    parameters: [
      L`p_t = \frac{\mathrm{debt}_{white}}{\mathrm{debt}_{white}+\mathrm{debt}_{black}}`,
      L`p_t = 0.5 \text{ si la dette totale vaut } 0`
    ],
    why:
      "Une Bernoulli suffit des qu'il n'y a que deux royaumes eligibles; l'etat du jeu deforme directement la probabilite.",
    simulation:
      "Le code calcule `whiteProbability`, le borne dans [0,1], puis appelle `std::bernoulli_distribution`.",
    parameterChoice:
      "La probabilite proportionnelle a la dette de sang rend le systeme reactif aux pertes infligees a chaque camp.",
    dependence:
      "Tres dependante de l'historique des destructions, donc non stationnaire sur une partie."
  },
  {
    title: "Activation d'un mouvement aleatoire en phase Searching",
    system: "Pieces du diable",
    lawUse: "Bernoulli simple",
    variable: L`B \sim \mathrm{Bernoulli}(0.333)`,
    phenomenon:
      "Decide si, lors d'un tour de **phase de recherche** (`Searching`), la piece du diable tente effectivement un mouvement purement aleatoire plutot qu'un deplacement entierement pilote par ses heuristiques.",
    parameters: ["probabilite de 33,3 % (`searching_random_move_chance_times_1000 = 333`)"],
    why:
      "Il s'agit d'un interrupteur oui/non sur une branche comportementale unique; la Bernoulli est la loi minimale adequate.",
    simulation:
      "Le systeme tire un entier uniforme sur [0, 999] et compare au seuil 333, ce qui realise une Bernoulli discretisee.",
    parameterChoice:
      "Une probabilite proche de 1/3 maintient de la pression sans rendre la branche erratique dominante.",
    dependence:
      "Conditionne par l'existence d'au moins un coup admissible apres filtrage."
  }
];

const poissonProcesses = [
  {
    title: "Declenchement d'apparition d'une piece du diable",
    system: "Pieces du diable",
    lawUse: "Poisson observee via l'evenement {N >= 1}",
    variable: L`N \sim \mathrm{Poisson}(\lambda_t)`,
    phenomenon:
      "Determine si une nouvelle apparition d'une piece du diable se declenche a ce tour.",
    parameters: [
      L`\lambda_t = \min(0.25, 0.02 + 0.012\,\mathrm{debt}_t)`,
      "`poisson_lambda_base_times_1000 = 20`",
      "`poisson_lambda_per_debt_times_1000 = 12`",
      "`poisson_lambda_cap_times_1000 = 250`"
    ],
    why:
      "Le design veut un compteur d'arrivees rares dont l'intensite croit avec la dette de sang; la Poisson est le modele naturel de comptage d'evenements.",
    simulation:
      "Le runtime echantillonne `std::poisson_distribution<int>(lambda)` puis declenche le spawn si le resultat est au moins 1.",
    parameterChoice:
      "Le cap a 0.25 borne `P(N \\ge 1) = 1 - e^{-\\lambda}` en dessous de 0.221, donc les pieces du diable restent menaçantes sans saturer la partie.",
    dependence:
      "`\\lambda_t` depend de la dette aggregatee, elle-meme mise a jour a chaque perte ou degat structurel."
  }
];

const truncatedNormalProcesses = [
  {
    title: "Recompenses d'XP",
    system: "XP",
    lawUse: "Normale tronquee puis arrondie a l'entier",
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
      "Une variable gaussienne tronquee preserve une moyenne intuitive tout en autorisant une dispersion controlee autour de chaque source d'XP.",
    simulation:
      "`RewardProfileSampling::sampleTruncatedNormal` tire une normale, la tronque sur `[mean - delta, mean + delta]`, puis arrondit et applique le minimum.",
    parameterChoice:
      "Le sigma est proportionnel a la moyenne via `sigma_multiplier_times_100`, ce qui garde une volatilite relative comparable entre petites et grosses recompenses.",
    dependence:
      "Les tirages sont independants conditionnellement au profil choisi, mais le profil depend du type d'evenement de jeu."
  },
  {
    title: "Montant d'or d'un coffre",
    system: "Coffres",
    lawUse: "Normale tronquee puis arrondie a l'entier",
    variable: L`G = \max\bigl(1, \mathrm{round}(\mathrm{clip}(X,[\mu-2\sigma,\mu+2\sigma]))\bigr)`,
    phenomenon:
      "Quand la recompense choisie est de l'or, fixe le montant reel donne au joueur.",
    parameters: [
      "`mean = 35`",
      "`sigma_multiplier_times_100 = 18`, donc sigma = 6.3",
      "`clamp_sigma_multiplier_times_100 = 200`, donc troncature a +/- 12.6",
      "`minimum = 1`"
    ],
    why:
      "La meme famille que pour l'XP permet d'obtenir une valeur centrale stable, avec un peu de volatilite sans outliers gigantesques.",
    simulation:
      "Le chemin `sampleGoldRewardAmount -> sampleTruncatedNormal` reutilise exactement le moteur commun de profils de recompense.",
    parameterChoice:
      "La moyenne 35 est coherente avec l'economie initiale et reste bien au-dessus du minimum meme apres troncature.",
    dependence:
      "Conditionne par le fait que la categorielle de type de recompense ait deja choisi la branche or."
  }
];

const weibullProcesses = [
  {
    title: "Delai de reapparition d'un coffre",
    system: "Coffres",
    lawUse: "Weibull discretisee et tronquee inferieurement",
    variable: L`D = \max\bigl(c, \mathrm{round}(T)\bigr),\quad T \sim \mathrm{Weibull}(k,\lambda)`,
    phenomenon:
      "Fixe le nombre de tours a attendre avant le prochain coffre.",
    parameters: [
      "`k = 1.80` via `weibull_shape_times_100 = 180`",
      "`lambda = 6` tours via `weibull_scale_turns = 6`",
      "cooldown plancher `c = 4` tours",
      "tour minimal de tout premier spawn = 4"
    ],
    why:
      "La Weibull est adaptee aux temps d'attente flexibles: avec `k > 1`, le hazard croissant rend les reapparitions plus plausibles apres plusieurs tours sans coffre.",
    simulation:
      "`sampleSpawnDelay` echantillonne `std::weibull_distribution<double>(shape, scale)`, arrondit, puis applique `max(respawnCooldown, randomDelay)`.",
    parameterChoice:
      "`k = 1.8` garde des delais variables tout en evitant une concentration trop forte pres de zero.",
    dependence:
      "Le delai est resample a chaque collecte ou echec de spawn, mais la logique de placement peut encore reporter l'apparition."
  }
];

const gammaProcesses = [
  {
    title: "Delai entre deux brouillards",
    system: "Meteo",
    lawUse: "Gamma discretisee par plafond",
    variable: L`D = m + \lceil T \rceil,\quad T \sim \Gamma(k,\theta)`,
    phenomenon:
      "Fixe le nombre de tours avant le prochain essai d'apparition d'un brouillard.",
    parameters: [
      "config active: `k = 4.00`, `theta = 10.00`, minimum `m = 0`",
      "par heritage code, la version par defaut etait `k = 3.20`, `theta = 2.40`"
    ],
    why:
      "Une Gamma controle naturellement des temps d'attente positifs et asymetriques, plus souples qu'une exponentielle simple.",
    simulation:
      "`scheduleNextSpawn` appelle `sampleGammaTurns`, qui echantillonne `std::gamma_distribution`, prend le plafond puis convertit en pas de temps.",
    parameterChoice:
      "Le passage par la config permet de rallonger ou compresser tres simplement la cadence globale des brouillards sans toucher au code.",
    dependence:
      "La tentative suivante reste aussi bloquee tant qu'un brouillard actif occupe deja la carte."
  },
  {
    title: "Duree visible d'un brouillard",
    system: "Meteo",
    lawUse: "Gamma discretisee par plafond",
    variable: L`V = \max(1, \lceil T \rceil),\quad T \sim \Gamma(k,\theta)`,
    phenomenon:
      "Fixe le nombre de tours pendant lesquels le brouillard doit rester sensiblement visible avant de quitter la carte.",
    parameters: [
      "`k = 2.60` via `duration_gamma_shape_times_100 = 260`",
      "`theta = 1.80` via `duration_gamma_scale_times_100 = 180`",
      "minimum logique applique ensuite: au moins 1 tour visible"
    ],
    why:
      "Une duree positive et asymetrique est mieux modelee par une Gamma que par une loi symetrique, surtout pour eviter des vies negatives ou quasi nulles.",
    simulation:
      "Le runtime tire `visibleTurnCount`, convertit en nombre de pas, puis adapte le trajet et l'elongation du brouillard pour respecter cette cible temporelle.",
    parameterChoice:
      "La moyenne continue `k\theta = 4.68` tours donne des brouillards visibles mais pas permanents.",
    dependence:
      "La duree interagit ensuite avec la vitesse, l'aire preservee et la geometre du plateau."
  }
];

const logNormalProcesses = [
  {
    title: "Densite locale d'un brouillard",
    system: "Meteo",
    lawUse: "Log-normale cellule par cellule, puis clamp d'alpha",
    variable: L`X(c) \sim \mathrm{LogNormal}(\mu,\sigma^2)`,
    phenomenon:
      "Multiplie l'opacite locale du brouillard pour obtenir des zones plus ou moins opaques a l'interieur d'une meme masse nuageuse.",
    parameters: [
      "`mu = -0.12` via `density_mu_times_100 = -12`",
      "`sigma = 0.35` via `density_sigma_times_100 = 35`",
      "alpha local = clamp(0.48 * X(c), 0.22, 0.82)"
    ],
    why:
      "La log-normale garantit des multiplicateurs strictement positifs, avec une queue a droite utile pour creer quelques poches tres opaques sans valeurs negatives.",
    simulation:
      "`sampleLogNormalCell` rederive un generateur par cellule a partir de `densitySeed`, puis echantillonne `std::lognormal_distribution<double>(mu, sigma)`.",
    parameterChoice:
      "La moyenne geometrique legerement sous 1 et un sigma modere donnent surtout des variations fines, ensuite bornees par l'alpha min/max.",
    dependence:
      "Toutes les cellules d'un meme brouillard partagent la meme graine de densite; le champ n'est donc pas i.i.d. (**independant et identiquement distribué**) a l'echelle du brouillard."
  }
];

const betaProcesses = [
  {
    title: "Luminosite de l'herbe",
    system: "Carte",
    lawUse: "Beta transformee par seuil et contraste",
    variable: L`B \sim \mathrm{Beta}(7,2)`,
    phenomenon:
      "Module la luminosite des cellules d'herbe pour casser l'uniformite du tapis vert sans toucher aux autres terrains.",
    parameters: [
      "`alpha = 7`, `beta = 2`",
      "`keep_default_threshold = 0.90`",
      "`min_brightness = 0.68`",
      "`contrast_exponent = 1.8`"
    ],
    why:
      "Avec `alpha > beta`, la Beta concentre la masse pres de 1, ce qui laisse la plupart des herbes proches de la luminosite nominale tout en autorisant quelques assombrissements visibles.",
    simulation:
      "Le code echantillonne la Beta via deux Gammas, applique un seuil a 0.90, puis remappe la partie basse vers `[0.68, 1]` avec une puissance 1.8.",
    parameterChoice:
      "Le couple (7, 2) et le seuil 0.90 donnent un plateau majoritairement clair, avec juste assez d'irregularite pour casser la repetition.",
    dependence:
      "Chaque cellule derive son seed d'un hachage de `worldSeed` et de sa position, donc le rendu est fixe pour un monde donne."
  }
];

const piecewiseLinearProcesses = [
  {
    title: "Position d'entree le long du bord d'un brouillard",
    system: "Meteo",
    lawUse: "Piecewise linear sur la coordonnee de bord",
    variable: L`X \in [0,M]`,
    phenomenon:
      "Choisit la position continue du centre du brouillard le long du bord d'entree.",
    parameters: [
      L`\text{noeuds } (0, \tfrac14 M, \tfrac12 M, \tfrac34 M, M)`,
      "poids `(0.7, 1.8, 1.98, 1.8, 0.7)` dans la config active",
      "avec `1.98 = 1.1 * 1.8` pour le point median"
    ],
    why:
      "Le jeu veut privilegier les entrees centrales tout en conservant une probabilite non nulle de depart par les coins; la piecewise linear est ideale pour cette densite dessinee a la main.",
    simulation:
      "`sampleEdgePosition` construit les bornes et les hauteurs puis utilise `std::piecewise_linear_distribution<double>`.",
    parameterChoice:
      "Le centre est volontairement surpondere par rapport aux quarts et aux coins pour produire des brouillards plus lisibles visuellement.",
    dependence:
      "Depend ensuite du bord retenu et de la direction du brouillard pour se convertir en coordonnees 2D."
  }
];

const proceduralProcesses = [
  {
    title: "Champ spatial de la terre",
    system: "Carte",
    lawUse: "Champ procedural correle derive de bruit value/fBm",
    variable: L`X_{terre}(c) = g_{S_{terre}}(c)`,
    phenomenon:
      "Produit des zones de terre connexes plutot que des cellules i.i.d. independantes.",
    parameters: [
      "`terrain_noise_scale = 14`",
      "`terrain_octaves = 3`",
      "couverture cible terre = 14 %",
      "post-traitement par composantes et amas: 6 amas, rayon 2 a 5"
    ],
    why:
      "Une loi usuelle scalaire ne suffit pas ici: il faut un champ spatial correle pour faire emerger des taches organiques.",
    simulation:
      "Le generateur evalue `valueNoise` puis `fractalNoise`, applique des seuils, conserve les composantes coherentes et ajoute des amas locaux de terre.",
    parameterChoice:
      "Les trois octaves donnent deja un relief suffisant sans rendre le calcul couteux sur tout le plateau.",
    dependence:
      "Forte correlation spatiale: des cellules voisines partagent la meme graine et des frequences proches."
  },
  {
    title: "Champ spatial de l'eau",
    system: "Carte",
    lawUse: "Champ procedural correle derive de bruit value/fBm",
    variable: L`X_{eau}(c) = h_{S_{eau}}(c)`,
    phenomenon:
      "Construit les poches d'eau et les petits lacs sans casser la jouabilite du plateau.",
    parameters: [
      "couverture cible eau = 4 %",
      "post-traitement par 3 lacs de rayon 2 a 3",
      "meme echelle et meme nombre d'octaves que la terre"
    ],
    why:
      "Comme pour la terre, on veut des zones spatialement coherentes, pas une Bernoulli par cellule qui gribouillerait le plateau.",
    simulation:
      "Le pipeline rederive un score de bruit, applique un seuil propre a l'eau, filtre par composantes puis injecte quelques lacs complementaires.",
    parameterChoice:
      "La faible couverture 4 % evite de couper brutalement les couloirs de circulation du jeu.",
    dependence:
      "Correlation spatiale importante et dependance indirecte au champ de la terre via les contraintes d'assemblage du plateau final."
  },
  {
    title: "Masque de flip des textures de terrain",
    system: "Carte",
    lawUse: "Pseudo-uniforme par hachage de position",
    variable: L`F(c) = \mathrm{hash}(worldSeed, type, c) \bmod 4`,
    phenomenon:
      "Retourne horizontalement et/ou verticalement les textures de terrain pour casser les repetitions visibles.",
    parameters: ["4 etats de flip", "hachage positionnel avec `worldSeed` et `CellType`"],
    why:
      "Le besoin principal est la reproductibilite locale, pas un echantillonnage i.i.d. complet a chaque frame.",
    simulation:
      "`terrainFlipMaskFor` rederive un seed melange, hache la position puis conserve les deux bits de flip utiles.",
    parameterChoice:
      "Conserver uniquement deux bits suffit pour coder aucun flip, horizontal, vertical ou double flip.",
    dependence:
      "Determinisme strict par cellule; dependence quasi nulle a longue distance mais pas modelisee comme une loi scalaire autonome."
  },
  {
    title: "Bruit de contour du brouillard",
    system: "Meteo",
    lawUse: "Champ procedurale de bord via value noise",
    variable: L`B(c) = 1 + (U(c)-0.5)\,a`,
    phenomenon:
      "Deforme la frontiere theorique du brouillard pour obtenir un contour nuageux irregulier.",
    parameters: [
      "`shape_noise_cell_span = 6`",
      "`shape_noise_amplitude_percent = 100`, donc `a = 1`",
      "`edge_softness_percent = 18`"
    ],
    why:
      "Un brouillard sans bruit aurait une silhouette trop analytique. Ici encore, il faut une fonction spatiale correlee plutot qu'une suite i.i.d. de variables.",
    simulation:
      "Le code evalue `valueNoise(shapeSeed, x, y, span)`, deforme la limite effective du brouillard, puis applique un fondu par `edgeSoftness`.",
    parameterChoice:
      "Une amplitude de 100 % autorise des bosses visibles, ensuite lisses par la grande echelle `span = 6` et le fondu de bord.",
    dependence:
      "Toutes les cellules du meme brouillard partagent la meme graine de forme, donc la correlation spatiale est intentionnellement forte."
  }
];

function createTheory({ support, law, expectation, variance, note = "" }) {
  return { support, law, expectation, variance, note };
}

const NOMINAL_EXPECTATION = L`\text{Pas d esperance canonique sans score auxiliaire } g`;
const NOMINAL_VARIANCE = L`\text{Pas de variance canonique sans score auxiliaire } g`;
const POSITION_EXPECTATION = L`\text{Pas d esperance scalaire canonique sur une position 2D}`;
const POSITION_VARIANCE = L`\text{Pas de variance scalaire canonique sur une position 2D}`;
const FIELD_EXPECTATION = L`\text{Le resume pertinent est spatial ou agrégé, pas cellule par cellule}`;
const FIELD_VARIANCE = L`\text{La variance utile est estimee sur la couverture ou la rugosite du champ}`;

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
    "Les moments ci-dessus sont surtout formels: l'enjeu gameplay reel est l'uniformite de la seed et sa reproductibilite."
  ),
  "Graine globale de l'eau": makeUniform32Theory(
    L`S_{eau}`,
    "Comme pour la terre, la moyenne n'est pas interpretee directement en jeu; la seed sert a indexer un monde procedural reproductible."
  ),
  "Rotation des mines et fermes neutres": makeUniformFiniteTheory({
    support: L`\{0,1,2,3\}`,
    law: L`\mathbb{P}(R=r)=\frac{1}{4}`,
    expectation: L`\mathbb{E}[R]=\frac{3}{2}`,
    variance: L`\mathrm{Var}(R)=\frac{5}{4}`,
    note: "Le codage 0,1,2,3 represente simplement les quatre quarts de tour equiprobables."
  }),
  "Flip des mines et fermes neutres": makeUniformFiniteTheory({
    support: L`\{0,1,2,3\}`,
    law: L`\mathbb{P}(F=f)=\frac{1}{4}`,
    expectation: L`\mathbb{E}[F]=\frac{3}{2}`,
    variance: L`\mathrm{Var}(F)=\frac{5}{4}`,
    note: "On reutilise un codage a deux bits pour les quatre etats de flip possibles."
  }),
  "Choix de position des batiments publics": makePositionTheory({
    support: L`A_{top}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P=p\mid p\in A_{top})=\frac{1}{|A_{top}|}`,
    note:
      "La loi est uniforme sur les cellules admissibles du plateau courant, mais une position 2D n'a pas de moment scalaire canonique tant qu'on n'introduit pas une distance ou un score."
  }),
  "Spawn des royaumes": makePositionTheory({
    support: L`A_W\cup A_B\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P_K=p\mid p\in A_K)=\frac{1}{|A_K|}`,
    note:
      "Chaque royaume echantillonne uniformement une case de spawn compatible avec sa zone. Le bon objet mathematique est une position, pas un scalaire."
  }),
  "Bord diagonal d'entree du brouillard": makeNominalTheory({
    support: L`\{e_1,e_2\}`,
    law: L`\mathbb{P}(E=e_i)=\frac{1}{2}`,
    note: "Les deux diagonales admissibles sont symetriques et equiprobables."
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
    note: "La valeur echantillonnee est ensuite transformee en ellipse et trajectoire discretes."
  }),
  "Graine de forme du brouillard": makeUniform32Theory(
    L`S_{forme}`,
    "La seed n'est pas interpretee seule: elle alimente le bruit de contour et doit surtout etre uniformement repartie."
  ),
  "Graine de densite du brouillard": makeUniform32Theory(
    L`S_{densite}`,
    "Cette seed conditionne tout le champ d'opacite local du brouillard."
  ),
  "Apparition de secours sur la frontiere pour une piece du diable": makePositionTheory({
    support: L`A_{bord}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(P=p\mid p\in A_{bord})=\frac{1}{|A_{bord}|}`,
    note:
      "Le support depend des cases frontiere encore libres au moment du spawn de secours."
  }),
  "Choix d'un mouvement aleatoire en phase Searching": makeNominalTheory({
    support: L`\mathcal{M}_{adm}(t)`,
    law: L`\mathbb{P}(M=m\mid m\in \mathcal{M}_{adm}(t))=\frac{1}{|\mathcal{M}_{adm}(t)|}`,
    note:
      "Le support est l'ensemble des coups legalement atteignables pour la piece au tour courant, donc il change avec l'etat du plateau."
  }),
  "Tie-break de retour vers le bord": makeNominalTheory({
    support: L`\mathcal{D}_{eq}(t)`,
    law: L`\mathbb{P}(D=d\mid d\in \mathcal{D}_{eq}(t))=\frac{1}{|\mathcal{D}_{eq}(t)|}`,
    note:
      "Cette uniforme ne sert qu'a trancher entre plusieurs directions equivalentes lors du retour vers le bord."
  }),
  "Ordre de placement des mines et fermes neutres": createTheory({
    support: L`\mathfrak{S}_5`,
    law: L`\mathbb{P}(\Pi=\pi)=\frac{1}{5!}`,
    expectation: L`\mathbb{E}[R_i]=\frac{5+1}{2}=3`,
    variance: L`\mathrm{Var}(R_i)=\frac{5^2-1}{12}=2`,
    note:
      "Une permutation n'a pas de moment canonique en tant qu'objet global; les moments affiches sont ceux du rang d'un batiment fixe dans l'ordre melange."
  }),
  "Case de spawn d'un coffre": makePositionTheory({
    support: L`A_{coffre}\subset \mathbb{Z}^2`,
    law: L`\mathbb{P}(C=c_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Les poids favorisent certaines cellules admissibles, mais l'analyse de moyenne/variance ne devient pertinente qu'apres choix d'un score spatial auxiliaire."
  }),
  "Type de recompense du coffre": makeNominalTheory({
    support: L`\{\text{or},\text{mouvement},\text{construction}\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "La variable est nominale: on ne prend pas la moyenne d'indices arbitraires, on etudie les probabilites de chaque categorie."
  }),
  "Direction du brouillard": makeNominalTheory({
    support: L`\{N,S,E,O,NE,NO,SE,SO\}`,
    law: L`\mathbb{P}(D=d_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "La direction est une categorie orientee; l'objet statistique central est la frequence de chaque orientation."
  }),
  "Type de cible primaire d'une piece du diable": makeNominalTheory({
    support: L`\{t_1,\dots,t_m\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le code choisit une famille de cible prioritaire plutot qu'une grandeur numerique."
  }),
  "Option d'apparition ciblee d'une piece du diable": makeNominalTheory({
    support: L`\{o_1,\dots,o_m\}`,
    law: L`\mathbb{P}(O=o_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Cette categorielle arbitre entre plusieurs heuristiques de spawn cible. Les poids varient avec l'etat tactique."
  }),
  "Type de remplacement d'une piece du diable": makeNominalTheory({
    support: L`\{\text{pawn},\text{knight},\text{bishop},\text{rook},\text{queen}\}`,
    law: L`\mathbb{P}(T=t_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le tirage porte sur une categorie de piece, pas sur une valeur numerique ordonnee."
  }),
  "Cible de remplacement d'une piece du diable": makeNominalTheory({
    support: L`\{c_1,\dots,c_m\}`,
    law: L`\mathbb{P}(C=c_i)=\frac{w_i}{\sum_j w_j}`,
    note:
      "Le support regroupe les cibles admissibles pour le remplacement. On compare donc des poids et non des moyennes."
  }),
  "Royaume cible d'une piece du diable": createTheory({
    support: L`K_t\in\{0,1\}`,
    law: L`K_t\sim\mathrm{Bernoulli}(p_t)`,
    expectation: L`\mathbb{E}[K_t]=p_t`,
    variance: L`\mathrm{Var}(K_t)=p_t(1-p_t)`,
    note:
      "Le parametre dynamique `p_t` est derive de la dette de sang normalisee; 1 peut etre code comme 'royaume blanc cible'."
  }),
  "Activation d'un mouvement aleatoire en phase Searching": createTheory({
    support: L`A_t\in\{0,1\}`,
    law: L`A_t\sim\mathrm{Bernoulli}(p),\qquad p=0.333`,
    expectation: L`\mathbb{E}[A_t]=p=0.333`,
    variance: L`\mathrm{Var}(A_t)=p(1-p)`,
    note:
      "Cette Bernoulli decide si la piece infernale abandonne sa trajectoire guidee pour un coup aleatoire au tour courant."
  }),
  "Declenchement d'apparition d'une piece du diable": createTheory({
    support: L`N_t\in\mathbb{N}`,
    law: L`N_t\sim\mathrm{Poisson}(\lambda_t)`,
    expectation: L`\mathbb{E}[N_t]=\lambda_t`,
    variance: L`\mathrm{Var}(N_t)=\lambda_t`,
    note:
      "Le runtime transforme ensuite ce comptage en evenement booléen via `N_t \ge 1`, mais la loi parente reste bien une Poisson."
  }),
  "Recompenses d'XP": createTheory({
    support: L`Y\in\{m,m+1,\dots,b\}`,
    law: L`Y=\max\!\bigl(m,\mathrm{round}(\mathrm{clip}(X,[a,b]))\bigr),\quad X\sim\mathcal{N}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\,\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}`,
    variance: L`\mathrm{Var}(X\mid a\le X\le b)=\sigma^2\!\left[1+\frac{\alpha\varphi(\alpha)-\beta\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}-\left(\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}\right)^2\right]`,
    note:
      "Les formules affichent les moments de la normale tronquee parente, avec `\alpha=(a-\mu)/\sigma` et `\beta=(b-\mu)/\sigma`. L'arrondi et le minimum du runtime deplacent legerement la moyenne finale."
  }),
  "Montant d'or d'un coffre": createTheory({
    support: L`G\in\{1,2,\dots\}`,
    law: L`G=\max\!\bigl(1,\mathrm{round}(\mathrm{clip}(X,[\mu-2\sigma,\mu+2\sigma]))\bigr),\quad X\sim\mathcal{N}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\,\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}`,
    variance: L`\mathrm{Var}(X\mid a\le X\le b)=\sigma^2\!\left[1+\frac{\alpha\varphi(\alpha)-\beta\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}-\left(\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)}\right)^2\right]`,
    note:
      "On note ici `a=\mu-2\sigma` et `b=\mu+2\sigma`. Comme pour l'XP, le runtime applique ensuite un arrondi et un plancher a 1."
  }),
  "Delai de reapparition d'un coffre": createTheory({
    support: L`D\in\{c,c+1,\dots\}`,
    law: L`D=\max\!\bigl(c,\mathrm{round}(T)\bigr),\quad T\sim\mathrm{Weibull}(k,\lambda)`,
    expectation: L`\mathbb{E}[T]=\lambda\,\Gamma\!\left(1+\frac{1}{k}\right),\qquad \mathbb{E}[D]\approx \max\!\left(c,\mathbb{E}[T]\right)`,
    variance: L`\mathrm{Var}(T)=\lambda^2\!\left[\Gamma\!\left(1+\frac{2}{k}\right)-\Gamma\!\left(1+\frac{1}{k}\right)^2\right]`,
    note:
      "Les moments fermes ci-dessus sont ceux de la Weibull continue; l'effet exact du `round` puis du plancher `c` est traite empiriquement dans les histogrammes."
  }),
  "Delai entre deux brouillards": createTheory({
    support: L`D\in\{m,m+1,\dots\}`,
    law: L`D=m+\lceil T\rceil,\quad T\sim\Gamma(k,\theta)`,
    expectation: L`\mathbb{E}[T]=k\theta,\qquad \mathbb{E}[D]\approx m+k\theta`,
    variance: L`\mathrm{Var}(T)=k\theta^2`,
    note:
      "Le plafond discretise la variable continue. Les moments affiches sont donc des references theoriques autour desquelles le runtime se concentre."
  }),
  "Duree visible d'un brouillard": createTheory({
    support: L`V\in\{1,2,\dots\}`,
    law: L`V=\max(1,\lceil T\rceil),\quad T\sim\Gamma(k,\theta)`,
    expectation: L`\mathbb{E}[T]=k\theta,\qquad \mathbb{E}[V]\approx \max(1,k\theta)`,
    variance: L`\mathrm{Var}(T)=k\theta^2`,
    note:
      "La Gamma fournit la duree cible continue, puis le runtime la convertit en nombre entier de tours visibles."
  }),
  "Densite locale d'un brouillard": createTheory({
    support: L`A(c)\in[0.22,0.82]`,
    law: L`A(c)=\mathrm{clip}(0.48\,X(c),0.22,0.82),\quad X(c)\sim\mathrm{LogNormal}(\mu,\sigma^2)`,
    expectation: L`\mathbb{E}[X(c)]=e^{\mu+\sigma^2/2}`,
    variance: L`\mathrm{Var}(X(c))=(e^{\sigma^2}-1)e^{2\mu+\sigma^2}`,
    note:
      "Les moments affiches sont ceux de la log-normale parente. Le `clip` d'alpha et le partage d'une meme seed de densite deformant ensuite la loi finale observee."
  }),
  "Luminosite de l'herbe": createTheory({
    support: L`Y\in[0.68,1]`,
    law: L`Y=h(B),\quad B\sim\mathrm{Beta}(7,2)`,
    expectation: L`\mathbb{E}[Y]=\int_0^1 h(b)\,f_{\mathrm{Beta}(7,2)}(b)\,db`,
    variance: L`\mathrm{Var}(Y)=\int_0^1 h(b)^2 f_{\mathrm{Beta}(7,2)}(b)\,db-\mathbb{E}[Y]^2`,
    note:
      "La transformation deterministe `h` encode le seuil a 0.90, le remappage vers `[0.68,1]` et l'exposant de contraste 1.8."
  }),
  "Position d'entree le long du bord d'un brouillard": createTheory({
    support: L`X\in[0,M]`,
    law: L`f_X(x)=\mathrm{piecewiseLinear}(x;\,0,\tfrac14M,\tfrac12M,\tfrac34M,M;\,0.7,1.8,1.98,1.8,0.7)`,
    expectation: L`\mathbb{E}[X]=\int_0^M x\,f_X(x)\,dx`,
    variance: L`\mathrm{Var}(X)=\int_0^M x^2 f_X(x)\,dx-\mathbb{E}[X]^2`,
    note:
      "Les integrales sont calculees numeriquement par la bibliotheque de distribution piecewise linear, ce qui colle exactement a la methode de simulation du runtime."
  }),
  "Champ spatial de la terre": createTheory({
    support: L`X_{terre}:\mathcal{G}\to\{0,1\}`,
    law: L`X_{terre}(c)=\mathbf{1}\{n_{terre}(c;S_{terre})+p_{terre}(c)>\tau_{terre}\}`,
    expectation: L`\mathbb{E}[\bar X_{terre}]\approx 0.14,\qquad \bar X_{terre}=\frac{1}{|\mathcal{G}|}\sum_{c\in\mathcal{G}} X_{terre}(c)`,
    variance: L`\mathrm{Var}(\bar X_{terre})\text{ est estimee par simulation car les cellules sont correlees}`,
    note:
      "Pour un champ procedural, on ne presente pas la moyenne d'une cellule abstraite mais celle d'un resume interpretable: ici la couverture totale en terre."
  }),
  "Champ spatial de l'eau": createTheory({
    support: L`X_{eau}:\mathcal{G}\to\{0,1\}`,
    law: L`X_{eau}(c)=\mathbf{1}\{n_{eau}(c;S_{eau})+p_{eau}(c)>\tau_{eau}\}`,
    expectation: L`\mathbb{E}[\bar X_{eau}]\approx 0.04,\qquad \bar X_{eau}=\frac{1}{|\mathcal{G}|}\sum_{c\in\mathcal{G}} X_{eau}(c)`,
    variance: L`\mathrm{Var}(\bar X_{eau})\text{ est estimee par simulation car la structure spatiale n'est pas i.i.d.}`,
    note:
      "Comme pour la terre, la quantite suivie est la couverture d'eau du plateau et non un pseudo-tirage independant cellule par cellule."
  }),
  "Masque de flip des textures de terrain": createTheory({
    support: L`F(c)\in\{0,1,2,3\}`,
    law: L`\mathbb{P}(F(c)=f)\approx \frac{1}{4}`,
    expectation: L`\mathbb{E}[F(c)]\approx \frac{3}{2}`,
    variance: L`\mathrm{Var}(F(c))\approx \frac{5}{4}`,
    note:
      "La loi est seulement pseudo-uniforme car elle provient d'un hachage deterministe de la position. Les moments sont ceux du codage entier des quatre flips."
  }),
  "Bruit de contour du brouillard": createTheory({
    support: L`B(c)\in[0.5,1.5]`,
    law: L`B(c)=1+(U(c)-0.5)\,a,\qquad a=1`,
    expectation: L`\mathbb{E}[B(c)]\approx 1`,
    variance: L`\mathrm{Var}(B(c))=a^2\,\mathrm{Var}(U(c))\quad \text{(estimee empiriquement)}`,
    note:
      "`U(c)` vient d'un value noise spatialement correle, pas d'une uniforme i.i.d.; la variance vraiment utile est donc mesuree sur la rugosite de contour observee."
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
      "Un jeu d'echecs de conquete ou le territoire, l'economie, la meteo et le brouillard recomposent chaque partie.",
    source: ""
  },
  gameIntroduction: {
    blocks: [
      {
        title: "1. Deux royaumes, un roi a proteger",
        vignetteId: "kingdoms",
        paragraphs: [
          "Chaque joueur controle un royaume blanc ou noir avec un roi, des pieces et ses propres batiments. Les règles de déplacement sont les mêmes que dans un jeu d'échecs classique (quelques ajustements ont été nécessaires pour le pion afin qu'il puisse bouger dans toutes les directions). La condition centrale reste simple: proteger son roi et mettre le royaume adverse en echec.",
          "La difference avec un echiquier classique, c'est que les pieces jouent sur une vraie carte. Les distances, les obstacles, les ressources et les evenements du terrain comptent autant que les mouvements eux-memes."
        ]
      },
      {
        title: "2. Chaque tour donne des points de mouvement et des points de construction",
        vignetteId: "turnBudget",
        paragraphs: [
          "Un tour ne correspond pas a un seul coup. Chaque royaume recoit un stock de **points de mouvement** pour deplacer ses pieces et un stock de **points de construction** pour poser, reparer ou lancer des batiments.",
          "**Par exemple, bouger une tour coute 4 points de mouvement, alors que bouger un pion ne coute qu'1 point de mouvement.** Ces deux budgets imposent un arbitrage permanent. Si vous depensez vos points pour avancer vos pieces, vous construisez moins. Si vous investissez dans les batiments, vous ralentissez votre pression immediate sur la carte."
        ]
      },
      {
        title: "3. Le terrain et l'eau changent les chemins possibles",
        vignetteId: "terrain",
        paragraphs: [
          "La carte est regeneree a chaque partie. On y trouve de l'herbe, de la terre et surtout des zones d'eau qui bloquent certaines trajectoires.",
          "L'eau force des detours, ferme des acces et protege parfois un cote du plateau. Avant meme de parler d'economie ou d'evenements aleatoires, il faut donc lire quels couloirs restent vraiment praticables."
        ]
      },
      {
        title: "4. Les batiments publics donnent des ressources et des objectifs de carte",
        vignetteId: "economy",
        paragraphs: [
          "La carte contient aussi des **batiments publics**, c'est-a-dire des **structures neutres a capturer**, par exemple des mines, des fermes ou des eglises. Ils ne servent pas de decor: ils creent des points a controler pour gagner plus de valeur sur la duree.",
          "**Par exemple, si une piece blanche occupe une mine, elle rapporte 10 d'or par tour.** Conquerir ces zones change donc directement l'economie. On ne joue pas seulement contre le roi adverse; on joue aussi pour tenir les secteurs qui donnent de l'or, de la production ou de la progression."
        ]
      },
      {
        title: "5. Construire et produire des pieces fait partie du tour",
        vignetteId: "production",
        paragraphs: [
          "Les **points de construction** servent notamment a poser ou reparer des structures. Les casernes permettent ensuite de produire de nouvelles pieces au lieu de se contenter de l'armee de depart.",
          "La partie devient donc un jeu de developpement en plus d'un jeu tactique. Vous pouvez consolider votre base, ouvrir un nouvel axe d'attaque ou preparer une piece supplementaire pour les tours suivants."
        ]
      },
      {
        title: "6. Arene et eglise permettent d'ameliorer une piece de maniere precise",
        vignetteId: "progression",
        paragraphs: [
          "**Une arene fait progresser une piece en experience au fil des tours**, et **une eglise permet certaines transformations speciales**. Ce systeme n'est donc pas abstrait: il passe par des batiments precis et par des combinaisons precises.",
          "**Par exemple, si on reunit dans une eglise un roi, un fou et une tour, alors la tour se transforme en reine.** La demonstration a droite montre exactement ce cas, puis recommence en boucle."
        ]
      },
      {
        title: "7. Les coffres donnent de l'or ou augmentent les budgets du tour",
        vignetteId: "chest",
        paragraphs: [
          "Des coffres apparaissent pendant la partie sur des cases visibles et contestables. Une piece qui atteint un coffre l'ouvre immediatement et obtient une recompense aleatoire.",
          "Cette recompense peut etre de l'**or**, un bonus permanent de **mouvement** maximal par tour, ou un bonus permanent de **construction** maximale par tour. Comme ces gains modifient directement vos budgets, les coffres creent de vraies courses sur la carte."
        ]
      },
      {
        title: "8. Le brouillard peut cacher des pieces et des batiments ennemis",
        vignetteId: "weather",
        paragraphs: [
          "Le brouillard ne sert pas seulement d'habillage visuel. Il peut cacher des pieces ou des batiments ennemis s'ils sont dans la zone couverte.",
          "**Par exemple, un joueur peut se deplacer sous un brouillard pour mener une embuscade sur la base d'un autre joueur.** Le point important est que le cache est lie au point de vue. Les Blancs et les Noirs ne voient donc pas toujours les memes informations au meme moment."
        ]
      },
      {
        title: "9. Les pieces du diable ajoutent une menace autonome en plus des deux royaumes",
        vignetteId: "infernal",
        paragraphs: [
          "Le jeu suit une **dette de sang** pour chaque royaume, c'est-a-dire un **compteur de menace** qui monte quand les captures et les degats s'accumulent, puis redescend progressivement avec le temps.",
          "**Dans l'exemple, une piece du diable de type tour apparait sur le bord droit, capture d'abord le fou blanc le plus proche, puis le pion blanc.** Plus la dette totale monte, plus une piece du diable a de chances d'apparaitre au bord de la carte. Cette piece autonome cible un royaume, se deplace seule et ajoute une pression supplementaire qu'aucun des deux joueurs ne controle directement."
        ]
      }
    ]
  },
  randomnessLink: {
    title: "Pourquoi les processus aleatoires sont essentiels au jeu",
    paragraphs: [
      "Ce rapport montre pourquoi les processus aleatoires sont essentiels au jeu: ils renouvellent les parties, enrichissent les choix strategiques et apportent une vraie plus-value par rapport aux echecs classiques, tout en restant assez lisibles pour etre analyses, modelises et observes."
    ],
    sections: [],
    reportDimensionsTitle: "Les trois dimensions du rapport",
    reportDimensions: [
      {
        title: "1. Inventaire des processus aleatoires",
        text:
          "La premiere couche du site recense les variables aleatoires actives de la codebase, leur famille de loi, leurs parametres, leur support, leur methode de simulation et leur ancrage deterministe dans `worldSeed` et les compteurs RNG serialises.",
        showSummaryStats: true
      },
      {
        title: "2. 500 parties simulees",
        sourceKind: "simulated",
        text:
          "La seconde couche repose sur 500 simulations theoriques. Il ne s'agit pas de matchs entre IA ni de parties humaines accelerees: on simule directement les mecanismes concernes, par exemple la generation du terrain, les coffres, la meteo ou les spawns infernaux, afin d'estimer leur comportement attendu."
      },
      {
        title: "3. Une partie reelle instrumentee",
        sourceKind: "real",
        text:
          "La troisieme couche exploite les donnees d'une partie complete jouee pendant plusieurs heures avec un ami. Les actions, les tours et les etats utiles ont ete enregistres pour confronter la theorie a un runtime reel et montrer des resultats lisibles en situation de jeu."
      }
    ],
    replayTitle: "Replay de la partie reelle",
    replayText:
      "Le viewer ci-dessous donne un apercu direct de la partie observee, en lecture automatique rapide, avec boucle et point de vue blanc. Il sert d'entree visuelle avant le detail statistique du rapport."
  },
  summaryStats: [
    {
      value: "38",
      label: "processus actifs",
      detail: "inventories dans l'audit runtime et reclasses ici par lois"
    },
    {
      value: "12",
      label: "familles modelisantes",
      detail: "lois usuelles + champs proceduraux correles"
    },
    {
      value: "5",
      label: "sous-systemes jouables",
      detail: "carte, XP, coffres, meteo, pieces du diable"
    },
    {
      value: "worldSeed",
      label: "racine deterministe",
      detail: "completee par des compteurs RNG serialises par systeme"
    }
  ],
  methodology: {
    paragraphs: [
      "Le jeu n'utilise pas l'aleatoire comme une boite noire. Chaque systeme stochastic passe par un schema recurrent: une seed de monde `worldSeed`, un compteur d'evenements `rngCounter`, puis une transformation specifique au systeme. L'analyse probabiliste correcte doit donc raisonner a deux niveaux: la loi brute tiree par la bibliotheque standard, puis la loi effectivement observee apres conditionnement, troncature, arrondi ou filtrage de gameplay.",
      "La classification choisie ici suit les lois plutot que les fichiers. Cela permet de voir immediatement que l'XP et l'or des coffres sont deux usages du meme schema de normale tronquee; que la meteo combine uniforme, Gamma, log-normale et piecewise linear; et que certains processus de carte ou de contour ne sont pas bien decrits par une variable scalaire classique mais par des champs spatiaux correles.",
      "Quand une variable est nominale, par exemple un type de recompense ou une direction, on insiste sur le fait qu'il n'existe pas d'esperance canonique sans choisir au prealable un score numerique auxiliaire. Ce point est essentiel pour ne pas ecrire de formules fausses juste parce qu'une API de tirage renvoie un entier d'indice."
    ],
    formulas: [
      {
        label: "Schema deterministe transverse",
        latex: L`U_t = G\bigl(worldSeed, rngCounter_t\bigr), \qquad X_t = \Phi\bigl(U_t, S_t\bigr)`
      },
      {
        label: "Evolution du compteur",
        latex: L`rngCounter_{t+1} = rngCounter_t + 1 \quad \text{a chaque evenement consommatif}`
      },
      {
        label: "Principe de lecture du rapport",
        latex: L`\text{loi observee} = \text{transformation gameplay} \circ \text{loi standard}`
      }
    ],
    highlights: [
      "Les systemes XP, Coffres, Meteo et Pieces du diable possedent chacun leur compteur RNG serialize; le determinisme persiste donc apres sauvegarde/rechargement.",
      "Les seeds auxiliaires de meteo et de generation de carte sont elles-memes des variables aleatoires uniformes a grand support, mais elles servent ensuite a piloter des champs non i.i.d.",
      "Le rapport distingue toujours la loi theorique continue de la loi runtime reellement observee quand un arrondi, un `ceil` ou un `clamp` est applique."
    ]
  },
  outputStats: [
    {
      title: "Variables etats serialisees",
      text:
        "La sauvegarde ne memorise pas seulement le resultat final; elle memorise aussi l'etat probabiliste necessaire pour rejouer la suite de la partie sans derive de seed.",
      bullets: [
        "`worldSeed` fixe le monde de reference.",
        "`rngCounter` de XP, Coffres, Meteo et Pieces du diable est serialize par systeme.",
        "Les etats derives comme `rewardRngCounter`, `currentRewardGeneration` et les descripteurs de brouillard conservent la continuite des lois conditionnelles."
      ]
    },
    {
      title: "Series statistiques directement extractibles",
      text:
        "Le code et les exports JSON permettent deja de reconstruire plusieurs series quantitatives utiles pour la validation empirique du modele.",
      bullets: [
        "Histogrammes d'XP par source et comparaison a la normale tronquee annoncee.",
        "Retards de reapparition des coffres et repartition des recompenses par regime early/late.",
        "Inter-arrivees meteo, durees visibles, couverture, aspect ratio et opacites locales.",
        "Dette de sang, intensite d'apparition induite et types de cibles effectivement selectionnes."
      ]
    },
    {
      title: "Sorties deja visibles dans le projet",
      text:
        "Le depot contient deja plusieurs supports de verification pratique pour confronter la theorie au runtime.",
      bullets: [
        "`debug_game_state/` contient des historiques de tours utiles pour relire les evenements stochastiques.",
        "`saves/` et `PARTICULAR SAVES/` montrent la persistance des seeds et compteurs dans des etats concrets.",
        "`statistiques-generator/` offre une base naturelle pour automatiser demain des comparaisons entre distributions attendues et distributions observees."
      ]
    }
  ],
  lawSections: [
    {
      id: "uniformes",
      title: "Uniformes discretes, continues et conditionnelles",
      badge: "15 processus",
      description: [
        "Cette famille couvre les tirages symetriques sur un support fini, les pourcentages uniformes sur un intervalle continu et les choix uniformes conditionnes a un sous-ensemble admissible. Elle regroupe aussi plusieurs seeds intermediaires, mathematiquement ordinaires mais gameplay-ment importantes parce qu'elles pilotent ensuite des champs proceduraux.",
        "Dans le code, ces lois apparaissent surtout via `std::uniform_int_distribution`, des appels directs a `generator()` sur 32 bits, ou des tirages uniformes apres filtrage/tri de candidats."
      ],
      formulaCards: [
        {
          label: "Uniforme discrete",
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
        "Pour les seeds 32 bits, la formule pertinente est celle de l'uniforme discrete sur un grand espace fini; leur esperance existe mais n'a pas d'interet gameplay direct.",
        "Quand le support depend de filtres geometriques, la loi observable n'est pas la loi brute mais la loi conditionnelle sur l'ensemble admissible courant."
      ],
      processes: illustratedUniformProcesses
    },
    {
      id: "permutation-uniforme",
      title: "Permutation uniforme",
      badge: "1 processus",
      description: [
        "Le placement public ne commence pas par choisir des positions, mais par melanger l'ordre des objets a poser. La variable naturelle n'est donc pas un entier simple, mais une permutation uniforme sur un ensemble fini.",
        "Pour retrouver des moments scalaires, on peut regarder une variable derivee comme le rang d'un objet fixe dans la permutation."
      ],
      formulaCards: [
        {
          label: "Loi sur le groupe symetrique",
          latex: L`\Pi \sim \mathrm{Unif}(\mathfrak{S}_n), \qquad \mathbb{P}(\Pi=\pi)=\frac{1}{n!}`
        },
        {
          label: "Rang d'un objet fixe",
          latex: L`R_i \sim \mathcal{U}_d(\{1,\dots,n\}), \qquad \mathbb{E}[R_i]=\frac{n+1}{2}, \quad \mathrm{Var}(R_i)=\frac{n^2-1}{12}`
        }
      ],
      notes: [
        "L'esperance d'une permutation en tant qu'objet du groupe symetrique n'est pas canonique; c'est pourquoi on passe par une statistique derivee comme le rang.",
        "Cette permutation agit ensuite en amont des tirages uniformes conditionnels de position."
      ],
      processes: illustratedPermutationProcesses
    },
    {
      id: "categorielles",
      title: "Categorielle ponderee",
      badge: "7 processus",
      description: [
        "Des qu'il faut choisir entre plusieurs categories nominales avec des poids relatifs, la bonne famille est la categorielle ponderee. C'est le cheval de bataille des coffres, de la meteo et surtout de la logique des pieces du diable.",
        "Mathematiquement, l'esperance n'est pas definie tant qu'on n'a pas choisi une fonction de score `g` sur les categories; on donne donc les moments de `g(X)` plutot que ceux de `X` lui-meme."
      ],
      formulaCards: [
        {
          label: "PMF ponderee",
          latex: L`\mathbb{P}(X=x_i)=\frac{w_i}{\sum_{j=1}^n w_j}, \qquad w_i \ge 0`
        },
        {
          label: "Moments via un score",
          latex: L`\mathbb{E}[g(X)] = \sum_{i=1}^n g(x_i)\,\frac{w_i}{\sum_j w_j}, \qquad \mathrm{Var}(g(X)) = \mathbb{E}[g(X)^2] - \mathbb{E}[g(X)]^2`
        }
      ],
      notes: [
        "Des poids egaux redonnent une uniforme discrete, mais l'implementation reste la meme en code via `std::discrete_distribution`.",
        "Quand les poids dependent du plateau, de la dette ou de la visibilite, la loi devient conditionnelle a l'etat courant."
      ],
      processes: illustratedCategoricalProcesses
    },
    {
      id: "bernoulli",
      title: "Bernoulli",
      badge: "2 processus",
      description: [
        "La Bernoulli intervient pour les decisions binaires: choisir un royaume plutot que l'autre, ou activer une branche de comportement aleatoire.",
        "Dans ce code, elle apparait soit explicitement via `std::bernoulli_distribution`, soit implicitement via un tirage uniforme compare a un seuil entier sur 1000."
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
        "La probabilite `p` peut etre statique, comme 0.333, ou dependre dynamiquement de l'etat du jeu comme la dette de sang.",
        "Une Bernoulli sur un support binaire reste la loi la plus lisible pour decrire ces branchements meme quand l'implementation passe par un entier uniforme."
      ],
      processes: illustratedBernoulliProcesses
    },
    {
      id: "poisson",
      title: "Poisson et gate d'arrivee",
      badge: "1 processus",
      description: [
        "Les pieces du diable ne reposent pas sur une simple probabilite fixe d'apparition, mais sur un comptage d'arrivees potentielles modele par une Poisson. Le gameplay observe seulement l'evenement `N >= 1`, mais la variable latente est bien un nombre entier de tentatives.",
        "Ce choix donne une interpretation propre de l'intensite comme dette de sang convertie en frequence moyenne d'arrivees."
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
          label: "Probabilite de spawn observee",
          latex: L`\mathbb{P}(N \ge 1)=1-e^{-\lambda}`
        }
      ],
      notes: [
        "Dans le gameplay courant, seule la classe d'evenement `0` contre `>= 1` est exploitée, mais raisonner sur `N` reste plus juste que d'ecrire directement une Bernoulli arbitraire.",
        "La dette de sang agit ici comme un parametre d'intensite, pas comme un poids categoriel."
      ],
      processes: illustratedPoissonProcesses
    },
    {
      id: "normales-tronquees",
      title: "Normales tronquees et discretisees",
      badge: "2 processus",
      description: [
        "L'XP et l'or des coffres reutilisent le meme patron: une normale centree sur une moyenne design, tronquee a un multiple de son ecart-type, arrondie a l'entier puis soumise a un plancher minimal.",
        "La loi effectivement jouee n'est donc pas une gaussienne pure: c'est une gaussienne transformee par clamp et quantification."
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
          label: "Moments de la version tronquee",
          latex: L`\mathbb{E}[X\mid a\le X\le b]=\mu+\sigma\frac{\varphi(\alpha)-\varphi(\beta)}{\Phi(\beta)-\Phi(\alpha)},\quad \alpha=\frac{a-\mu}{\sigma},\; \beta=\frac{b-\mu}{\sigma}`
        },
        {
          label: "Transformation runtime",
          latex: L`Y = \max\bigl(m, \mathrm{round}(\mathrm{clip}(X,[a,b]))\bigr)`
        }
      ],
      notes: [
        "L'arrondi et le minimum modifient legerement l'esperance par rapport a la formule continue; la formule ci-dessus est donc la bonne reference theorique, pas la valeur exacte apres discretisation.",
        "Dans ce code, `sigma = max(1, mean * sigmaMultiplier)` et `a,b = mean +/- clampMultiplier * sigma`."
      ],
      processes: illustratedTruncatedNormalProcesses
    },
    {
      id: "weibull",
      title: "Weibull discretisee",
      badge: "1 processus",
      description: [
        "La Weibull apparait pour les delais de reapparition des coffres. C'est un choix pertinent des qu'on veut un temps d'attente positif dont la probabilite de survenue change avec l'anciennete du delai ecoule.",
        "Le runtime n'utilise pas la variable continue telle quelle: il l'arrondit et la borne inferieurement par un cooldown minimal."
      ],
      formulaCards: [
        {
          label: "Densite Weibull",
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
        "Avec `k > 1`, le hazard augmente avec le temps, ce qui correspond bien a l'intuition 'plus le coffre tarde, plus sa reapparition devient plausible'."
      ],
      processes: illustratedWeibullProcesses
    },
    {
      id: "gamma",
      title: "Gamma discretisee",
      badge: "2 processus",
      description: [
        "La Gamma pilote les inter-arrivees et certaines durees meteo. Son support positif et sa grande souplesse de forme en font un bon compromis entre exponentialite pure et modele trop rigide.",
        "Le code applique `ceil`, puis convertit en tours ou pas de simulation. La loi observee est donc une version quantifiee de la Gamma continue."
      ],
      formulaCards: [
        {
          label: "Densite Gamma",
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
        "La meteo utilise deux Gammas distinctes: l'une pour l'attente avant apparition, l'autre pour la duree visible cible."
      ],
      processes: illustratedGammaProcesses
    },
    {
      id: "lognormale",
      title: "Log-normale",
      badge: "1 processus",
      description: [
        "La log-normale apparait dans la texture d'opacite des brouillards. Le choix est mathematiquement naturel des qu'on veut des multiplicateurs strictement positifs, susceptibles d'etre parfois un peu plus grands que 1 sans jamais devenir negatifs.",
        "Le runtime rederive la graine par cellule a partir de `densitySeed`, puis re-borne le resultat via des `alphaMin` et `alphaMax`."
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
        "La variable finale vue a l'ecran est encore transformee par `alpha = clamp(alphaBase * X, alphaMin, alphaMax)`, donc les moments exacts apres clamp doivent etre estimes numeriquement."
      ],
      processes: illustratedLogNormalProcesses
    },
    {
      id: "beta",
      title: "Beta transformee",
      badge: "1 processus",
      description: [
        "La Beta est utilisee pour la luminosite de l'herbe. C'est une bonne famille pour modeler une variable naturellement bornee dans `[0,1]` avant remappage visuel.",
        "Le code ne l'obtient pas par API directe, mais via le quotient de deux Gammas, ce qui est mathematiquement standard."
      ],
      formulaCards: [
        {
          label: "Densite Beta",
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
        "Avec `(\alpha, \beta) = (7, 2)`, la moyenne brute vaut `7/9`, donc les tirages se concentrent naturellement vers des valeurs elevees avant seuil et remappage."
      ],
      processes: illustratedBetaProcesses
    },
    {
      id: "piecewise-linear",
      title: "Piecewise linear",
      badge: "1 processus",
      description: [
        "La position d'entree d'un brouillard le long du bord n'est ni uniforme, ni gaussienne. Elle suit une densite dessinee a la main par morceaux lineaires afin de surponderer les entrees centrales tout en gardant des coins possibles.",
        "C'est un bon exemple de loi standard de la bibliotheque C++ qui n'est pas toujours mobilisee dans les rapports probabilistes classiques, mais qui reste parfaitement legitime ici."
      ],
      formulaCards: [
        {
          label: "Densite lineaire par morceaux",
          latex: L`f(x)=\frac{\ell(x)}{\int_0^M \ell(u)\,du}, \qquad \ell \text{ lineaire sur chaque intervalle de noeuds}`
        },
        {
          label: "Moments generaux",
          latex: L`\mathbb{E}[X]=\int_0^M x f(x)\,dx, \qquad \mathrm{Var}(X)=\int_0^M x^2 f(x)\,dx - \mathbb{E}[X]^2`
        }
      ],
      notes: [
        "Les moments exacts se calculent numeriquement a partir des noeuds et poids, ce qui est approprie puisque la densite est entierement specifiee par ces donnees."
      ],
      processes: illustratedPiecewiseLinearProcesses
    },
    {
      id: "procedural-fields",
      title: "Variables personnalisees et champs proceduraux correles",
      badge: "4 processus",
      description: [
        "Tous les processus aleatoires du jeu ne sont pas raisonnablement resumables par une unique variable scalaire. Les champs de terrain et les deformations de contour du brouillard sont des fonctions aleatoires de la cellule et d'une seed, avec forte correlation spatiale.",
        "Les traiter comme des Bernoulli i.i.d. (**independantes et identiquement distribuees**), serait mathematiquement faux et gameplay-ment trompeur: on perdrait exactement la structure de regions, de bords et de textures que le code cherche a produire."
      ],
      formulaCards: [
        {
          label: "Champ aleatoire spatial",
          latex: L`X(c)=g_s(c), \qquad c \in \mathcal{G}`
        },
        {
          label: "Dependance spatiale",
          latex: L`\mathrm{Cov}\bigl(X(c),X(c')\bigr) \neq 0 \quad \text{en general pour des cellules proches}`
        },
        {
          label: "Lecture correcte",
          latex: L`\text{processus observe} = \text{seed uniforme} + \text{fonction de bruit / post-traitement}`
        }
      ],
      notes: [
        "Ici, la bonne unite mathematique n'est plus 'une realisation d'une loi scalaire', mais 'une realisation d'un champ spatial'.",
        "Les statistiques pertinentes sont alors la couverture, la taille des composantes, la correlation spatiale, la rugosite de bord ou la distribution des rayons effectifs."
      ],
      processes: illustratedProceduralProcesses
    }
  ],
  dependenceNotes: [
    "Le coeur du determinisme est `worldSeed + rngCounter`; cela cree une dependance structurelle commune a tous les tirages d'un meme systeme, tout en rendant la suite parfaitement replayable apres sauvegarde.",
    "Les lois conditionnelles dominent le gameplay reel: une uniforme ou une categorielle n'est presque jamais tiree sur un support absolu, mais sur un support deja filtre par la geometrie, la visibilite, l'occupation ou l'historique des choix precedents.",
    "Le mode de rattrapage des coffres (`current_loot_catch_up_enabled`) signifie que **les deux royaumes partagent temporairement une meme recompense courante**; **le tirage suivant n'apparait que lorsque les deux l'ont deja collectee**. Les recompenses de coffre ne sont donc **pas independantes** entre royaumes quand ce mode est actif.",
    "Les brouillards portent deux graines internes, l'une pour la forme et l'autre pour la densite, qui induisent de fortes correlations spatiales intra-brouillard, puis une dependance temporelle via la duree gamma et le prochain delai d'apparition.",
    "Les pieces du diable ne reposent pas sur un systeme a parametres fixes: leur Bernoulli de royaume cible et leur Poisson d'apparition dependent directement d'un etat dynamique, la dette de sang."
  ],
  difficulties: [
    {
      title: "Transformer une loi theorique en variable runtime jouable",
      text:
        "Dans ce projet, presque aucune loi standard n'arrive brute a l'ecran. Les recompenses d'XP et d'or sont tronquees, arrondies et bornees par un minimum; les delais Weibull et Gamma passent par `round` ou `ceil`; l'opacite du brouillard est encore `clamp`ee apres la log-normale. La difficulte reelle a donc ete de documenter a la fois la loi parente et la variable effectivement utilisee par le runtime, sans faire croire qu'un histogramme discret final est exactement une Gaussienne, une Gamma ou une Weibull continues."
    },
    {
      title: "Donner une lecture statistique correcte a des variables non numeriques",
      text:
        "Une direction de brouillard, un type de recompense de coffre ou un type de piece du diable ne portent pas naturellement une moyenne ou une variance. La difficulte n'etait pas de calculer un nombre coute que coute, mais d'eviter une erreur de modelisation. Pour ces variables, j'ai donc choisi d'expliciter le support, la loi categorielle et les poids, puis de reserver les moments aux cas ou une variable numerique ou un score auxiliaire avaient un sens."
    },
    {
      title: "Mesurer des champs spatiaux correles sans les reduire a du i.i.d.",
      text:
        "La terre, l'eau et les contours de brouillard sont produits par du bruit procedural partageant une meme seed et un meme post-traitement spatial. Une statistique cellule par cellule aurait masque le vrai phenomene, qui est l'apparition de regions coherentes, de lacs, de couloirs et de silhouettes. La difficulte a donc ete de choisir comme variables observees des resumes adaptes a un champ: couverture totale, cellules refusees, rugosite de bord, duree visible ou nombre de pieces masquees, plutot qu'une fausse Bernoulli independante par cellule."
    },
    {
      title: "Instrumenter le runtime reel sans perdre le determinisme des parties",
      text:
        "Le rapport ne repose pas seulement sur des simulations hors ligne: il fallait aussi extraire une partie reelle instrumentee, rejouable, et comparer ses trajectoires a des lois theoriques. Cela oblige a serialiser correctement `worldSeed`, les compteurs RNG, les etats de brouillard, la dette de sang et les evenements de spawn pour qu'une sauvegarde et un replay racontent exactement la meme histoire statistique. Sans cette instrumentation, les jolies formules du rapport seraient restees decouplees du jeu execute."
    },
    {
      title: "Relier batch simule et partie reelle sans surinterpreter",
      text:
        "Les 500 parties simulees donnent des tendances robustes sur les familles de lois, mais une partie reelle unique reste fortement dependante de son histoire tactique. La difficulte finale a donc ete d'assumer cette asymetrie: utiliser le batch pour verifier les supports, les ordres de grandeur et les histogrammes globaux, puis utiliser la partie instrumentee pour montrer comment les dependances d'etat, la visibilite et la dette de sang deformaient concrètement ces lois pendant une vraie partie."
    }
  ],
  perspectives: [
    "La premiere perspective est de **mieux ajuster les parametres des mecanismes aleatoires** a partir d'un volume de parties plus important. Je n'ai pas encore assez de recul statistique pour equilibrer proprement ces variables: par exemple, les apparitions des pieces du diable produisent encore trop souvent des pions, alors que cette piece est lente et peu impactante, et la loi normale des recompenses d'or des coffres reste trop resserree autour de sa moyenne, ce qui rend les variations peu perceptibles pour le joueur.",
    "Une deuxieme perspective est donc d'**accumuler beaucoup plus de donnees de partie** afin d'ameliorer l'equilibrage general du jeu. L'objectif n'est pas seulement de decrire les lois utilisees, mais de disposer d'assez d'observations pour corriger les desequilibres reels, ajuster les amplitudes utiles et verifier que les evenements aleatoires enrichissent effectivement la partie au lieu d'aplatir ses situations.",
    "Enfin, un chantier important sera de developper une **intelligence artificielle symbolique** capable d'agir a partir de regles deterministes tout en **anticipant des evenements aleatoires probables**. Explorer cette articulation entre raisonnement symbolique et incertitude serait utile a la fois pour mieux jouer, pour mieux tester le jeu et pour mieux exploiter toutes les statistiques produites par ce travail."
  ]
};