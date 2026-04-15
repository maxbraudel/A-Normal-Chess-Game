# A Normal Chess Game — Réponses aux questions d'implémentation

> Ce document reprend chaque question du document de questions d'implémentation et y apporte la réponse décidée. Les réponses incluent l'intégralité des informations fournies, y compris les précisions, corrections et compléments apportés en dehors du cadre strict de chaque question.

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

**1.1 — Quel moteur de jeu est utilisé ?**
> Pas de réponse fournie. À déterminer.

**1.2 — Quel langage de programmation principal ?**
> Pas de réponse fournie. À déterminer.

**1.3 — Le jeu est-il en 2D ou en 3D ?**
> Le jeu est en **100% 2D**, en **vue du dessus** (top-down). Pas de vue isométrique.

**1.4 — Quelles sont les plateformes cibles ?**
> Pas de réponse explicite fournie. À déterminer.

### Architecture logicielle

**1.5 — L'architecture du code doit-elle suivre un pattern spécifique ?**
> Pas de pattern imposé. Cependant, il est souligné que le game engine doit être **très bien développé**, avec un système **itératif**, notamment pour la génération de bâtiments publics, avec des règles de génération bien définies. La philosophie est que le jeu soit codé de manière **neutre** : deux royaumes avec exactement les mêmes pouvoirs et fonctionnalités, l'un contrôlé par le joueur, l'autre par l'IA.

**1.6 — Comment est structuré le projet ?**
> Au runtime, les textures utilisées par le jeu sont chargées depuis le dossier `assets/textures/`. La structure active est :
> ```
> assets/
>   textures/
>     cells/          → textures de blocs/cellules (16×16 pixels chacune)
>       barrak.png
>       bridge.png
>       church.png
>       dirt.png
>       farm.png
>       grass.png
>       mine.png
>       wall_stone.png
>       wall_wood.png
>       water.png
>     pieces/         → sprites des pièces d'échecs
>       black/        → pièces du royaume noir
>         bishop.png, king.png, knight.png, pawn.png, queen.png, rook.png
>       white/        → pièces du royaume blanc
>         bishop.png, king.png, knight.png, pawn.png, queen.png, rook.png
>     ui/             → icônes UI
>       crossed_swords.png
>       shield_black.png
>       shield_white.png
> ```
>
> Le dossier `src/textures/` existe encore mais il n'est pas chargé par le runtime actuel. Il sert aujourd'hui de reliquat historique et de zone de travail/source art pour certaines textures générées. Le code exécutable charge `assets/`, pas `src/`.

**1.7 — Le jeu doit-il supporter le multijoueur à terme ou est-ce uniquement joueur solo vs IA ?**
> Pour l'instant, c'est uniquement **joueur solo vs IA**. Le joueur contrôle le **royaume blanc**, l'IA contrôle le **royaume noir**.

**1.8 — Y a-t-il un budget de performance à respecter ?**
> Pas de contrainte explicite mentionnée.

**1.9 — Faut-il prévoir un système de modding ou d'extension par les joueurs ?**
> Pas de système de modding prévu.

---

## 2. La carte — Dimensions, génération et terrain

### Dimensions

**2.1 — Quelle est la taille définitive de la carte ?**
> La carte n'est **pas rectangulaire** : c'est un **cercle parfait** (pixelisé, puisque la carte est une grille de cellules). Le **rayon** de ce cercle est de **8 × 64 = 512 cases**. C'est au développeur de calculer la forme de ce cercle pixelisé. La taille doit être **paramétrable** dans le back-end du jeu via une **variable simple** accessible au développeur, mais **pas par l'utilisateur**.

**2.2 — La taille de la carte est-elle fixe ou configurable par le joueur avant la partie ?**
> La taille est **configurable uniquement par le développeur** dans le back-end (variable globale). L'utilisateur ne peut pas la modifier.

**2.3 — La taille de la carte peut-elle être rectangulaire ou est-elle toujours carrée ?**
> Ni l'un ni l'autre : la carte est un **cercle pixelisé**. Les cases en dehors du cercle sont considérées comme hors-carte.

### Génération procédurale

**2.4 — Quel algorithme de génération procédurale ?**
> L'algorithme doit être le **plus naturel et le plus optimisé pour les petites cartes**. Le choix est laissé au développeur. Pour l'instant, l'algorithme est **très simple** : il génère une carte composée de **blocs d'herbe (grass)** avec, à certains endroits, des **points d'eau** (petits lacs) et des **zones de terre (dirt)** qui sont purement décoratives (des zones un peu plus sèches, avec moins d'herbe).

**2.5 — Faut-il un seed visible/partageable pour reproduire une même carte ?**
> **Non.** L'utilisateur peut créer une partie comme dans la plupart des jeux, mais il ne voit pas la seed et ne peut pas la partager.

**2.6 — La carte doit-elle être symétrique par rapport aux deux camps ?**
> **Non**, la carte peut être **complètement asymétrique**.

**2.7 — Comment est déterminé le placement des bâtiments publics ?**
> **Correction importante par rapport au brainstorming :** les arènes ne sont **plus** des bâtiments publics. Ce sont des **bâtiments constructibles** par le roi, comme les casernes. Il n'y a **pas de spawn** ou de placement prédéfini pour les arènes ; c'est le roi du royaume qui les place.
>
> Pour les bâtiments publics restants :
> - **Mines et champs** : générés **aléatoirement** sur la carte. Il faut **2 mines** et **3 champs** par carte. Les règles de génération sont :
>   - Un bâtiment public ne peut **pas spawner par-dessus un autre** bâtiment.
>   - Il faut une **distance minimum de 10 blocs** entre les bâtiments publics.
> - **L'église** : toujours positionnée **au centre de la carte**.
>
> Le game engine doit être bien développé pour avoir un système **itératif** de génération de bâtiments publics avec des règles de génération claires.

**2.8 — Combien de mines et de champs sont générés sur la carte ?**
> **2 mines** et **3 champs** par carte.

**2.9 — Les arènes sont-elles aussi générées à des positions aléatoires ?**
> Les arènes ne sont **plus des bâtiments publics** : ce sont des **bâtiments constructibles** par le roi. Elles ne sont pas générées, elles sont **placées par le joueur/l'IA** où il le souhaite sur la carte.

### Types de terrain

**2.10 — L'eau est-elle un terrain infranchissable ?**
> **Oui**, l'eau est **complètement infranchissable** par toutes les pièces, sans exception. Aucune pièce ne peut la traverser. L'eau est également **inconstructible** : on ne peut rien construire dessus, on ne peut rien y poser.

**2.11 — Y a-t-il des effets de terrain ?**
> **Non, pas pour le moment.** On peut concevoir que chaque bloc puisse, à terme, permettre ou non des choses pour les pièces, mais pour l'instant, tous les blocs ont les mêmes propriétés, **à l'exception de l'eau** qui est bloquante et inconstructible.

**2.12 — La terre et l'herbe sont-elles mécaniquement différentes ?**
> **Non, aucune différence fonctionnelle.** La terre est purement décorative. Au niveau de la génération, il y aura des zones un peu plus sèches avec de la terre, des petites zones avec un peu moins d'herbe, mais c'est uniquement visuel.

**2.13 — Peut-on construire des bâtiments sur n'importe quel type de terrain ?**
> On peut construire sur **tous les terrains sauf l'eau**. L'eau est inconstructible.

**2.14 — Y a-t-il d'autres types de terrain à prévoir ?**
> **Non, pas pour l'instant.** On fera ça plus tard. Les terrains actuels sont : herbe, terre (décorative) et eau.

**2.15 — L'eau est-elle générée en blocs contigus ou en cases isolées ?**
> L'eau est générée sous forme de **petits lacs** (blocs contigus), dispersés de temps en temps sur la carte. Pas de proportion maximale spécifiée.

---

## 3. Rendu visuel — Direction artistique et esthétique

### Style graphique

**3.1 — Quel est le style artistique visé ?**
> **Pixel art.**

