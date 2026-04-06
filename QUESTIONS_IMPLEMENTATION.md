# A Normal Chess Game — Questions d'implémentation

> Ce document liste **toutes les questions** à résoudre avant et pendant le développement du jeu, organisées par thématique. Chaque question est numérotée de manière unique pour faciliter le suivi des décisions.

---

## Table des matières

1. [Technique — Moteur, plateforme et architecture](#1-technique--moteur-plateforme-et-architecture)
2. [La carte — Dimensions, génération et terrain](#2-la-carte--dimensions-génération-et-terrain)
3. [Rendu visuel — Direction artistique et esthétique](#3-rendu-visuel--direction-artistique-et-esthétique)
4. [Les pièces d'échecs — Règles de déplacement et portée](#4-les-pièces-déchecs--règles-de-déplacement-et-portée)
5. [Système d'expérience et d'upgrade](#5-système-dexpérience-et-dupgrade)
6. [Les ressources et l'économie](#6-les-ressources-et-léconomie)
7. [Les bâtiments — Dimensions, coûts et mécaniques](#7-les-bâtiments--dimensions-coûts-et-mécaniques)
8. [Le système de tour par tour — Mécaniques précises](#8-le-système-de-tour-par-tour--mécaniques-précises)
9. [Le système de formations](#9-le-système-de-formations)
10. [Configuration initiale et début de partie](#10-configuration-initiale-et-début-de-partie)
11. [Condition de victoire et fin de partie](#11-condition-de-victoire-et-fin-de-partie)
12. [Événements aléatoires et météo](#12-événements-aléatoires-et-météo)
13. [Mobs neutres](#13-mobs-neutres)
14. [Système de déité — Dieu, Diable et sacrifice](#14-système-de-déité--dieu-diable-et-sacrifice)
15. [Intelligence artificielle (IA ennemie)](#15-intelligence-artificielle-ia-ennemie)
16. [Interface utilisateur (UI) et menus](#16-interface-utilisateur-ui-et-menus)
17. [Audio — Musique et effets sonores](#17-audio--musique-et-effets-sonores)
18. [Caméra et navigation sur la carte](#18-caméra-et-navigation-sur-la-carte)
19. [Sauvegarde et persistance](#19-sauvegarde-et-persistance)
20. [Équilibrage et game design chiffré](#20-équilibrage-et-game-design-chiffré)
21. [Lore et univers narratif](#21-lore-et-univers-narratif)

---

## 1. Technique — Moteur, plateforme et architecture

### Moteur de jeu et langage

| #    | Question |
|------|----------|
| 1.1  | Quel moteur de jeu est utilisé ? (Unity, Godot, Unreal, framework custom, web-based…) |
| 1.2  | Quel langage de programmation principal ? (C#, GDScript, C++, TypeScript/JavaScript, Rust…) |
| 1.3  | Le jeu est-il en **2D** ou en **3D** ? (Vue du dessus isométrique, top-down orthographique, 3D avec caméra libre…) |
| 1.4  | Quelles sont les **plateformes cibles** ? (PC Windows uniquement ? Mac/Linux ? Navigateur web ? Mobile ?) |

### Architecture logicielle

| #    | Question |
|------|----------|
| 1.5  | L'architecture du code doit-elle suivre un pattern spécifique ? (ECS, MVC, state machine…) |
| 1.6  | Comment est structuré le projet ? Y a-t-il des dossiers imposés, une convention de nommage ? |
| 1.7  | Le jeu doit-il supporter le **multijoueur** à terme (en ligne, local, hot-seat) ou est-ce uniquement **joueur solo vs IA** ? |
| 1.8  | Y a-t-il un budget de **performance** à respecter ? (FPS minimum, taille mémoire max, temps de chargement max) |
| 1.9  | Faut-il prévoir un système de **modding** ou d'extension par les joueurs ? |

---

## 2. La carte — Dimensions, génération et terrain

### Dimensions

| #    | Question |
|------|----------|
| 2.1  | Quelle est la taille **définitive** de la carte ? (16×16 et 64×64 ont été évoqués — lequel est retenu ?) |
| 2.2  | La taille de la carte est-elle **fixe** ou **configurable** par le joueur avant la partie ? |
| 2.3  | La taille de la carte peut-elle être **rectangulaire** (ex : 64×48) ou est-elle toujours **carrée** ? |

### Génération procédurale

| #    | Question |
|------|----------|
| 2.4  | Quel algorithme de génération procédurale ? (Perlin noise, Cellular automata, Wave Function Collapse, placement aléatoire simple…) |
| 2.5  | Faut-il un **seed** visible/partageable pour reproduire une même carte ? |
| 2.6  | La carte doit-elle être **symétrique** par rapport aux deux camps (pour l'équité) ou peut-elle être totalement asymétrique ? |
| 2.7  | Comment est déterminé le **placement des bâtiments publics** (mines, champs, arènes) ? Nombre minimum/maximum de chaque type ? Distance minimale entre eux ? Distance aux points de spawn des royaumes ? |
| 2.8  | Combien de **mines** et de **champs** sont générés sur la carte ? Est-ce un nombre fixe ou proportionnel à la taille de la carte ? |
| 2.9  | Les arènes sont-elles aussi générées à des positions **aléatoires** ou ont-elles des positions prédéfinies (comme l'église au centre) ? |

### Types de terrain

| #    | Question |
|------|----------|
| 2.10 | L'**eau** est-elle un terrain **infranchissable** par toutes les pièces ou certaines pièces peuvent-elles la traverser ? |
| 2.11 | Y a-t-il des **effets de terrain** ? (Ex : terre = déplacement normal, herbe = bonus, eau = bloquant…) |
| 2.12 | La **terre** et l'**herbe** sont-elles mécaniquement différentes ou uniquement visuellement différentes ? |
| 2.13 | Peut-on **construire** des bâtiments sur n'importe quel type de terrain ? (Ex : pas de construction sur l'eau ?) |
| 2.14 | Y a-t-il d'autres types de terrain à prévoir ? (Montagne, forêt, marécage, sable…) |
| 2.15 | L'eau est-elle générée en **blocs contigus** (lacs, rivières) ou en cases isolées ? Quelle proportion maximale de la carte peut être de l'eau ? |

---

## 3. Rendu visuel — Direction artistique et esthétique

### Style graphique

| #    | Question |
|------|----------|
| 3.1  | Quel est le **style artistique** visé ? (Pixel art, low-poly 3D, dessin vectoriel, réaliste, cartoon, style médiéval illustré…) |
| 3.2  | Quelle est la **résolution cible** du jeu ? (1920×1080, 2560×1440, adaptatif…) |
| 3.3  | Y a-t-il une **palette de couleurs** imposée ou une ambiance de couleurs (sombre/médiéval, coloré/fantaisie, sobre…) ? |
| 3.4  | Le jeu doit-il avoir un aspect **parodique/humoristique** (le titre "A Normal Chess Game" le suggère) ou un aspect **sérieux** ? |

### Textures et sprites

| #    | Question |
|------|----------|
| 3.5  | Quelle est la **taille d'une case** en pixels à l'écran ? (Ex : 32×32, 64×64, 128×128…) |
| 3.6  | Quelles **textures de terrain** sont nécessaires ? (Herbe, terre, eau — variantes ? Transitions entre types de terrain ?) |
| 3.7  | Y a-t-il des **transitions visuelles** entre types de terrain (bords herbe/eau par ex.) ou des séparations nettes case par case ? |
| 3.8  | Les pièces d'échecs ont-elles un **sprite unique par type** ou des **variantes visuelles** (ex : différents cavaliers selon le niveau) ? |
| 3.9  | Faut-il des **animations** pour les pièces ? (Idle, déplacement, attaque, mort, upgrade…) |
| 3.10 | Comment les **bâtiments** sont-ils représentés visuellement quand ils occupent plusieurs cases ? (Un seul grand sprite ? Un sprite par case qui s'assemble ?) |
| 3.11 | Comment est visuellement représentée la **destruction progressive** d'un bâtiment ? (Cases grisées — quel niveau de détail ? Fissures, fumée, sprite dégradé ?) |
| 3.12 | Quel visuel pour les **murs de bois** vs les **murs de pierre** ? Est-ce qu'un mur de pierre endommagé (2/3 PV) a un visuel différent de plein (3/3 PV) ? |

### Différenciation des royaumes

| #    | Question |
|------|----------|
| 3.13 | Comment sont **différenciées visuellement** les pièces des deux royaumes ? (Couleurs blanc/noir classiques ? Bleu/rouge ? Bannières ? Contours ?) |
| 3.14 | Les bâtiments privés (casernes, murs) portent-ils les **couleurs du royaume** qui les a construits ? |
| 3.15 | Comment sont visuellement représentés les **mobs neutres** pour les distinguer des deux royaumes ? |

---

## 4. Les pièces d'échecs — Règles de déplacement et portée

### Déplacements

| #    | Question |
|------|----------|
| 4.1  | Les pièces obéissent-elles **strictement** aux règles de déplacement des échecs classiques ? (Pion : 1 case en avant + diagonale pour capturer ; Cavalier : L ; Fou : diagonale ; Tour : lignes droites ; Reine : diagonale + lignes droites ; Roi : 1 case dans toutes les directions) |
| 4.2  | Le **pion** peut-il avancer de **2 cases** au premier coup, comme aux échecs classiques ? Si oui, comment est défini "premier coup" vu qu'il est recruté en cours de partie ? |
| 4.3  | La prise **en passant** existe-t-elle dans ce jeu ? |
| 4.4  | Le **roque** existe-t-il dans ce jeu ? Si oui, comment fonctionne-t-il sur une carte de 64×64 ? |
| 4.5  | Dans quelle **direction** un pion avance-t-il ? (Toujours vers le camp adverse ? Mais sur une grande carte, c'est quoi "vers le camp adverse" ?) |
| 4.6  | Les pièces peuvent-elles **traverser** d'autres pièces alliées ? (Le cavalier saute dans les échecs classiques — est-ce conservé ?) |
| 4.7  | Les pièces peuvent-elles **traverser les bâtiments** (publics ou privés) ? Ou les bâtiments bloquent-ils le passage comme un obstacle ? |
| 4.8  | Un pion peut-il être **promu** (promotion classique des échecs quand il atteint le bout de l'échiquier) ? Si oui, c'est quoi "le bout" sur une carte 64×64 ? Ou est-ce remplacé par le système d'upgrade via XP ? |

### Portée

| #    | Question |
|------|----------|
| 4.9  | Quelle est la **portée initiale** exacte de chaque type de pièce au niveau de base ? |
| 4.10 | Quelle est la **portée maximale** de chaque type de pièce au niveau maximal ? |
| 4.11 | La **tour** a-t-elle aussi une portée limitée ? Quelles sont ses valeurs par niveau ? |
| 4.12 | La **reine** a-t-elle aussi une portée limitée ? Quelles sont ses valeurs ? |
| 4.13 | Le **roi** a-t-il une portée limitée (normalement 1 case dans toutes les directions aux échecs) ? |
| 4.14 | Le **cavalier** a-t-il une portée variable ou son mouvement en "L" est-il fixe ? Si la portée augmente, qu'est-ce que ça signifie pour un cavalier (L plus large ? Plusieurs sauts ?) |
| 4.15 | Le **pion** a-t-il une portée upgradable ? Si oui, que signifie une portée de 2+ pour un pion ? (Il avance de plusieurs cases ?) |
| 4.16 | La notion de "portée" s'applique-t-elle au **mouvement** et à l'**attaque** de la même manière, ou sont-ce deux valeurs distinctes ? |

---

## 5. Système d'expérience et d'upgrade

### Valeurs d'XP

| #    | Question |
|------|----------|
| 5.1  | Quel est le **seuil d'XP** exact pour chaque type de pièce pour atteindre 100% ? (Le brainstorming mentionne 500 pour le pion, 1000 pour la tour — quelles valeurs pour cavalier, fou ?) |
| 5.2  | Combien d'**XP rapporte** le fait de manger un pion ? Un cavalier ? Un fou ? Une tour ? Une reine ? |
| 5.3  | Combien d'**XP rapporte** la destruction d'un bloc de mur de bois ? De mur de pierre ? D'une case de caserne ? |
| 5.4  | Combien d'**XP par tour** rapporte l'arène par pièce posée dessus ? |
| 5.5  | L'XP est-elle **différente** selon le niveau de la pièce qui mange vs qui est mangée ? (Ex : un pion qui mange une tour gagne-t-il plus d'XP qu'un pion qui mange un pion ?) |
| 5.6  | Y a-t-il un bonus d'XP pour le système de **vétéran** mentionné dans le brainstorming ? (Chance à chaque kill de devenir vétéran, doublant ses capacités.) Ce système est-il retenu ou abandonné au profit du système d'upgrade standard ? |

### Mécaniques d'upgrade

| #    | Question |
|------|----------|
| 5.7  | L'**upgrade** est-elle **gratuite** ou coûte-t-elle des écus en plus de l'XP requise ? |
| 5.8  | L'upgrade s'applique-t-elle **immédiatement** (au moment où le joueur clique) ou au **tour suivant** ? |
| 5.9  | Quand une pièce est **upgradée**, conserve-t-elle sa **position** sur la carte ou doit-elle apparaître ailleurs ? |
| 5.10 | Quand une pièce est upgradée, sa **portée** passe-t-elle au niveau 1 de la nouvelle pièce ou garde-t-elle un bonus lié à sa progression antérieure ? |
| 5.11 | Y a-t-il un **nombre maximum** de tours, cavaliers, fous, etc. par royaume ? Ou le joueur peut-il en avoir autant qu'il veut (hors reine limitée à 1) ? |
| 5.12 | L'upgrade est-elle **obligatoire** quand l'XP atteint 100% ou le joueur peut-il **choisir de ne pas upgrader** ? |

---

## 6. Les ressources et l'économie

### Les écus

| #    | Question |
|------|----------|
| 6.1  | Quel est le **nombre d'écus initial** au début de la partie ? (Le brainstorming dit "zéro" — est-ce bien confirmé ?) |
| 6.2  | Y a-t-il un **plafond** (maximum stockable) d'écus ? |
| 6.3  | Les écus sont-ils **partagés** globalement pour le royaume ou liés à une caserne/zone particulière ? |

### Zones de farm

| #    | Question |
|------|----------|
| 6.4  | Combien d'**écus par tour** rapporte une mine ? Est-ce un montant fixe global ou un montant par case de la mine ? |
| 6.5  | Combien d'**écus par tour** rapporte un champ ? Les champs et les mines ont-ils des rendements **différents** ? Si oui, quelles sont les différences ? |
| 6.6  | Quelle est la **taille** (nombre de cases) typique d'une mine ? D'un champ ? Est-ce fixe ou variable ? |
| 6.7  | Faut-il qu'au moins **une pièce** soit sur la zone de farm pour collecter, ou faut-il qu'il y en ait sur **toutes** les cases ? |
| 6.8  | À quel moment du tour les écus sont-ils **crédités** ? (Au début du tour du joueur ? À la fin ? Instantanément au placement de la pièce ?) |
| 6.9  | Si un joueur occupe une zone de farm avec une seule pièce sur une des cases et que l'ennemi pose une pièce sur une **autre** case de la même zone, la zone cesse-t-elle de rapporter immédiatement ou à la fin du tour ? |

### Coûts

| #    | Question |
|------|----------|
| 6.10 | Quel est le **coût en écus** pour recruter chaque type de pièce à la caserne ? |
| 6.11 | Quel est le **coût en écus** pour construire une caserne ? |
| 6.12 | Quel est le **coût en écus** pour construire un mur de bois ? |
| 6.13 | Quel est le **coût en écus** pour construire un mur de pierre ? |
| 6.14 | Combien de **tours de fabrication** prend chaque type de pièce à la caserne ? (Le brainstorming dit : pion = 1 tour, pièces plus fortes = plusieurs tours — valeurs exactes ?) |
| 6.15 | Y a-t-il un coût de **maintenance** (écus par tour) pour entretenir ses pièces ou ses bâtiments, ou les coûts sont uniquement à la création ? |

---

## 7. Les bâtiments — Dimensions, coûts et mécaniques

### La caserne

| #    | Question |
|------|----------|
| 7.1  | Quelle est la **taille exacte** de la caserne en cases ? (2×3 a été évoqué — est-ce confirmé ?) |
| 7.2  | Combien de **points de vie** a chaque case d'une caserne ? (1 PV comme les pièces ? Plus ?) |
| 7.3  | Une caserne peut-elle **produire plusieurs unités en parallèle** (file de production) ou une seule à la fois ? |
| 7.4  | Sur quelles cases exactement une pièce **apparaît-elle** quand elle sort de la caserne ? (N'importe quelle case adjacente à la caserne ? Une case de sortie spécifique ?) |
| 7.5  | Y a-t-il un **maximum de casernes** constructibles par royaume ? |
| 7.6  | Si une caserne est en cours de production et qu'elle est **détruite**, que se passe-t-il ? (Production annulée ? Écus remboursés ?) |
| 7.7  | Peut-on **annuler** une production en cours dans une caserne pour récupérer (une partie des) écus ? |

### Les murs

| #    | Question |
|------|----------|
| 7.8  | Un mur **bloque-t-il** le passage de toutes les pièces (alliées et ennemies) ou seulement des pièces ennemies ? |
| 7.9  | Le **cavalier** peut-il **sauter par-dessus** un mur, conformément à sa capacité de saut aux échecs ? |
| 7.10 | Peut-on **réparer** un mur endommagé ? Si oui, comment et à quel coût ? |
| 7.11 | Y a-t-il une **limite** au nombre de murs constructibles ? |
| 7.12 | Les murs bloquent-ils la **ligne de vue** des fous, tours, et reines (les empêchant de traverser en diagonale ou en ligne droite) ? |

### L'église

| #    | Question |
|------|----------|
| 7.13 | L'église mesure 4×3 cases. Quelles cases exactement le roi et la tour doivent-ils occuper pour que le mariage soit possible ? (N'importe quelles cases de l'église ? Cases spécifiques ?) |
| 7.14 | Le mariage prend **un tour** pour être déclenché et la tour se transforme au **tour suivant**. Pendant ce tour d'attente, le roi et la tour doivent-ils **rester** dans l'église ou peuvent-ils bouger ? |
| 7.15 | Si la reine est **tuée**, peut-on en recréer une via un nouveau mariage ? (Le brainstorming dit "il ne peut y avoir qu'une reine" — est-ce par partie ou une seule à la fois ?) |
| 7.16 | L'église bloque-t-elle le **passage** des pièces ou peut-on traverser ses cases ? |

### L'arène

| #    | Question |
|------|----------|
| 7.17 | L'arène fait "environ 9 blocs de large". Quelles sont ses **dimensions exactes** ? (3×3 ? 9×1 ? Autre forme ?) |
| 7.18 | La pièce posée dans l'arène peut-elle **aussi agir normalement** (attaquer, se déplacer) ou est-elle **immobilisée** pendant l'entraînement ? |
| 7.19 | L'XP gagnée dans l'arène est-elle **identique** pour toutes les pièces ou proportionnelle à leur niveau ? |
| 7.20 | Les deux arènes sont-elles placées **symétriquement** sur la carte (une proche de chaque camp) ou aléatoirement ? |

### Mines et champs

| #    | Question |
|------|----------|
| 7.21 | Les mines et les champs sont-ils visuellement et mécaniquement **distincts** ? (Ou sont-ce deux noms pour le même concept de zone de farm ?) |
| 7.22 | Si distincts, quelle est la **différence mécanique** entre une mine et un champ ? (Rendement différent ? Taille différente ? Fréquence différente d'apparition ?) |
| 7.23 | Les zones de farm ont-elles une **forme** fixe (carré, rectangle) ou peuvent-elles avoir des formes irrégulières ? |

---

## 8. Le système de tour par tour — Mécaniques précises

### Actions par tour

| #    | Question |
|------|----------|
| 8.1  | Le joueur a-t-il un **nombre limité d'actions par tour** ou peut-il effectuer **toutes** les actions listées (fabrication + déplacement + mariage + construction) dans un seul tour ? |
| 8.2  | Le système de **points d'action (PA)** a été proposé mais pas confirmé. Est-il retenu ? Si oui, combien de PA par tour au départ ? |
| 8.3  | Si le système de PA est retenu, la **construction** coûte-t-elle des PA ? Le **lancement d'une fabrication** en caserne coûte-t-il des PA ? Le **mariage** coûte-t-il des PA ? |
| 8.4  | Le brainstorming dit "bouger **une** pièce/formation" par tour. Est-ce bien **un seul déplacement** par tour, ou le système de PA permet-il plusieurs déplacements ? |
| 8.5  | Peut-on cumuler **construire un bâtiment** ET **déplacer une pièce** dans le même tour, ou la construction consomme-t-elle le tour entier ? (Le brainstorming dit "consomme un tour".) |
| 8.6  | Les fabrications en caserne sont-elles **déclenchées automatiquement** chaque tour (file) ou le joueur doit-il **relancer** la production manuellement chaque tour ? |

### Déroulement

| #    | Question |
|------|----------|
| 8.7  | Y a-t-il une **limite de temps** par tour ou le joueur a-t-il tout le temps qu'il veut ? |
| 8.8  | Quel est l'**ordre des phases** dans un tour ? (Ex : 1. Revenus → 2. Actions → 3. Résolution → 4. Fin de tour ?) |
| 8.9  | Les **revenus** des zones de farm sont-ils collectés au **début** du tour du joueur qui les possède ou à un autre moment ? |
| 8.10 | Les **pièces en production** avancent-elles leur compteur au début du tour ou à la fin ? |
| 8.11 | Les **gains d'XP à l'arène** sont-ils attribués au début ou à la fin du tour ? |
| 8.12 | Comment l'**IA** joue-t-elle son tour ? Le joueur voit-il les actions de l'IA en temps réel (animations) ou seulement le résultat ? |

---

## 9. Le système de formations

| #    | Question |
|------|----------|
| 9.1  | "Adjacentes" signifie-t-il les **4 directions** (haut, bas, gauche, droite) ou aussi les **8 directions** (incluant les diagonales) ? |
| 9.2  | Une formation peut-elle contenir des pièces de **types différents** (ex : 3 fous + 2 pions ensemble) ? Si oui, quel **pattern de déplacement** s'applique ? |
| 9.3  | Si une formation contient des pièces de types différents, se déplace-t-elle selon la pièce la **plus lente** ? La plus nombreuse ? Ou les formations doivent-elles être **homogènes** (un seul type de pièce) ? |
| 9.4  | Le déplacement d'une formation compte-t-il comme **une seule action** ou comme autant d'actions que de pièces dans la formation ? |
| 9.5  | Quand une formation se déplace, la **disposition relative** des pièces entre elles est-elle conservée ? |
| 9.6  | Y a-t-il un **nombre maximum** de pièces dans une formation ? |
| 9.7  | Que se passe-t-il si le mouvement de la formation amène certaines pièces sur des **obstacles** (murs, eau, bâtiments) ? La formation entière est-elle bloquée ou se disloque-t-elle ? |
| 9.8  | Les formations sont-elles **créées automatiquement** dès que des pièces sont adjacentes ou le joueur doit-il **manuellement** regrouper ses pièces en formation ? |
| 9.9  | Peut-on **dissoudre** une formation volontairement ? |

---

## 10. Configuration initiale et début de partie

| #     | Question |
|-------|----------|
| 10.1  | Où exactement le **pion initial** (futur roi) apparaît-il ? (Coin de la carte ? Bord ? Position aléatoire ? Position symétrique à l'adversaire ?) |
| 10.2  | Où est placée la **caserne de départ** par rapport au pion initial ? (Adjacente ? À quelle distance ?) |
| 10.3  | Les deux camps ont-ils des **positions de départ symétriques** (miroir) sur la carte ? |
| 10.4  | La caserne de départ est-elle **déjà fonctionnelle** immédiatement ou nécessite-t-elle une activation/un premier tour de construction ? |
| 10.5  | Le pion initial a-t-il un **statut spécial** avant de devenir roi ? (Peut-il être tué ? Si oui, c'est game over immédiat ?) |
| 10.6  | Le pion initial peut-il **farmer** lui-même sur une zone de farm pour obtenir des premiers écus, ou est-ce seulement sa présence qui suffit ? |
| 10.7  | Combien d'écus faut-il pour recruter le **premier pion** dans la caserne (seuil minimal pour démarrer) ? |
| 10.8  | Quand le pion initial **devient roi**, change-t-il de **règles de déplacement** ? (Passe de déplacement pion à déplacement roi ?) |
| 10.9  | Si le roi est tué alors qu'il n'a plus de sujets, redevient-il un pion ou est-ce la fin de la partie ? |

---

## 11. Condition de victoire et fin de partie

| #     | Question |
|-------|----------|
| 11.1  | L'**échec et mat** est la condition de victoire. Mais sur une carte ouverte de 64×64, comment l'échec et mat est-il défini exactement ? (Le roi est "en échec" quand une pièce ennemie peut l'atteindre ; "mat" quand il ne peut ni fuir, ni être protégé, ni capturer l'attaquant ?) |
| 11.2  | Le concept d'**échec** (menace sur le roi) existe-t-il pendant le jeu ? Le joueur est-il **obligé** de sortir d'échec, comme aux échecs classiques ? |
| 11.3  | Le **pat** (aucun mouvement légal mais pas en échec) provoque-t-il un match nul ? |
| 11.4  | Peut-il y avoir un **match nul** d'une autre manière ? (Timeout, accord mutuel, répétition de positions…) |
| 11.5  | Le roi peut-il être **capturé directement** (prise du roi = victoire) ou faut-il passer obligatoirement par la mécanique échec/mat ? |
| 11.6  | Si le roi adverse **n'a pas encore de sujets** (est encore un pion au début), peut-on le tuer pour gagner ? |
| 11.7  | Y a-t-il un **écran de victoire/défaite** ? Que montre-t-il ? (Statistiques, replay, score…) |

---

## 12. Événements aléatoires et météo

### Météo

| #     | Question |
|-------|----------|
| 12.1  | À quelle **fréquence** les événements météo se déclenchent-ils ? (Tous les X tours ? Probabilité par tour ?) |
| 12.2  | Un événement météo affecte-t-il **toute la carte** ou une **zone localisée** ? |
| 12.3  | Combien de **tours** dure un événement météo ? |
| 12.4  | Le **brouillard** cache les pièces : cache-t-il les pièces ennemies uniquement ou aussi les siennes ? Sur quelle zone ? |
| 12.5  | Le brouillard fait jouer les pièces "toutes seules" : qu'est-ce que cela signifie exactement ? (Les pièces dans le brouillard se déplacent aléatoirement ? Elles attaquent automatiquement ? Le joueur perd le contrôle ?) |
| 12.6  | La **pluie** réduit la portée : de combien ? (Divisée par 2 ? Moins X cases ? S'applique à toutes les pièces ?) |
| 12.7  | Y a-t-il d'autres types de météo à prévoir au-delà du brouillard et de la pluie ? |
| 12.8  | Est-il possible d'avoir **plusieurs événements météo simultanés** ? |
| 12.9  | Le joueur est-il **averti** à l'avance qu'un événement météo va se produire ou est-ce une surprise totale ? |

### Cases piégées et événements

| #     | Question |
|-------|----------|
| 12.10 | Combien de **cases piégées** sont générées sur la carte ? |
| 12.11 | Les cases piégées sont-elles **visibles** ou **cachées** ? |
| 12.12 | Que fait une case piégée quand une pièce marche dessus ? (Dégât, téléportation, perte de tour, mort instantanée…) |
| 12.13 | Les **événements sur cases** mentionnés avec des dialogues à choisir : combien de types d'événements différents ? Quels sont-ils ? Quelles sont les options de dialogue et leurs conséquences ? |
| 12.14 | Les événements sur cases sont-ils **générés au début de la partie** ou apparaissent-ils **dynamiquement** en cours de jeu ? |
| 12.15 | Est-ce que les deux royaumes peuvent déclencher le **même** événement de case ou un événement est-il consommé après une seule activation ? |

---

## 13. Mobs neutres

| #     | Question |
|-------|----------|
| 13.1  | Combien de **mobs neutres** sont générés sur la carte ? |
| 13.2  | Quel **type de pièce** sont les mobs neutres ? (Pions ? Pièces spéciales ? Types variés ?) |
| 13.3  | Quelle est la **portée de vision** des mobs neutres ? (Nombre de cases autour d'eux ?) |
| 13.4  | Les mobs neutres se **déplacent-ils** ou restent-ils **statiques** ? |
| 13.5  | Si un mob est défensif et ne chasse pas, **à quel moment** attaque-t-il ? (Seulement quand une pièce entre dans sa case ? Quand une pièce est dans sa portée de vision et à portée d'attaque ?) |
| 13.6  | Les mobs neutres ont-ils **1 PV** comme les pièces classiques ou davantage ? |
| 13.7  | Tuer un mob neutre rapporte-t-il de l'**XP** ? Des **écus** ? Autre chose ? |
| 13.8  | Les mobs neutres peuvent-ils se trouver **sur** une zone de farm et bloquer son exploitation (comme une pièce ennemie) ? |
| 13.9  | Les mobs neutres **respawnent-ils** après avoir été tués ? Si oui, où et après combien de tours ? |
| 13.10 | À quel moment du tour les mobs neutres **agissent-ils** ? (Entre les tours des deux royaumes ? Pendant le tour de chaque joueur ?) |

---

## 14. Système de déité — Dieu, Diable et sacrifice

### Mécaniques de sacrifice

| #     | Question |
|-------|----------|
| 14.1  | Comment un joueur **effectue-t-il** un sacrifice ? (Action dédiée pendant le tour ? Menu spécial ? Bâtiment nécessaire ?) |
| 14.2  | Peut-on sacrifier **n'importe quelle pièce** ou seulement des pions ? |
| 14.3  | Quel est le **bonus de probabilité** exact par pièce sacrifiée ? (Le brainstorming dit "+X% par pion" — quelle valeur pour X ?) |
| 14.4  | Le bonus de sacrifice est-il **permanent** (pour toute la partie) ou **temporaire** (X tours) ? |
| 14.5  | Y a-t-il un **maximum** de bonus cumulable via les sacrifices ? |
| 14.6  | Les sacrifices sont-ils effectués **n'importe où** sur la carte ou à un endroit spécifique (ex : un autel, l'église) ? |

### Système de déité / affiliation

| #     | Question |
|-------|----------|
| 14.7  | Le système Dieu/Diable est-il **retenu** dans le scope actuel d'implémentation ou est-il reporté à plus tard ? |
| 14.8  | Si retenu : quels **critères d'actions** font pencher l'affiliation vers Dieu vs Diable ? Liste exhaustive des actions trackées ? |
| 14.9  | Quelles **faveurs/bonus** le Dieu accorde-t-il ? Et le Diable ? |
| 14.10 | L'affiliation est-elle représentée par une **jauge** (type curseur 0-100 entre Dieu et Diable) ou un **système de points** indépendants pour chacun ? |
| 14.11 | Les interventions du dieu "woke" (rééquilibrage) sont-elles **automatiques** ou le joueur les déclenche-t-il ? |
| 14.12 | Si plusieurs dieux : combien de dieux ? Quelles sont leurs **thématiques** respectives ? Quels sont leurs bonus/malus ? |
| 14.13 | L'IA ennemie interagit-elle aussi avec le **système de déité** ? |

---

## 15. Intelligence artificielle (IA ennemie)

| #     | Question |
|-------|----------|
| 15.1  | Quel **niveau de complexité** pour l'IA ? (IA simple avec règles basiques, IA avec arbre de décision, IA avec évaluation minimax, IA basée sur le machine learning…) |
| 15.2  | Y a-t-il plusieurs **niveaux de difficulté** sélectionnables par le joueur ? |
| 15.3  | L'IA a-t-elle accès aux **mêmes informations** que le joueur (pas d'omniscience, brouillard de guerre) ou voit-elle toute la carte ? |
| 15.4  | L'IA gère-t-elle la **construction** (casernes, murs), le **farm** (occupation de zones), les **upgrades**, et les **mariages** comme le joueur ? |
| 15.5  | L'IA interagit-elle avec le **système de sacrifice/déité** ? |
| 15.6  | L'IA a-t-elle une **personnalité** (agressive, défensive, économique) ou un comportement unique ? |
| 15.7  | L'IA respecte-t-elle les mêmes **contraintes** que le joueur (même nombre d'actions par tour, mêmes coûts, etc.) ou a-t-elle des avantages/triche (ex : ressources bonus en difficulté haute) ? |

---

## 16. Interface utilisateur (UI) et menus

### Menu principal

| #     | Question |
|-------|----------|
| 16.1  | Quels sont les **éléments du menu principal** ? (Nouvelle partie, Continuer, Options, Crédits, Quitter…) |
| 16.2  | Y a-t-il un **menu de configuration de partie** avant de lancer un jeu ? (Taille de carte, difficulté IA, seed…) |
| 16.3  | Y a-t-il un **menu Options** ? Si oui, quels paramètres ? (Volume, résolution, plein écran, raccourcis clavier, langue…) |

### HUD en jeu

| #     | Question |
|-------|----------|
| 16.4  | Quels éléments sont affichés dans le **HUD** permanent pendant la partie ? (Nombre d'écus, numéro du tour, indicateur de qui joue, minimap, bouton "Suivant"…) |
| 16.5  | Y a-t-il une **minimap** ? Si oui, de quelle taille et que montre-t-elle ? |
| 16.6  | Comment les **informations d'une pièce** sont-elles affichées quand on la sélectionne ? (Panneau latéral ? Tooltip ? Pop-up ?) |
| 16.7  | Quelles informations apparaissent sur le panneau de la pièce sélectionnée ? (Type, PV, XP actuelle/max, portée, niveau, possibilité d'upgrade…) |
| 16.8  | Les **cases accessibles** par une pièce sélectionnée sont-elles **surlignées** sur la carte ? De quelle couleur ? |
| 16.9  | L'**interface de la caserne** (choix des troupes à produire) est-elle un pop-up, un panneau latéral, un menu dédié ? |
| 16.10 | L'**outil de construction** du roi : comment est-il présenté ? (Barre d'outils en bas ? Menu contextuel au clic droit ? Roue de sélection ?) |
| 16.11 | Y a-t-il un **indicateur visuel** pour savoir quelles zones de farm sont occupées/disputées/libres ? |
| 16.12 | Y a-t-il un **journal d'événements** (log) qui liste ce qui s'est passé au tour précédent ? |
| 16.13 | Comment sont affichés les **événements météo** à l'écran ? (Overlay sur la carte ? Icône ? Notification texte ?) |
| 16.14 | Y a-t-il un **indicateur d'affiliation** Dieu/Diable visible dans le HUD ? |
| 16.15 | Les **dialogues** des événements sur cases : comment sont-ils affichés ? (Pop-up central ? Bulle de dialogue ? Panneau narratif ?) |
| 16.16 | Y a-t-il un menu **pause** en jeu ? |

### Interactions joueur

| #     | Question |
|-------|----------|
| 16.17 | Le jeu se joue-t-il à la **souris** seule, au **clavier** seul, ou **souris + clavier** ? |
| 16.18 | Quels sont les **raccourcis clavier** prévus ? (Fin de tour, sélection rapide, caméra…) |
| 16.19 | Comment sélectionne-t-on une pièce ? (Clic gauche ? Clic et drag pour sélectionner plusieurs ?) |
| 16.20 | Comment ordonne-t-on un déplacement ? (Clic gauche sur pièce → clic gauche sur destination ? Drag & drop ?) |
| 16.21 | Comment ordonne-t-on une **attaque** ? (Le déplacement sur une pièce ennemie est automatiquement une attaque, comme aux échecs ?) |
| 16.22 | Y a-t-il un système de **confirmation** avant action (pour éviter les erreurs) ou les actions sont-elles instantanées ? |
| 16.23 | Peut-on **annuler** une action (undo) dans le même tour avant de passer au suivant ? |

---

## 17. Audio — Musique et effets sonores

| #     | Question |
|-------|----------|
| 17.1  | Y a-t-il de la **musique** ? Si oui, quel style ? (Médiéval, orchestral, ambiant, 8-bit…) |
| 17.2  | Y a-t-il des **effets sonores** ? (Déplacement de pièces, capture, construction, destruction, météo, UI clicks…) |
| 17.3  | Y a-t-il des **voix** ou des sons pour les dialogues d'événements ? |
| 17.4  | La musique change-t-elle selon le **contexte** ? (Combat, exploration, tension, victoire, défaite…) |
| 17.5  | Le son est-il une **priorité** pour la première version ou est-il reporté ? |

---

## 18. Caméra et navigation sur la carte

| #     | Question |
|-------|----------|
| 18.1  | La caméra est-elle en **vue du dessus** (top-down) ou en **vue isométrique** ? |
| 18.2  | Le joueur peut-il **zoomer/dézoomer** ? Quelles sont les limites de zoom ? |
| 18.3  | Le joueur peut-il **déplacer la caméra** librement ? (Flèches directionnelles, clic molette + drag, bords de l'écran…) |
| 18.4  | Y a-t-il un raccourci pour **centrer la caméra** sur le roi ou sur une unité spécifique ? |
| 18.5  | Le joueur voit-il **toute la carte** en permanence ou y a-t-il un **brouillard de guerre** (fog of war) masquant les zones non explorées ? |
| 18.6  | Si brouillard de guerre : les zones déjà explorées restent-elles **visibles** (mais sans infos en temps réel) ou redeviennent-elles **masquées** ? |

---

## 19. Sauvegarde et persistance

| #     | Question |
|-------|----------|
| 19.1  | Le joueur peut-il **sauvegarder** sa partie en cours ? |
| 19.2  | Le système de sauvegarde est-il **manuel** (le joueur choisit quand sauvegarder) ou **automatique** (autosave à chaque tour) ? |
| 19.3  | Y a-t-il plusieurs **emplacements** de sauvegarde ? |
| 19.4  | Le format de sauvegarde : quel type de fichier ? (JSON, binaire, base de données locale…) |
| 19.5  | Y a-t-il un système de **replay** pour revoir une partie terminée ? |

---

## 20. Équilibrage et game design chiffré

> Cette section regroupe toutes les **valeurs numériques** qui doivent être fixées pour que le jeu fonctionne. Chaque valeur est une décision de game design.

### Tableau récapitulatif des paramètres à définir

| #     | Paramètre | Valeur à définir |
|-------|-----------|-----------------|
| 20.1  | Taille de la carte | ? × ? cases |
| 20.2  | Écus de départ | ? écus |
| 20.3  | Écus rapportés par une mine (par tour) | ? écus/tour |
| 20.4  | Écus rapportés par un champ (par tour) | ? écus/tour |
| 20.5  | Coût pion (caserne) | ? écus |
| 20.6  | Coût cavalier (caserne) | ? écus |
| 20.7  | Coût fou (caserne) | ? écus |
| 20.8  | Coût tour (caserne) | ? écus |
| 20.9  | Tours de fabrication — pion | ? tours |
| 20.10 | Tours de fabrication — cavalier | ? tours |
| 20.11 | Tours de fabrication — fou | ? tours |
| 20.12 | Tours de fabrication — tour | ? tours |
| 20.13 | Coût construction caserne | ? écus |
| 20.14 | Coût construction mur de bois | ? écus |
| 20.15 | Coût construction mur de pierre | ? écus |
| 20.16 | PV mur de bois | 1 (confirmé) |
| 20.17 | PV mur de pierre | 3 (confirmé) |
| 20.18 | PV d'une case de caserne | ? |
| 20.19 | Portée pion — base | ? cases |
| 20.20 | Portée pion — max | ? cases |
| 20.21 | Portée cavalier — base (ou fixe) | ? cases |
| 20.22 | Portée fou — niv.1 | 4 cases (évoqué) |
| 20.23 | Portée fou — niv.2 | 6 cases (évoqué) |
| 20.24 | Portée fou — niv.3 | 8 cases (évoqué) |
| 20.25 | Portée tour — base | ? cases |
| 20.26 | Portée tour — max | ? cases |
| 20.27 | Portée reine — base | ? cases |
| 20.28 | Portée reine — max | ? cases |
| 20.29 | XP max pion (pour upgrade) | 500 (évoqué) |
| 20.30 | XP max cavalier | ? |
| 20.31 | XP max fou | ? |
| 20.32 | XP max tour | 1000 (évoqué) |
| 20.33 | XP donnée par kill d'un pion | ? |
| 20.34 | XP donnée par kill d'un cavalier | ? |
| 20.35 | XP donnée par kill d'un fou | ? |
| 20.36 | XP donnée par kill d'une tour | ? |
| 20.37 | XP donnée par kill d'une reine | ? |
| 20.38 | XP par tour à l'arène | ? |
| 20.39 | XP donnée par destruction d'un mur | ? |
| 20.40 | XP donnée par destruction d'une case de caserne | ? |
| 20.41 | Points d'action par tour (si retenu) | ? PA |
| 20.42 | PA — déplacer un pion | 1 PA (évoqué) |
| 20.43 | PA — déplacer cavalier/fou | 2 PA (évoqué) |
| 20.44 | PA — déplacer tour | 3 PA (évoqué) |
| 20.45 | PA — déplacer reine | ? PA |
| 20.46 | PA — déplacer roi | ? PA |
| 20.47 | Rayon de construction du roi | ? cases |
| 20.48 | Nombre de mines sur la carte | ? |
| 20.49 | Nombre de champs sur la carte | ? |
| 20.50 | Nombre de mobs neutres | ? |
| 20.51 | Portée de vision des mobs | ? cases |
| 20.52 | Bonus probabilité par sacrifice de pion | +?% |
| 20.53 | Fréquence d'événements météo | ? (tous les X tours ou Y% par tour) |
| 20.54 | Durée d'un événement météo | ? tours |
| 20.55 | Réduction de portée sous la pluie | ? cases ou ?% |
| 20.56 | Taille de l'église | 4×3 (confirmé) |
| 20.57 | Taille de l'arène | ~ 9 cases (à préciser) |
| 20.58 | Taille de la caserne | 2×3 (évoqué, à confirmer) |
| 20.59 | Taille d'une mine | ? cases |
| 20.60 | Taille d'un champ | ? cases |

---

## 21. Lore et univers narratif

| #     | Question |
|-------|----------|
| 21.1  | Le jeu a-t-il un **contexte narratif** ? (Histoire, backstory des deux royaumes en guerre, raison du conflit…) |
| 21.2  | Les deux royaumes ont-ils des **noms** ? Des **identités visuelles** distinctes (armoiries, bannières, thèmes de couleur) ? |
| 21.3  | Le roi a-t-il un **nom/titre** ? (Simplement "Roi" ou un nom que le joueur choisit ?) |
| 21.4  | Les dieux/le diable ont-ils des **noms**, des **apparences**, des **personnalités** définies ? |
| 21.5  | Le ton du jeu est-il **sérieux**, **humoristique/parodique** (en lien avec le titre "A Normal Chess Game"), ou un **mélange** des deux ? |
| 21.6  | Y a-t-il des **textes descriptifs** (ex : tooltip des bâtiments, descriptions des pièces, textes d'événements) ? Si oui, dans quelle **langue** ? (Français, anglais, multilingue ?) |
| 21.7  | Les événements sur cases ont-ils des **textes narratifs** à écrire ? Si oui, combien d'événements distincts et quelle longueur pour chaque dialogue ? |
| 21.8  | Y a-t-il un **tutoriel** ou un **mode apprentissage** pour guider le joueur dans ses premiers tours ? |

---

*Ce document sera mis à jour au fur et à mesure des décisions prises.*