**3.2 — Quelle est la résolution cible du jeu ?**
> **Résolution adaptative** par rapport à l'écran de l'utilisateur. La fenêtre de jeu doit pouvoir être **complètement redimensionnable** sans problème. La carte est un canevas sur lequel on peut zoomer, dézoomer et déplacer la caméra. Au-dessus du contenu du jeu, des éléments UI/UX sont **ancrés** sur les côtés de la fenêtre de l'application.

**3.3 — Y a-t-il une palette de couleurs imposée ?**
> Pas de palette imposée, mais l'ambiance doit être **plutôt sombre et médiévale**. Le choix exact est laissé au développeur.

**3.4 — Le jeu doit-il avoir un aspect parodique/humoristique ou sérieux ?**
> **Aspect sérieux.**

### Textures et sprites

**3.5 — Quelle est la taille d'une case en pixels à l'écran ?**
> Les cellules (cases de la grille) utilisent des textures de **16×16 pixels**. La carte est une grille contenant des cellules, et chaque cellule peut contenir un bloc de bâtiment ou un bloc de terrain. L'utilisateur peut zoomer et dézoomer sur cette grille avec la caméra.

**3.6 — Quelles textures de terrain sont nécessaires ?**
> Les textures de terrain utilisées au runtime sont fournies dans `assets/textures/cells/` :
> - `grass.png` — herbe
> - `dirt.png` — terre (décoratif)
> - `water.png` — eau
>
> La plupart des textures nécessaires pour le jeu ont été placées dans ce dossier. Les textures manquantes sont principalement celles de l'interface UI/UX, qui n'ont pas été fournies : l'interface doit être créée simplement par le développeur.

**3.7 — Y a-t-il des transitions visuelles entre types de terrain ?**
> **Non, pas pour l'instant.** Pas de transition visuelle entre les types de terrain.

**3.8 — Les pièces d'échecs ont-elles un sprite unique par type ou des variantes visuelles ?**
> **Sprite unique par type**, pas de variante visuelle. Les sprites utilisés au runtime sont fournis dans `assets/textures/pieces/` avec un dossier `black/` et un dossier `white/`, chacun contenant : `bishop.png`, `king.png`, `knight.png`, `pawn.png`, `queen.png`, `rook.png`.

**3.9 — Faut-il des animations pour les pièces ?**
> **Non, aucune animation** n'est nécessaire pour les pièces.

**3.10 — Comment les bâtiments sont-ils représentés visuellement quand ils occupent plusieurs cases ?**
> Historiquement, chaque case du bâtiment utilisait **la même texture** 16×16 par type, chargée depuis `assets/textures/cells/`.
>
> Ce n'est plus totalement vrai aujourd'hui pour les bâtiments publics chunkés : l'église, la mine et la ferme peuvent désormais charger une texture différente par case locale via `assets/textures/cells/structures/{building}/`.
>
> Les bâtiments non chunkés continuent à utiliser une texture unique par type dans `assets/textures/cells/` :
> - `barrak.png` — caserne
> - `church.png` — église
> - `farm.png` — champ
> - `mine.png` — mine
> - `wall_stone.png` — mur de pierre
> - `wall_wood.png` — mur de bois
> - `bridge.png` — pont (texture présente, usage à définir)

**3.11 — Comment est visuellement représentée la destruction progressive d'un bâtiment ?**
> Les cases détruites sont simplement **grisées**. Pas de fissures, pas de fumée, pas de sprite dégradé. Juste un grisage. Lorsque **toutes** les cases d'un bâtiment sont détruites/grisées, le bâtiment disparaît entièrement.

**3.12 — Quel visuel pour les murs de bois vs les murs de pierre ?**
> - **Mur de bois** : détruit instantanément en un coup (1 PV), donc **pas besoin** de visuel endommagé.
> - **Mur de pierre** (3 PV) : quand il est abîmé, il **devient grisé de plus en plus** au fur et à mesure qu'il perd des PV. Le grisage est proportionnel aux dégâts subis.

### Différenciation des royaumes

**3.13 — Comment sont différenciées visuellement les pièces des deux royaumes ?**
> Par les couleurs classiques : un **royaume blanc** et un **royaume noir**. Les textures correspondantes sont dans `assets/textures/pieces/white/` et `assets/textures/pieces/black/`.

**3.14 — Les bâtiments privés portent-ils les couleurs du royaume qui les a construits ?**
> Pas de réponse explicite. Les textures de bâtiments fournies (`barrak.png`, `wall_stone.png`, `wall_wood.png`) sont uniques (pas de variante blanc/noir).

**3.15 — Comment sont visuellement représentés les mobs neutres ?**
> Les mobs neutres **ne sont pas implémentés** dans le scope actuel. Reporté à plus tard.

---

## 4. Les pièces d'échecs — Règles de déplacement et portée

### Déplacements

**4.1 — Les pièces obéissent-elles strictement aux règles de déplacement des échecs classiques ?**
> **Oui**, les pièces obéissent **complètement** aux règles de déplacement des échecs classiques, **avec les adaptations suivantes** :
> - Le **pion** peut se déplacer dans **toutes les directions sauf en diagonale** (haut, bas, gauche, droite d'une case). C'est une différence par rapport aux échecs classiques où le pion ne va que vers l'avant.
> - Le concept de "direction de l'avant" pour le pion n'existe plus sur une carte ouverte.

**4.2 — Le pion peut-il avancer de 2 cases au premier coup ?**
> **Non.** Le pion ne peut pas avancer de 2 cases pour son premier coup.

**4.3 — La prise en passant existe-t-elle dans ce jeu ?**
> **Non**, la prise en passant n'existe pas dans le jeu.

**4.4 — Le roque existe-t-il dans ce jeu ?**
> **Non**, le roque n'existe pas dans le jeu.

**4.5 — Dans quelle direction un pion avance-t-il ?**
> Le pion peut se déplacer dans **toutes les directions sauf en diagonale** : haut, bas, gauche, droite (1 case). Il n'a plus de concept de "vers l'avant" puisque la carte est ouverte.

**4.6 — Les pièces peuvent-elles traverser d'autres pièces alliées ?**
> **Non**, sauf le cavalier qui peut sauter par-dessus des pièces, comme dans les règles classiques des échecs.

**4.7 — Les pièces peuvent-elles traverser les bâtiments ?**
> **Oui**, de manière générale. Tous les bâtiments actuels (publics et privés) sont constitués de blocs qui **peuvent être traversés** : église, mines, champs, casernes. Les **seules cellules non traversables** dans le jeu sont :
> - **L'eau** — infranchissable.
> - **Les murs** (de bois et de pierre) — non traversables.
>
> Les murs sont les seules constructions qui bloquent le passage.

**4.8 — Un pion peut-il être promu quand il atteint le bout de l'échiquier ?**
> **Non.** Il n'y a pas de promotion classique à la façon des échecs. Le seul moyen de créer une reine dans le jeu est la configuration suivante :
>
> **Correction majeure par rapport au brainstorming :** la création de la reine ne passe **plus** par le mariage roi + tour. La nouvelle règle est :
> - Il faut que dans l'église, sur les cases de l'église, soient présents : **un fou**, **un pion** et **le roi**.
> - Le **roi** et le **pion** doivent être sur des **cases adjacentes**.
> - Une fois cette configuration réunie, le **pion se transforme en reine**.

### Portée

**4.9 — Quelle est la portée initiale exacte de chaque type de pièce au niveau de base ?**
> La portée maximale de **toutes les pièces** est fixée à **8 cases**. C'est une variable globale dans le back-end. En dehors de cette limite, ce sont les **règles de déplacement classiques des échecs** qui s'appliquent :
> - **Pion** : 1 case (haut, bas, gauche, droite).
> - **Cavalier** : mouvement en L classique (fixe).
> - **Fou** : diagonale, **jusqu'à 8 cases maximum**.
> - **Tour** : lignes droites (horizontale/verticale), **jusqu'à 8 cases maximum**.
> - **Reine** : diagonale + lignes droites, **jusqu'à 8 cases maximum**.
> - **Roi** : 1 case dans toutes les directions.
>
> Cette limite de 8 cases s'inscrit comme une variable globale dans le back-end pour être facilement modifiable.

**4.10 — Quelle est la portée maximale de chaque type de pièce au niveau maximal ?**
> **8 cases** pour toutes les pièces. Il n'y a pas de système de portée par niveau pour le moment. La portée maximale est une **unique variable globale** valant 8.

**4.11 — La tour a-t-elle aussi une portée limitée ?**
> **Oui**, maximum **8 cases** (variable globale).

**4.12 — La reine a-t-elle aussi une portée limitée ?**
> **Oui**, maximum **8 cases** (variable globale).

**4.13 — Le roi a-t-il une portée limitée ?**
> Le roi se déplace de **1 case** dans toutes les directions, comme aux échecs classiques. La limite globale de 8 ne le concerne pas en pratique.

**4.14 — Le cavalier a-t-il une portée variable ?**
> **Non**, le cavalier garde son mouvement en L classique et fixe des échecs. La portée de 8 ne le concerne pas en pratique.

**4.15 — Le pion a-t-il une portée upgradable ?**
> **Non**, le pion se déplace de **1 case** dans les directions non diagonales. Pas de portée variable pour le pion.

**4.16 — La notion de "portée" s'applique-t-elle au mouvement et à l'attaque de la même manière ?**
> **Oui**, la portée est la même pour le mouvement et l'attaque. Le principe des échecs s'applique : une pièce attaque en se déplaçant sur la case de la cible.

---

## 5. Système d'expérience et d'upgrade

> **Refonte majeure par rapport au brainstorming :** Le système de pourcentage d'XP (0% à 100%) est **complètement abandonné**. Il est remplacé par un système de **seuils d'XP** sans plafond.

### Principes fondamentaux

Chaque pièce sur la carte possède une **identité propre et immuable** qui ne change pas, peu importe si elle est améliorée ou non. Cette identité regroupe :
- La **position** de la pièce sur la carte.
- Son **expérience** (valeur numérique sans plafond).
- Son **type actuel** (pion, cavalier, fou, tour, reine, roi).
- D'autres éléments à définir.

Quand une pièce est upgradée, c'est **uniquement son type qui change**. Tout le reste (position, XP, identité) est conservé.

### Valeurs d'XP

**5.1 — Quel est le seuil d'XP exact pour chaque type de pièce ?**
> Il n'y a **pas de limite d'XP**, pas de maximum, pas de pourcentage. L'XP s'accumule **sans plafond**. Ce sont des **seuils/paliers** qui déclenchent la possibilité d'upgrade :
>
> | Transition | Seuil d'XP requis |
> |---|---|
> | Pion → Cavalier **ou** Fou | ≥ 100 XP |
> | Cavalier ou Fou → Tour | ≥ 300 XP |
> | Tour → (bloqué) | Pas d'upgrade possible au-delà |
>
> La reine et le roi **peuvent aussi accumuler de l'XP**, mais pour l'instant, cette XP n'a **pas d'utilité** car ces pièces ne peuvent pas s'améliorer. L'XP accumulée servira potentiellement dans des versions futures.

**5.2 — Combien d'XP rapporte le fait de manger une pièce ?**
> | Pièce mangée | XP gagnée |
> |---|---|
> | Pion | 20 XP |
> | Cavalier | 50 XP |
> | Fou | 50 XP |
> | Tour | 100 XP |
> | Reine | 300 XP |

**5.3 — Combien d'XP rapporte la destruction d'un bloc ?**
> Si un **bloc est détruit** (que ce soit un mur de bois, un mur de pierre ou une case de caserne), la pièce qui l'a détruit gagne **10 XP**.

**5.4 — Combien d'XP par tour rapporte l'arène par pièce posée dessus ?**
> **10 XP par tour** pour chaque pièce posée sur une case de l'arène.

**5.5 — L'XP est-elle différente selon le niveau de la pièce qui mange vs qui est mangée ?**
> **Non**, l'XP dépend uniquement du **type de la pièce mangée** (voir tableau ci-dessus), pas du type de la pièce qui mange.

**5.6 — Y a-t-il un bonus d'XP pour le système de vétéran ?**
> Le système de vétéran **n'est pas retenu** dans le scope actuel.

### Mécaniques d'upgrade

**5.7 — L'upgrade est-elle gratuite ou coûte-t-elle des écus ?**
> L'upgrade **n'est pas gratuite**. Elle coûte des **écus en plus de l'XP** requise. Le coût en écus varie **selon le niveau de l'upgrade**. Les montants exacts ne sont pas encore définis.

**5.8 — L'upgrade s'applique-t-elle immédiatement ou au tour suivant ?**
> Pas de réponse explicite. À déterminer.

**5.9 — Quand une pièce est upgradée, conserve-t-elle sa position ?**
> **Oui, absolument.** La pièce a une identité immuable qui inclut la position. Quand elle est upgradée, c'est **juste son type qui change**. Tout le reste est conservé.

**5.10 — Quand une pièce est upgradée, sa portée passe-t-elle au niveau 1 de la nouvelle pièce ?**
> La portée est définie par les **règles classiques des échecs** pour chaque type de pièce, avec une **portée maximale globale de 8 cases** (variable back-end). Quand une pièce est upgradée, elle adopte simplement les **règles de déplacement de son nouveau type**.

**5.11 — Y a-t-il un nombre maximum de tours, cavaliers, fous, etc. par royaume ?**
> **Non**, il n'y a **aucune limite** sur le nombre de pièces de chaque type par royaume (hors la reine, limitée à **une seule** à la fois par royaume).

**5.12 — L'upgrade est-elle obligatoire quand l'XP atteint le seuil ?**
> **Non.** Il n'y a plus de système de pourcentage à 100%. L'XP continue de s'accumuler sans plafond. Le joueur **upgrade quand il veut**, du moment que le seuil est atteint.

---

## 6. Les ressources et l'économie

### Les écus

**6.1 — Quel est le nombre d'écus initial au début de la partie ?**
> **Zéro (0) écus** au début de la partie. Confirmé.

**6.2 — Y a-t-il un plafond (maximum stockable) d'écus ?**
> **Non**, pas de plafond ni de maximum d'écus pour le moment.

**6.3 — Les écus sont-ils partagés globalement pour le royaume ?**
> **Oui**, les écus sont partagés **globalement** pour tout le royaume. Il n'y a pas de ressources liées à une zone ou une caserne particulière.

### Zones de farm

**6.4 — Combien d'écus par tour rapporte une mine ?**
> Ce n'est pas la mine globalement qui rapporte, mais **chaque case de mine occupée**. Si une pièce est sur une case de mine, cela rapporte **10 écus par tour** par case.

**6.5 — Combien d'écus par tour rapporte un champ ?**
> **5 écus par tour** par case de champ occupée.

**6.6 — Quelle est la taille d'une mine ? D'un champ ?**
> | Bâtiment | Largeur | Hauteur |
> |---|---|---|
> | **Mine** | 6 blocs | 6 blocs |
> | **Champ (ferme)** | 4 blocs | 3 blocs |

**6.7 — Faut-il qu'au moins une pièce soit sur la zone de farm pour collecter ?**
> **Oui**, il faut qu'au moins une pièce du royaume soit posée sur **au moins une case** de la zone de farm. Le revenu est par case occupée (chaque case avec une pièce du royaume rapporte ses écus).

**6.8 — À quel moment du tour les écus sont-ils crédités ?**
> Les écus sont crédités **quand le tour est consommé** (quand le joueur appuie sur le bouton "Jouer"). C'est à la **fin du tour**, au moment de la consommation. Il faut évidemment que la pièce soit déjà sur la case à ce moment-là.

**6.9 — Si la zone est contestée au tour suivant, que se passe-t-il ?**
> Si la zone de farm, la mine ou l'église est **occupée par l'ennemi** au tour suivant, elle **ne rapporte plus**. Les règles de contestation s'appliquent normalement.

### Coûts

**6.10 — Quel est le coût en écus pour recruter chaque type de pièce à la caserne ?**
> Les coûts exacts en écus par type de pièce recrutée **n'ont pas été spécifiés** individuellement. À définir.

**6.11 — Quel est le coût en écus pour construire une caserne ?**
> **50 écus.** Note importante : quand on place un bâtiment (caserne, arène, etc.), on le place **entièrement** d'un coup, avec toutes ses cases. On ne place pas chaque bloc un par un.

**6.12 — Quel est le coût en écus pour construire un mur de bois ?**
> **20 écus.**

**6.13 — Quel est le coût en écus pour construire un mur de pierre ?**
> **40 écus.**

**6.14 — Combien de tours de fabrication prend chaque type de pièce à la caserne ?**
> La durée de fabrication dépend du **niveau du type** de pièce :
>
> | Niveau du type | Pièces concernées | Tours de fabrication |
> |---|---|---|
> | Niveau 0 | Pion | 2 tours |
> | Niveau 1 | Cavalier, Fou | 4 tours |
> | Niveau 2 | Tour | 6 tours |
>
> Il n'y a **pas de niveau 3 ou 4** en fabrication : on **ne peut pas fabriquer** de roi ni de reine en caserne. La reine est obtenue uniquement via le rituel à l'église. Le roi est le pion initial transformé.
>
> **Précision sur la hiérarchie de niveaux :** niveau 0 = pion, niveau 1 = fou/cavalier, niveau 2 = tour. (Note : il a été mentionné verbalement "deux = reine et trois = roi" dans un passage, mais cela est en contradiction avec les règles de fabrication et d'upgrade établies. En pratique, la fabrication s'arrête au niveau 2 = tour, et la reine/roi ne sont pas fabricables.)

**6.15 — Y a-t-il un coût de maintenance pour entretenir ses pièces ou ses bâtiments ?**
> **Non**, il n'y a pas de coût de maintenance. Les coûts sont **uniquement à la création**.

---

## 7. Les bâtiments — Dimensions, coûts et mécaniques

### La caserne

**7.1 — Quelle est la taille exacte de la caserne en cases ?**
> **4 blocs de large × 2 blocs de haut.**

**7.2 — Combien de points de vie a chaque case d'une caserne ?**
> **1 PV.** Si une pièce attaque une case de la caserne, cette case est immédiatement **grisée** et considérée comme **cassée**.

**7.3 — Une caserne peut-elle produire plusieurs unités en parallèle ?**
> **Absolument pas.** Une caserne ne peut produire qu'**une seule unité à la fois**. Il faut relancer la production manuellement après qu'une unité a été créée.

**7.4 — Sur quelles cases exactement une pièce apparaît-elle quand elle sort de la caserne ?**
> Sur **n'importe quelle case adjacente disponible** la plus proche. Si les cases adjacentes sont occupées par d'autres troupes ou des obstacles, la pièce apparaît sur la **case adjacente libre la plus proche**.

**7.5 — Y a-t-il un maximum de casernes constructibles par royaume ?**
> **Non**, aucun maximum.

**7.6 — Si une caserne est en cours de production et qu'elle est détruite, que se passe-t-il ?**
> La **production est annulée** et les **écus ne sont pas remboursés**.

**7.7 — Peut-on annuler une production en cours dans une caserne ?**
> **Non**, on ne peut pas annuler une production en cours pour récupérer une partie des écus.

### Les murs

**7.8 — Un mur bloque-t-il le passage de toutes les pièces alliées et ennemies ?**
> **Oui**, un mur bloque le passage de **toutes les pièces**, alliées et ennemies, **sauf** : le **cavalier allié** peut **sauter par-dessus un mur allié** (conformément à sa capacité de saut aux échecs). Par contre, un cavalier **ne peut pas** sauter par-dessus un mur **ennemi**.

**7.9 — Le cavalier peut-il sauter par-dessus un mur ?**
> Le cavalier peut uniquement sauter par-dessus **un mur allié**, mais **pas un mur ennemi**.

**7.10 — Peut-on réparer un mur endommagé ?**
> **Non**, on ne peut **pas réparer** de structures endommagées.

**7.11 — Y a-t-il une limite au nombre de murs constructibles ?**
> **Non**, aucune limite.

**7.12 — Les murs bloquent-ils la ligne de vue des fous, tours, et reines ?**
> **Oui, absolument.** C'est le principe des murs : ils sont considérés comme **bloquants pour la ligne de vue**. Les fous, tours et reines ne peuvent pas jouer au-delà d'un mur. Les murs sont bloquants pour la ligne de vue **y compris pour les cavaliers ennemis** (un cavalier ennemi ne peut pas sauter par-dessus).

### L'église

**7.13 — Quelles cases le roi et la tour doivent-ils occuper pour le mariage ?**
> **Correction par rapport au brainstorming :** le mariage ne se fait plus avec le roi et une tour. La nouvelle règle est :
> - Il faut que sur les cases de l'église soient présents : **un fou**, **un pion** et **le roi**.
> - Le **roi** et le **pion** doivent être sur des **cases adjacentes**.
> - Le **pion** se transforme alors en **reine**.
>
> N'importe quelles cases de l'église peuvent être utilisées, tant que les trois pièces y sont et que le roi et le pion sont adjacents.

**7.14 — Le mariage prend un tour pour être déclenché. Pendant ce tour d'attente, le roi et la tour doivent-ils rester dans l'église ?**
> Le mariage prend **un tour** pour être déclenché. Pas de précision explicite sur la nécessité de rester dans l'église pendant l'attente.

**7.15 — Si la reine est tuée, peut-on en recréer une via un nouveau mariage ?**
> **Oui**, si la reine est tuée, on peut en recréer une via un nouveau mariage. Il ne peut y avoir qu'**une seule reine à la fois** par royaume.

**7.16 — L'église bloque-t-elle le passage des pièces ?**
> **Non.** L'église ne bloque pas le passage des pièces. De manière générale, **les bâtiments ne bloquent pas du tout le passage des pièces**. Il n'y a que **les murs** et **l'eau** qui bloquent le passage.

### L'arène

**7.17 — Quelles sont les dimensions exactes de l'arène ?**
> Les dimensions ont été mentionnées comme "environ 9 blocs" dans le brainstorming. La réponse renvoie aux dimensions déjà données sans les repréciser. La forme exacte (3×3, etc.) reste à confirmer.

**7.18 — La pièce posée dans l'arène peut-elle aussi agir normalement ?**
> Pas de réponse explicite. Une pièce dans l'arène gagne de l'XP passivement par tour.

**7.19 — L'XP gagnée dans l'arène est-elle identique pour toutes les pièces ?**
> **Oui**, l'XP de l'arène est **identique pour toutes les pièces** : **10 XP par tour** pour chaque pièce posée sur une case de l'arène.

**7.20 — Les deux arènes sont-elles placées symétriquement ?**
> **L'arène est désormais un bâtiment constructible** (pas un bâtiment public). Il n'y a plus deux arènes prédéfinies. Chaque royaume **construit ses propres arènes** où il veut, sans limitation de nombre. Les arènes sont placées par le roi du royaume.

### Mines et champs

**7.21 — Les mines et les champs sont-ils visuellement et mécaniquement distincts ?**
> **Oui**, ils sont **visuellement distincts** (textures séparées : `mine.png` et `farm.png`). Ils sont aussi **mécaniquement distincts** : la mine rapporte **10 écus/tour/case** et le champ rapporte **5 écus/tour/case**. Leurs tailles sont aussi différentes (mine = 6×6, champ = 4×3).

**7.22 — Si distincts, quelle est la différence mécanique entre une mine et un champ ?**
> | | Mine | Champ |
> |---|---|---|
> | Rendement | 10 écus/tour/case | 5 écus/tour/case |
> | Taille | 6×6 blocs | 4×3 blocs |
> | Nombre sur la carte | 2 | 3 |

**7.23 — Les zones de farm ont-elles une forme fixe ou irrégulière ?**
> Elles ont une forme fixe **rectangulaire** (mine = 6×6, champ = 4×3).

### Destruction des bâtiments

**7.6 (rappel) — Destruction des bâtiments**
> - Les **bâtiments publics** (église, mines, champs) sont **indestructibles**.
> - Les **bâtiments privés** (casernes, murs, arènes) peuvent être détruits par l'ennemi.
> - Chaque case d'un bâtiment a ses propres PV.
> - Les cases détruites sont **grisées**.
> - Quand **toutes les cases** sont détruites, le bâtiment disparaît.

---

## 8. Le système de tour par tour — Mécaniques précises

### Actions par tour

**8.1 — Le joueur a-t-il un nombre limité d'actions par tour ou peut-il effectuer toutes les actions listées dans un seul tour ?**
> Le joueur peut effectuer **toutes les catégories d'actions** suivantes dans un **seul tour**, mais avec des limites par catégorie :
>
> | Catégorie d'action | Limite par tour |
> |---|---|
> | **Construction** (bâtiment) | **1 seul** bâtiment par tour |
> | **Mariage** à l'église | **1 seul** mariage par tour |
> | **Déplacement** (pièce ou formation) | **1 seul** déplacement par tour (n'importe quelle pièce) |
> | **Fabrication** (lancement de production en caserne) | **1 seule** fabrication par tour |
>
> Toutes ces catégories peuvent être effectuées **dans le même tour**. C'est-à-dire qu'en un tour, le joueur peut à la fois : construire 1 bâtiment, lancer 1 fabrication, faire 1 déplacement et célébrer 1 mariage.
>
> **Précision importante :** c'est le **roi** qui construit. La construction doit se faire dans la **périphérie directe** du roi (cases adjacentes).

**8.2 — Le système de points d'action (PA) est-il retenu ?**
> **Non, pas pour le moment.** Le système de PA sera implémenté **plus tard**. Pour l'instant, on part du principe qu'**un seul mouvement ou attaque** est possible par tour, toute pièce confondue. C'est juste un déplacement possible avec n'importe quelle pièce.

**8.3 — Si le système de PA est retenu, la construction coûte-t-elle des PA ?**
> Le système de PA **n'est pas retenu** pour le moment. Non applicable.

**8.4 — Est-ce bien un seul déplacement par tour ?**
> **Oui**, un seul déplacement par tour, pour **n'importe quelle pièce** du royaume.

**8.5 — Peut-on cumuler construire un bâtiment ET déplacer une pièce dans le même tour ?**
> **Oui.** Chaque dimension (construction, déplacement, mariage, fabrication) peut être effectuée dans le même tour. Elles ne s'excluent pas mutuellement.

**8.6 — Les fabrications en caserne sont-elles déclenchées automatiquement ?**
> **Non**, elles ne sont **pas automatiques**. Le joueur doit **relancer la production manuellement** à chaque fois. C'est lui qui choisit la troupe et à quel prix, si la caserne est disponible (pas déjà en cours de construction d'une autre troupe).

### Déroulement

**8.7 — Y a-t-il une limite de temps par tour ?**
> **Non**, le joueur a **tout le temps qu'il veut** pour jouer son tour.

**8.8 — Quel est l'ordre des phases dans un tour ?**
> Il n'y a **pas d'ordre de phases** séquentielles. Quand le tour est consommé, **tout se passe au même moment** : les actions, les revenus, tout est résolu simultanément.

**8.9 — Les revenus des zones de farm sont-ils collectés au début du tour ou à un autre moment ?**
> Les revenus sont collectés **à la fin**, quand le **tour est consommé** (bouton "Jouer"). Il faut que les pièces soient déjà présentes sur les cases de farm à ce moment-là.

**8.10 — Les pièces en production avancent-elles leur compteur au début ou à la fin du tour ?**
> Pas de réponse explicite distincte, mais par cohérence avec le système (tout se résout à la consommation du tour), les compteurs de production avancent à la **consommation du tour**.

**8.11 — Les gains d'XP à l'arène sont-ils attribués au début ou à la fin du tour ?**
> Par cohérence avec le système, les gains d'XP de l'arène sont attribués **à la consommation du tour**.

**8.12 — Comment l'IA joue-t-elle son tour ?**
> **Instantanément** après que le joueur a consommé son tour. L'IA calcule son tour et le consomme immédiatement. Il y a un temps de **chargement** pendant lequel l'IA réfléchit (calculs logiques), puis elle effectue son tour d'un coup. Tout s'articule autour du **roi ennemi** qui réfléchit à tout : recruter, attaquer, créer une arène, aller à l'église pour un mariage, etc.
>
> **Précision visuelle :** quand le joueur déplace une pièce pendant son tour, tant qu'il n'a pas consommé le tour, la pièce apparaît **grisée/en transparence** à l'endroit du déplacement prévu. Une **flèche** indique le mouvement. La pièce reste visible à sa position originale comme un **fantôme** de ce qui va se passer. Le déplacement réel n'a lieu **qu'à la consommation du tour**.

---

## 9. Le système de formations

**9.1 — "Adjacentes" signifie-t-il les 4 directions ou les 8 directions ?**
> Des pièces sont considérées comme adjacentes si elles sont **les unes à côté des autres** et **pas séparées par des cases vides**. Exemple donné : quatre fous ou quatre pions côte à côte. (Le nombre exact de directions — 4 ou 8 — n'a pas été explicitement précisé.)

**9.2 — Une formation peut-elle contenir des pièces de types différents ?**
> Pas de réponse explicite. Les exemples donnés concernent des formations de même type (4 fous, 4 pions).

**9.3 — Si une formation contient des pièces de types différents, quel pattern de déplacement s'applique ?**
> Non précisé (voir 9.2).

**9.4 — Le déplacement d'une formation compte-t-il comme une seule action ?**
> La formation se comporte **comme une pièce à part entière**. Elle se déplace selon la **règle de translation** de la pièce aux échecs (mouvement du type de pièce qui la compose). Quand le joueur sélectionne une des pièces de la formation, il **sélectionne toutes les pièces** de la formation en même temps, et le déplacement se fait **d'un seul bloc**.

**9.5 — Quand une formation se déplace, la disposition relative des pièces est-elle conservée ?**
> **Oui**, implicitement. La formation se comporte comme une **seule pièce** et se déplace par translation, donc la disposition relative est conservée.

**9.6 — Y a-t-il un nombre maximum de pièces dans une formation ?**
> Pas de réponse explicite. Aucun maximum mentionné.

**9.7 — Que se passe-t-il si le mouvement de la formation amène certaines pièces sur des obstacles ?**
> Pas de réponse explicite. À définir.

**9.8 — Les formations sont-elles créées automatiquement ou manuellement ?**
> **Manuellement.** Quand des pièces sont les unes à côté des autres, le joueur sélectionne une des pièces et un **bouton/option** apparaît, lui permettant de **créer** ou **défaire** la formation. La formation n'est **pas créée automatiquement** par le simple fait d'être adjacentes.

**9.9 — Peut-on dissoudre une formation volontairement ?**
> **Oui.** Le même bouton/option qui permet de créer la formation permet aussi de la **défaire**.

---

## 10. Configuration initiale et début de partie

**10.1 — Où exactement le pion initial apparaît-il ?**
> Le pion du joueur apparaît **aléatoirement** dans les **25% gauche** de la carte. Le pion ennemi apparaît dans les **25% droite** de la carte. (Note : il y a eu des hésitations entre 50% et 25% lors de la réponse, la valeur finale retenue est 25%.)

**10.2 — Où est placée la caserne de départ par rapport au pion initial ?**
> **Correction majeure par rapport au brainstorming :** il n'y a **pas de caserne de départ**. Le roi (pion initial) doit se débrouiller seul pour aller dans une **mine** ou un **champ**, récolter de l'argent à chaque tour, et ensuite pouvoir construire sa première caserne.

**10.3 — Les deux camps ont-ils des positions de départ symétriques ?**
> **Non**, les positions ne sont pas symétriques. Le joueur apparaît sur les 25% gauche de la carte, l'ennemi sur les 25% droite, mais les positions exactes sont aléatoires.

**10.4 — La caserne de départ est-elle déjà fonctionnelle ?**
> Il n'y a **pas de caserne de départ** (voir 10.2).

**10.5 — Le pion initial a-t-il un statut spécial avant de devenir roi ?**
> **Non**, le pion initial **n'a pas de statut spécial**. S'il est tué comme un pion normal, **la partie est finie** (game over).

**10.6 — Le pion initial peut-il farmer lui-même ?**
> **Oui.** Le pion initial (futur roi) peut aller lui-même dans une mine ou un champ pour récolter de l'argent à chaque tour. C'est même la première chose qu'il doit faire puisqu'il commence avec 0 écus et sans caserne.

**10.7 — Combien d'écus faut-il pour recruter le premier pion ?**
> Le coût exact par pièce n'a pas été spécifié individuellement. Il faut d'abord construire une caserne (50 écus), puis payer le coût de la pièce (non spécifié).

**10.8 — Quand le pion initial devient roi, change-t-il de règles de déplacement ?**
> **Oui.** Quand le pion initial devient roi (dès qu'il a au moins un sujet), il **change de statut** et se déplace comme un **roi** (1 case dans toutes les directions, y compris les diagonales) au lieu des règles du pion (1 case dans les 4 directions cardinales).

**10.9 — Si le roi est tué alors qu'il n'a plus de sujets, redevient-il un pion ?**
> **Non.** Le roi, quand il n'a plus de sujets, **reste roi**. Il ne redevient pas un pion. C'est une partie d'échecs classique à partir de là : il ne peut pas être tué directement, mais il peut être mis en **échec et mat**. Les règles d'échec et mat classiques s'appliquent.

---

## 11. Condition de victoire et fin de partie

**11.1 — Comment l'échec et mat est-il défini sur une carte ouverte de 64×64 ?**
> C'est un système d'**échec et mat classique** des échecs. Le roi doit être **incapable de bouger** car il est mis en joue par les pièces adverses (toutes les cases accessibles au roi sont contrôlées par l'ennemi, et il ne peut ni fuir, ni être protégé, ni capturer l'attaquant). Les mêmes règles des échecs classiques s'appliquent, adaptées à la grande carte.

**11.2 — Le concept d'échec existe-t-il pendant le jeu ?**
> **Oui**, le système d'échec et mat classique existe. Le roi est en échec quand une pièce ennemie peut l'atteindre. Le joueur doit sortir d'échec (règles classiques).

**11.3 — Le pat provoque-t-il un match nul ?**
> Pas de réponse explicite. À déterminer (en suivant les règles classiques des échecs, le pat serait un match nul).

**11.4 — Peut-il y avoir un match nul d'une autre manière ?**
> Pas de réponse explicite. À déterminer.

**11.5 — Le roi peut-il être capturé directement ou faut-il passer par échec/mat ?**
> Le roi **ne peut pas être capturé directement** une fois qu'il est roi. C'est la règle classique des échecs : il faut le mettre en **échec et mat**. Toutefois, **avant qu'il ne devienne roi** (quand c'est encore un pion initial), il **peut être tué** comme un pion normal, ce qui met fin à la partie.

**11.6 — Si le roi adverse n'a pas encore de sujets, peut-on le tuer pour gagner ?**
> **Oui**, tant que le roi adverse est encore un **pion** (n'a pas encore de sujets), il peut être tué comme un pion normal et la partie est terminée.

**11.7 — Y a-t-il un écran de victoire/défaite ?**
> Pas de réponse explicite. À déterminer.

---

## 12. Événements aléatoires et météo

### Météo

**12.1 à 12.9 — Toutes les questions sur la météo.**
> **Aucun événement météo pour le moment.** Pas du tout de météo, pas du tout d'événements aléatoires de météo. Ce sera implémenté **plus tard**.

### Cases piégées et événements

**12.10 à 12.15 — Toutes les questions sur les cases piégées et événements.**
> **Zéro événement, zéro cases piégées pour le moment.** Ces systèmes n'existent pas dans le jeu à ce stade. Ce sera implémenté **plus tard**.

---

## 13. Mobs neutres

**13.1 à 13.10 — Toutes les questions sur les mobs neutres.**
> **Aucun mob neutre** dans le jeu pour le moment. Ce sera implémenté **plus tard**.

---

## 14. Système de déité — Dieu, Diable et sacrifice

**14.1 à 14.13 — Toutes les questions sur le système de déité.**
> Le système de déité, Dieu, Diable et sacrifice **n'est pas du tout implémenté** pour le moment. Ça n'existe pas dans le jeu. Ce sera fait **plus tard**.

---

## 15. Intelligence artificielle (IA ennemie)

**15.1 — Quel niveau de complexité pour l'IA ?**
> L'IA doit être **extrêmement intelligente**, définie sur des **règles symboliques**. Il faut bien réfléchir à toutes les possibilités du jeu, à tous les mécanismes, pour proposer une IA **très performante**. Elle doit prendre en compte l'ensemble du gameplay : farm, construction, recrutement, attaque, mariage, arène, etc.

**15.2 — Y a-t-il plusieurs niveaux de difficulté sélectionnables ?**
> **Non**, pas de niveaux de difficulté dans l'interface du jeu. Par contre, au niveau **back-end**, il doit y avoir un **fichier de paramétrage** ou des **variables globales** qui permettent au développeur de tout régler très facilement au niveau de l'IA : son fonctionnement, les côtés aléatoires, etc. L'IA est globalement très intelligente, et ce qui permet de **baisser son intelligence** doit être **complètement paramétrable** par le développeur.

**15.3 — L'IA a-t-elle accès aux mêmes informations que le joueur ?**
> Pas de réponse explicite sur l'omniscience de l'IA. Il n'y a pas de brouillard de guerre dans le jeu (le joueur voit tout), donc l'IA voit probablement tout aussi.

**15.4 — L'IA gère-t-elle la construction, le farm, les upgrades, et les mariages ?**
> **Oui**, l'IA fait **exactement les mêmes choses** que le joueur : recruter, attaquer, créer des arènes, aller à l'église pour un mariage, etc. Tout s'articule autour du roi ennemi qui réfléchit à tout ce qu'il fait.

**15.5 — L'IA interagit-elle avec le système de sacrifice/déité ?**
> Non applicable (le système de déité n'est pas implémenté).

**15.6 — L'IA a-t-elle une personnalité ?**
> **Non**, pas de personnalité pour le moment. L'IA doit être la **plus performante possible**.

**15.7 — L'IA respecte-t-elle les mêmes contraintes que le joueur ?**
> **Oui, absolument.** L'IA a **exactement les mêmes pouvoirs, les mêmes fonctionnalités** que le joueur. Le jeu est codé de manière **neutre** : il y a deux royaumes qui peuvent faire exactement la même chose. Le joueur contrôle le royaume blanc, l'IA contrôle le royaume noir, mais les règles sont strictement identiques. Pas de triche, pas d'avantage.

---

## 16. Interface utilisateur (UI) et menus

### Menu principal

**16.1 — Quels sont les éléments du menu principal ?**
> - **Nouvelle partie** : ouvre un menu qui permet de nommer la partie et de la lancer. Comme sur la plupart des jeux (type Subnautica), mais **très basique**.
> - **Continuer** : reprendre une partie sauvegardée.
>
> **Pas inclus pour le moment :** pas de menu Options, pas de menu Crédits.

**16.2 — Y a-t-il un menu de configuration de partie avant de lancer un jeu ?**
> Oui, un **menu très basique** : on peut **nommer** la partie et la **lancer**. Pas de configuration de taille de carte, difficulté, ou seed (au moins pour l'utilisateur).

**16.3 — Y a-t-il un menu Options ?**
> **Non, pas pour le moment.** Ce sera fait plus tard.

### HUD en jeu

**16.4 — Quels éléments sont affichés dans le HUD pendant la partie ?**
> - **Numéro du tour** actuel.
> - **Nombre d'écus** du royaume.
> - **Indicateur de qui joue** (quel royaume est en train de jouer).
> - **Bouton "Jouer"** ("Jouer mon tour") — et non pas "Suivant".
> - **Bouton "Reset"** à côté du bouton "Jouer", qui permet de **réinitialiser** toutes les actions du tour en cours si on ne l'a pas encore confirmé (revenir avant les modifications).

**16.5 — Y a-t-il une minimap ?**
> **Non, pas de minimap.** L'utilisateur peut déjà zoomer, dézoomer et se déplacer librement sur la carte, donc une minimap ne sert à rien.

**16.6 — Comment les informations d'une pièce sont-elles affichées quand on la sélectionne ?**
> Via un **panneau latéral**. De manière générale, quand on sélectionne n'importe quel élément (troupe, bâtiment), c'est toujours le **même type de panneau latéral** qui s'affiche.

**16.7 — Quelles informations apparaissent sur le panneau de la pièce sélectionnée ?**
> - **XP** actuelle.
> - **Possibilités d'upgrade** (si le seuil d'XP est atteint).
> - **Portée** de la pièce.
> - Toutes les **autres informations utiles** relatives à la pièce (type, etc.). Le détail exact est laissé au développeur.

**16.8 — Les cases accessibles par une pièce sélectionnée sont-elles surlignées sur la carte ?**
> **Oui.** Les cases accessibles sont recouvertes d'une **couleur verte transparente** (overlay vert semi-transparent).

**16.9 — L'interface de la caserne est-elle un pop-up, un panneau latéral, un menu dédié ?**
> C'est un **panneau latéral** (le même type de panneau que pour les pièces). La caserne affiche :
> - Le choix de la **troupe à produire**.
> - Le **statut** de la caserne : disponible pour la production ou en cours de fabrication.
> - Si en cours de fabrication : le **nombre de tours restants** pour que la troupe soit produite.
> - Un **bouton** pour lancer la production.

**16.10 — L'outil de construction du roi : comment est-il présenté ?**
> En **bas à gauche** du HUD, il y a **deux outils** disponibles :
>
> 1. **Outil souris classique** : c'est le mode par défaut. Permet de déplacer la carte, zoomer, dézoomer, sélectionner les objets.
> 2. **Outil construction** (icône de **marteau**) : quand on le sélectionne, un **panneau latéral** s'ouvre avec la **liste des bâtiments/objets disponibles** à la construction. On en sélectionne un, et ensuite, sur la carte, avec cet outil, on peut **placer** l'élément. Avant le clic gauche de placement, on voit en **overlay transparent** sur la carte un **aperçu** de l'emplacement du bâtiment (preview de l'endroit où il va se poser).

**16.11 — Y a-t-il un indicateur visuel pour les zones de farm occupées/disputées/libres ?**
> **Oui.** Un indicateur **flottant au-dessus** des zones de farm (un peu à la manière des Sims), visuel, avec **trois icônes** possibles :
>
> | Icône | Signification | Fichier texture |
> |---|---|---|
> | **Bouclier noir** | Zone occupée par le royaume noir | `shield_black.png` |
> | **Bouclier blanc** | Zone occupée par le royaume blanc | `shield_white.png` |
> | **Épées croisées** | Zone disputée (les deux royaumes y sont présents) | `crossed_swords.png` |
>
> Ce même système d'icônes s'applique **aussi à l'église** et aux **autres bâtiments publics**.

**16.12 — Y a-t-il un journal d'événements ?**
> **Oui.** C'est un **troisième outil** dans la barre d'outils (en bas à gauche), aux côtés de l'outil souris et de l'outil construction :
>
> 3. **Outil journal** (icône de **journal/livre**) : ouvre un **panneau latéral** qui fonctionne comme un **terminal** listant tous les événements qui se sont produits.
>
> Le journal contient **deux sections** :
> - **Événements alliés** : tout ce qui s'est passé du côté du joueur.
> - **Événements ennemis** : tout ce qui s'est passé du côté de l'IA.
>
> Le journal couvre **toute la partie** (pas seulement le tour précédent), avec l'historique complet.

**16.13 — Comment sont affichés les événements météo à l'écran ?**
> Non applicable (pas de météo dans le scope actuel).

**16.14 — Y a-t-il un indicateur d'affiliation Dieu/Diable visible dans le HUD ?**
> Non applicable (pas de système de déité dans le scope actuel).

**16.15 — Les dialogues des événements sur cases : comment sont-ils affichés ?**
> Non applicable (pas d'événements sur cases dans le scope actuel).

**16.16 — Y a-t-il un menu pause en jeu ?**
> **Non**, pas de menu pause dédié. Pour mettre en pause, le joueur appuie sur la touche **Échap (Escape)**.

### Interactions joueur

**16.17 — Le jeu se joue-t-il à la souris seule, au clavier seul, ou souris + clavier ?**
> **À la souris seule** pour toutes les interactions de jeu :
> - **Déplacement de la caméra** : clic molette (middle click) + drag, ou drag sur les espaces vides (zones sans sélection possible).
> - **Zoom/dézoom** : molette de la souris.
> - **Sélection** : clic gauche.
> - **Déplacement de pièces** : clic gauche sur la pièce → clic gauche sur la destination.
> - Tout le reste se fait à la souris.
>
> Seul raccourci clavier mentionné : **Espace** pour centrer la caméra sur le roi.

**16.18 — Quels sont les raccourcis clavier prévus ?**
> - **Espace** : centrer la caméra sur le roi.
> - **Échap** : mettre en pause.
> - Pas d'autres raccourcis mentionnés pour le moment.

**16.19 — Comment sélectionne-t-on une pièce ?**
> **Clic gauche** sur la pièce.

**16.20 — Comment ordonne-t-on un déplacement ?**
> On sélectionne une pièce (clic gauche) → les cases accessibles s'affichent en vert transparent → on clique gauche sur la **destination**. Le déplacement apparaît alors en **transparence** (flèche + fantôme de la pièce) jusqu'à la **consommation du tour**.

**16.21 — Comment ordonne-t-on une attaque ?**
> Comme aux échecs classiques : le **déplacement** sur une pièce ennemie ou un **bloc** (mur, case de caserne) est automatiquement considéré comme une **attaque**. Il n'y a pas d'action d'attaque séparée.

**16.22 — Y a-t-il un système de confirmation avant action ?**
> **Oui**, de façon implicite : les actions ne sont **réellement exécutées** qu'à la **consommation du tour** (bouton "Jouer"). Avant ça :
> - Les déplacements sont affichés en **transparence** (flèche + fantôme).
> - Le joueur peut voir ce qui **va se passer** avant de confirmer.
> - Il existe un bouton **"Reset"** pour tout réinitialiser.

**16.23 — Peut-on annuler une action dans le même tour avant de passer au suivant ?**
> On **ne peut pas annuler une action individuellement**. Par contre, il y a un bouton **"Reset"** à côté du bouton "Jouer" qui permet de **réinitialiser le tour entier** (revenir à l'état d'avant les modifications) tant que le tour n'a pas été confirmé.

---

## 17. Audio — Musique et effets sonores

**17.1 à 17.5 — Toutes les questions sur l'audio.**
> **Il n'y a absolument rien** en audio pour le moment. Pas de musique, pas d'effets sonores, pas de voix. Ne rien configurer. Ce sera fait plus tard.

---

## 18. Caméra et navigation sur la carte

**18.1 — La caméra est-elle en vue du dessus ou en vue isométrique ?**
> **100% vue du dessus** (top-down). Pas de vue isométrique. C'est **100% de la 2D** et **100% de la vue du dessus**.

**18.2 — Le joueur peut-il zoomer/dézoomer ?**
> **Oui**, zoom et dézoom avec la **molette de la souris**. Pas de limites de zoom spécifiées.

**18.3 — Le joueur peut-il déplacer la caméra librement ?**
> **Oui**, déplacement libre de la caméra. Via le **clic molette + drag** ou en **draguant** sur des zones sans sélection possible.

**18.4 — Y a-t-il un raccourci pour centrer la caméra sur le roi ?**
> **Oui**, la touche **Espace** centre la caméra sur le roi.

**18.5 — Le joueur voit-il toute la carte ou y a-t-il un brouillard de guerre ?**
> **Pas de brouillard de guerre.** Le joueur peut **tout voir** en zoomant et dézoomant. Toute la carte est visible en permanence.

**18.6 — Si brouillard de guerre : les zones explorées restent-elles visibles ?**
> Non applicable (pas de brouillard de guerre).

---

## 19. Sauvegarde et persistance

**19.1 — Le joueur peut-il sauvegarder sa partie en cours ?**
> **Oui.** Il y a un système de sauvegarde. Quand le joueur est sur une partie et veut la quitter, il peut :
> - **Quitter sans sauvegarder.**
> - **Quitter en sauvegardant.**
> - **Sauvegarder** (sans quitter).
>
> Les sauvegardes sont sur du **stockage permanent**.

**19.2 — Le système de sauvegarde est-il manuel ou automatique ?**
> **Manuel.** C'est le joueur qui décide quand sauvegarder.

**19.3 — Y a-t-il plusieurs emplacements de sauvegarde ?**
> **Oui**, on peut avoir **plusieurs parties** avec chacune ses **saves**. On fait des sauvegardes pendant notre game.

**19.4 — Le format de sauvegarde : quel type de fichier ?**
> Pas de réponse explicite. À déterminer par le développeur.

**19.5 — Y a-t-il un système de replay pour revoir une partie terminée ?**
> **Non, pas pour le moment.** Mais il faut **poser les bases** pour que le replay ne soit pas compliqué à implémenter si on veut l'ajouter plus tard.

---

## 20. Équilibrage et game design chiffré

### Tableau récapitulatif des paramètres définis

| #     | Paramètre | Valeur |
|-------|-----------|--------|
| 20.1  | Forme et taille de la carte | **Cercle** de rayon **512 cases** (8 × 64), paramétrable en back-end |
| 20.2  | Écus de départ | **0** |
| 20.3  | Écus rapportés par une mine (par tour par case) | **10 écus/tour/case** |
| 20.4  | Écus rapportés par un champ (par tour par case) | **5 écus/tour/case** |
| 20.5  | Coût pion (caserne) | Non spécifié |
| 20.6  | Coût cavalier (caserne) | Non spécifié |
| 20.7  | Coût fou (caserne) | Non spécifié |
| 20.8  | Coût tour (caserne) | Non spécifié |
| 20.9  | Tours de fabrication — pion (niv. 0) | **2 tours** |
| 20.10 | Tours de fabrication — cavalier (niv. 1) | **4 tours** |
| 20.11 | Tours de fabrication — fou (niv. 1) | **4 tours** |
| 20.12 | Tours de fabrication — tour (niv. 2) | **6 tours** |
| 20.13 | Coût construction caserne | **50 écus** |
| 20.14 | Coût construction mur de bois | **20 écus** |
| 20.15 | Coût construction mur de pierre | **40 écus** |
| 20.16 | PV mur de bois | **1 PV** |
| 20.17 | PV mur de pierre | **3 PV** |
| 20.18 | PV d'une case de caserne | **1 PV** |
| 20.19 | Portée maximale globale (toutes pièces) | **8 cases** (variable back-end) |
| 20.20 | XP seuil pion → cavalier/fou | **100 XP** |
| 20.21 | XP seuil cavalier/fou → tour | **300 XP** |
| 20.22 | XP gagnée par kill pion | **20 XP** |
| 20.23 | XP gagnée par kill cavalier/fou | **50 XP** |
| 20.24 | XP gagnée par kill tour | **100 XP** |
| 20.25 | XP gagnée par kill reine | **300 XP** |
| 20.26 | XP gagnée par destruction de bloc | **10 XP** |
| 20.27 | XP par tour à l'arène | **10 XP/tour/pièce** |
| 20.28 | Coût construction arène | Non spécifié |
| 20.29 | Points d'action par tour (PA) | **Non implémenté** pour le moment |
| 20.30 | Rayon de construction du roi | **Cases adjacentes directes** |
| 20.31 | Nombre de mines sur la carte | **2** |
| 20.32 | Nombre de champs sur la carte | **3** |
| 20.33 | Taille de l'église | **4 × 3** blocs (confirmé) |
| 20.34 | Taille de l'arène | ~9 blocs (mentionné dans le brainstorming, à préciser) |
| 20.35 | Taille de la caserne | **4 × 2** blocs |
| 20.36 | Taille d'une mine | **6 × 6** blocs |
| 20.37 | Taille d'un champ | **4 × 3** blocs |
| 20.38 | Distance minimum entre bâtiments publics | **10 blocs** |
| 20.39 | Zone d'apparition joueur | **25% gauche** de la carte (aléatoire) |
| 20.40 | Zone d'apparition IA | **25% droite** de la carte (aléatoire) |

### Paramètres restant à définir

| # | Paramètre | Statut |
|---|-----------|--------|
| — | Coût recrutement par type de pièce (caserne) | **À définir** |
| — | Coût en écus d'une upgrade (par niveau) | **À définir** |
| — | Coût construction arène | **À définir** |
| — | Dimensions exactes de l'arène | **À préciser** |
| — | Limites de zoom caméra (min/max) | **À définir** |

---

## 21. Lore et univers narratif

**21.1 — Le jeu a-t-il un contexte narratif ?**
> **Non**, le jeu n'a **pas encore de contexte narratif**, pas de backstory, pas d'histoire. Ce sera fait plus tard.

**21.2 — Les deux royaumes ont-ils des noms ?**
> **Non**, pas encore de noms ni d'identités visuelles définies pour les royaumes, au-delà de "blanc" et "noir".

**21.3 — Le roi a-t-il un nom/titre ?**
> **Non**, pas encore.

**21.4 — Les dieux/le diable ont-ils des noms, des apparences ?**
> Non applicable (le système de déité n'est pas implémenté).

**21.5 — Le ton du jeu est-il sérieux ou humoristique ?**
> **Sérieux.**

**21.6 — Y a-t-il des textes descriptifs ? Dans quelle langue ?**
> Pas de réponse explicite. Le développeur est libre d'ajouter un peu de **lore** (noms, titres pour les royaumes), mais ce n'est **pas le cœur du projet** pour le moment.

**21.7 — Les événements sur cases ont-ils des textes narratifs ?**
> Non applicable (pas d'événements sur cases dans le scope actuel).

**21.8 — Y a-t-il un tutoriel ?**
> Pas de réponse explicite. Non mentionné.

---

## Annexe — Corrections et évolutions par rapport au brainstorming original

Ce tableau résume les **changements majeurs** apportés par rapport au document de brainstorming initial :

| Élément | Brainstorming | Décision finale |
|---|---|---|
| **Forme de la carte** | Carrée (16×16 ou 64×64) | **Cercle** de rayon 512 cases |
| **Caserne de départ** | Présente par défaut | **Supprimée** — le joueur démarre sans caserne |
| **Arènes** | Bâtiments publics (2, prédéfinis) | **Bâtiments constructibles** par le roi, sans limitation |
| **Création de la reine** | Roi + Tour dans l'église → Tour devient reine (mariage) | **Roi + Fou + Pion dans l'église**, roi et pion adjacents → **Pion** devient reine |
| **Système d'XP** | Pourcentage 0-100%, XP max par pièce, XP repart à 0 après upgrade | **Seuils sans plafond** (100 et 300 XP), XP accumulée indéfiniment |
| **Mouvement du pion** | Règles classiques (avant + diagonale pour capturer) | Toutes directions **sauf diagonale** (haut/bas/gauche/droite) |
| **Portée des pièces** | Variable par niveau de pièce (ex : fou niv.1 = 4, niv.2 = 6, niv.3 = 8) | **Unique variable globale de 8 cases** max pour toutes les pièces |
| **Météo, événements, mobs, déités** | Prévus | Tous **reportés** à plus tard |
| **Nombre de mines/champs** | Non spécifié | **2 mines**, **3 champs** |
| **Hiérarchie de niveaux (fabrication)** | Niv. 0-4 | Fabrication uniquement **niv. 0, 1, 2** (pion, cavalier/fou, tour) |

---

*Ce document sera mis à jour à chaque nouvelle décision prise.*
