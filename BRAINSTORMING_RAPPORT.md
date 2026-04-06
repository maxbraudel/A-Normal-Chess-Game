# A Normal Chess Game — Rapport de Brainstorming

---

## Table des matières

- [A Normal Chess Game — Rapport de Brainstorming](#a-normal-chess-game--rapport-de-brainstorming)
  - [Table des matières](#table-des-matières)
  - [1. Vue d'ensemble et concept général](#1-vue-densemble-et-concept-général)
  - [2. Inspirations](#2-inspirations)
  - [3. La carte](#3-la-carte)
    - [3.1 Dimensions et génération](#31-dimensions-et-génération)
    - [3.2 Types de terrain](#32-types-de-terrain)
    - [3.3 Bâtiments publics](#33-bâtiments-publics)
    - [3.4 Bâtiments privés](#34-bâtiments-privés)
  - [4. Les royaumes et la configuration initiale](#4-les-royaumes-et-la-configuration-initiale)
    - [4.1 Nombre de royaumes](#41-nombre-de-royaumes)
    - [4.2 Configuration de départ](#42-configuration-de-départ)
    - [4.3 Condition de victoire](#43-condition-de-victoire)
  - [5. Les ressources](#5-les-ressources)
    - [5.1 Les écus](#51-les-écus)
    - [5.2 Zones de farm](#52-zones-de-farm)
    - [5.3 Règles d'occupation des zones de farm](#53-règles-doccupation-des-zones-de-farm)
  - [6. Les pièces d'échecs](#6-les-pièces-déchecs)
    - [6.1 Hiérarchie et niveaux](#61-hiérarchie-et-niveaux)
    - [6.2 Points de vie](#62-points-de-vie)
    - [6.3 Portée des pièces](#63-portée-des-pièces)
    - [6.4 Système d'expérience et d'upgrade](#64-système-dexpérience-et-dupgrade)
    - [6.5 La reine — cas particulier](#65-la-reine--cas-particulier)
    - [6.6 Le roi — cas particulier](#66-le-roi--cas-particulier)
  - [7. Les bâtiments](#7-les-bâtiments)
    - [7.1 La caserne](#71-la-caserne)
    - [7.2 Les murs](#72-les-murs)
    - [7.3 L'église](#73-léglise)
    - [7.4 L'arène](#74-larène)
    - [7.5 Mines et champs](#75-mines-et-champs)
    - [7.6 Destruction des bâtiments](#76-destruction-des-bâtiments)
  - [8. Système de gameplay — Tour par tour](#8-système-de-gameplay--tour-par-tour)
    - [8.1 Structure d'un tour](#81-structure-dun-tour)
    - [8.2 Actions disponibles par tour](#82-actions-disponibles-par-tour)
    - [8.3 Système de formations](#83-système-de-formations)
    - [8.4 Système de points d'action (proposition)](#84-système-de-points-daction-proposition)
  - [9. Événements aléatoires](#9-événements-aléatoires)
    - [9.1 Météo](#91-météo)
    - [9.2 Cases piégées et événements sur cases](#92-cases-piégées-et-événements-sur-cases)
    - [9.3 Mobs neutres](#93-mobs-neutres)
  - [10. Système de déité](#10-système-de-déité)
    - [10.1 Concept général](#101-concept-général)
    - [10.2 Système de sacrifice](#102-système-de-sacrifice)
    - [10.3 Dieu et Diable — affiliation dynamique](#103-dieu-et-diable--affiliation-dynamique)
  - [11. Idées explorées et questions ouvertes](#11-idées-explorées-et-questions-ouvertes)
    - [11.1 Système de chunks (idée explorée)](#111-système-de-chunks-idée-explorée)
    - [11.2 Multiples factions (idée explorée)](#112-multiples-factions-idée-explorée)
    - [11.3 Victoire territoriale (idée explorée)](#113-victoire-territoriale-idée-explorée)
    - [11.4 Questions ouvertes identifiées en séance](#114-questions-ouvertes-identifiées-en-séance)

---

## 1. Vue d'ensemble et concept général

Le jeu s'appelle **A Normal Chess Game**. C'est un jeu d'échecs hybride qui incorpore des éléments de jeu de stratégie en temps réel (RTS) et de RPG. L'idée centrale est de transformer le jeu d'échecs classique en une expérience plus globale et plus profonde, en conservant le tour par tour et les règles de déplacement des pièces d'échecs, mais en les intégrant dans une grande carte avec un système de construction, de recrutement, de ressources, et d'événements aléatoires.

---

## 2. Inspirations

Les références citées lors du brainstorming sont les suivantes :

- **Age of Empire** : RTS avec construction de bâtiments, recrutement d'unités, gestion de ressources, agrandissement d'une armée.
- **War Selection** : jeu dans l'esprit d'Age of Empire, cité en parallèle.
- **Bannerlord** : cité pour son approche de conquête territoriale sans fin définie.
- **Baldur's Gate** : cité pour son système de points d'action dans les RPG classiques.

---

## 3. La carte

### 3.1 Dimensions et génération

- La carte est **grande**, bien plus grande qu'un échiquier classique (8×8).
- Les dimensions évoquées : **16×16** (première mention), puis **64×64** (mention plus précise lors d'une autre session).
- La carte est **générée procéduralement**.
- Les ressources (mines, champs) sont également générées **aléatoirement** sur la carte.

### 3.2 Types de terrain

Plusieurs types de blocs/cases sont prévus sur la carte :

- **Herbe**
- **Terre**
- **Eau** (pour certaines étendues)
- **Bâtiments** (occupant plusieurs cases, voir section dédiée)

### 3.3 Bâtiments publics

Les bâtiments publics sont des structures présentes sur la carte qui **ne peuvent pas être détruites** par l'un ou l'autre des royaumes. Ils ne peuvent pas être possédés, mais ils peuvent être **occupés**. Les bâtiments publics identifiés sont :

- **L'église** (voir section 7.3)
- **L'arène** (voir section 7.4)
- **Les mines** (voir section 7.5)
- **Les champs** (voir section 7.5)

**Règle commune à tous les bâtiments publics :** Un royaume ne peut pas utiliser ou exploiter un bâtiment public si une pièce ennemie s'y trouve. Si les deux royaumes ont des pièces à l'intérieur d'un même bâtiment public, celui-ci ne fonctionne pour aucun des deux.

### 3.4 Bâtiments privés

Les bâtiments privés sont construits par les joueurs. Ils peuvent être détruits par les pièces ennemies. Les bâtiments privés identifiés sont :

- **La caserne** (voir section 7.1)
- **Les murs de bois** (voir section 7.2)
- **Les murs de pierre** (voir section 7.2)

---

## 4. Les royaumes et la configuration initiale

### 4.1 Nombre de royaumes

La décision prise en séance est de **limiter le jeu à deux royaumes** :

- Le **joueur** contrôle un royaume.
- Une **IA** contrôle le royaume ennemi.

Il n'y a pas de système d'alliance, de diplomatie, ni de royaumes multiples dans le scope actuel.

### 4.2 Configuration de départ

Pour chaque camp :

- Par défaut, **une caserne est déjà construite** au démarrage.
- Le joueur commence avec **un seul pion** (pas encore un roi) et **zéro ressource**.
- Ce pion n'est pas encore un roi : il **devient roi uniquement lorsqu'il possède au moins un sujet** (une pièce fidèle).
- Pour obtenir des ressources, le pion doit se rendre dans une **zone de farm** (mine ou champ) et récolter des écus.
- Avec ces écus, il peut choisir les pièces à générer via la caserne.
- Une fois qu'il a au moins une pièce à ses côtés, il devient **roi**.

### 4.3 Condition de victoire

La fin du jeu survient lorsqu'un royaume **met le roi de l'autre royaume en échec et mat**.

---

## 5. Les ressources

### 5.1 Les écus

La seule ressource du jeu est les **écus** (pièces d'or). Il n'y a pas de ressources multiples (bois, nourriture, etc.) : tout est simplifié en une seule monnaie. Les écus servent à :

- Recruter des pièces via les casernes.
- Construire des bâtiments (murs, casernes supplémentaires, etc.).

### 5.2 Zones de farm

Les zones de farm sont des **mines** et des **champs** générés procéduralement sur la carte. Chacune de ces zones correspond à **un certain nombre de cases**.

- **Règle d'occupation :** Lorsqu'un royaume place une ou plusieurs pièces sur les cases d'une zone de farm, la zone rapporte un **nombre fixe d'écus par tour** à ce royaume.
- Le nombre d'écus rapportés est **défini par la zone de farm elle-même**, pas par le nombre de pièces présentes ni par leur type.
- Il a été évoqué que chaque case de la zone rapporte un certain nombre d'écus par tour.

**Discussion sur la proportionnalité :** Il y a eu un débat sur le fait que les ressources soient ou non proportionnelles au nombre de pièces présentes dans la zone. La conclusion est que **la zone rapporte un montant fixe par tour**, peu importe le nombre ou le type de pièces qui l'occupent. Ce qui compte, c'est qu'**un seul royaume soit présent** sur la zone.

### 5.3 Règles d'occupation des zones de farm

- Si un seul royaume a des pièces sur la zone → la zone rapporte ses écus à ce royaume.
- Si les **deux royaumes** ont chacun une ou plusieurs pièces sur la même zone → la zone **ne rapporte rien à personne**.
- Cela crée un enjeu stratégique : il faut occuper la zone, défendre ses pièces sur place, et empêcher l'ennemi d'y poser les siennes.

---

## 6. Les pièces d'échecs

### 6.1 Hiérarchie et niveaux

Les pièces sont organisées selon une hiérarchie de niveaux :

| Niveau | Pièce(s)            |
|--------|---------------------|
| 0      | Pion                |
| 1      | Cavalier / Fou (même niveau) |
| 2      | Tour                |
| 3      | Reine               |
| 4      | Roi                 |

### 6.2 Points de vie

- Toutes les pièces ont **1 point de vie** (principe des échecs : une pièce est mangée en un coup).
- Les **murs** ont un système de points de vie différent (voir section 7.2).
- Une pièce est détruite (mangée) lorsqu'une pièce ennemie se déplace sur sa case, selon les règles de déplacement des échecs.

### 6.3 Portée des pièces

- Par défaut, les pièces n'ont **pas une portée infinie** sur la grande carte.
- Exemple donné pour les fous : un fou de **niveau 1** a une portée de **4 cases**, niveau 2 : **6 cases**, niveau 3 : **8 cases**.
- Ce système de range s'applique également aux autres pièces (range minimum et range maximum upgradable).
- La portée des pièces peut être augmentée via le système de niveaux (voir section 6.4).

### 6.4 Système d'expérience et d'upgrade

Chaque pièce possède une **barre d'expérience** (exemple donné : pion à 0/500, tour à 0/1000).

**Sources d'expérience :**
- Manger une pièce ennemie.
- Détruire une structure ennemie.
- Être posée sur une case de l'**arène** (gain d'expérience passif par tour).

**Montée en niveau :**
- Quand une pièce atteint **100% de son expérience**, elle peut être **upgradée au niveau supérieur**.
- L'upgrade est un **choix du joueur** : lorsque la pièce est sélectionnée, une option d'upgrade apparaît.
- Après l'upgrade, la pièce est **remplacée par la pièce du niveau supérieur** et son expérience repart **à zéro**.

**Arborescences d'upgrade :**
- **Pion (niv. 0)** → au choix : **Cavalier** ou **Fou** (niv. 1)
- **Cavalier ou Fou (niv. 1)** → **Tour** (niv. 2)
- **Tour (niv. 2)** → **bloquée** (ne peut pas s'upgrader vers la Reine par ce système)
- La **Reine** est obtenue uniquement par le biais du mariage à l'église (voir section 6.5).

**Note :** Une idée d'upgrade aléatoire via l'arène avait été évoquée initialement (chance à chaque tour qu'une pièce dans l'arène monte de niveau), mais cette idée a été **abandonnée** au profit du système d'XP décrit ci-dessus qui est jugé meilleur.

### 6.5 La reine — cas particulier

- La reine **ne peut pas être recrutée dans une caserne** et **ne peut pas être obtenue via le système d'XP**.
- Elle est créée uniquement par un **mariage** à l'église :
  - Le **roi** et une **tour** doivent se trouver sur des cases de l'**église**.
  - Le joueur choisit de déclencher l'action "mariage".
  - Au tour suivant, la tour est **transformée en reine**.
- Il ne peut y avoir **qu'une seule reine** par partie (par royaume).

### 6.6 Le roi — cas particulier

- Le roi **ne possède pas de caserne initiale** ; c'est lui qui en construit.
- Il dispose d'un **outil de construction** qui lui permet de placer des bâtiments dans son rayon proche.
- Utiliser l'outil de construction **consomme un tour**.
- Le roi commence en tant que simple pion et **ne devient roi qu'au moment où il possède au moins un sujet**.

---

## 7. Les bâtiments

### 7.1 La caserne

- La caserne est un bâtiment privé, constructible par le roi.
- Elle est présente par défaut pour chaque camp au début de la partie.
- Elle occupe **plusieurs cases** sur la carte (exemple donné : un rectangle de 2×3).
- Elle permet de **recruter toutes les pièces d'échecs, à l'exception de la reine**.
- Plus la pièce recrutée est puissante, plus elle est **chère en écus** et plus elle met de **tours** à être générée :
  - Les **pions** sortent au **tour suivant** la commande.
  - Les pièces plus puissantes prennent **plusieurs tours** avant d'être générées.
- Lorsque l'on clique sur la caserne, une **interface permet de choisir les troupes à produire** et de les placer sur la carte dans le **rayon direct de la caserne**.

### 7.2 Les murs

Deux types de murs sont disponibles dans les options de construction :

| Type de mur   | Points de vie | Taille |
|---------------|----------------|--------|
| Mur de bois   | 1 PV           | 1 case |
| Mur de pierre | 3 PV           | 1 case |

- Un mur est **attaqué** selon les règles des échecs : une pièce attaque le mur en se déplaçant sur sa case.
- À chaque attaque, le mur perd **1 PV**. Quand il atteint **0 PV**, il est détruit.

### 7.3 L'église

- L'église est un **bâtiment public** (indestructible) placé **au centre de la carte**.
- Elle mesure **4 cases de large et 3 cases de hauteur**.
- Elle sert à **créer la reine** via le mariage (voir section 6.5).
- Sa position centrale crée une **zone de conflit stratégique** : les deux royaumes devront s'affronter pour en prendre le contrôle au moment où ils veulent créer leur reine.
- Elle ne peut pas être utilisée si les **deux royaumes y ont des pièces** simultanément.

### 7.4 L'arène

- L'arène est un **bâtiment public** (indestructible).
- Il y a **deux arènes** sur la carte.
- Chaque arène correspond à **environ 9 blocs de large**.
- Lorsqu'une pièce est posée sur une case de l'arène, elle **gagne de l'expérience** à chaque tour de manière passive.
- Elle ne peut pas être utilisée si les **deux royaumes y ont des pièces** simultanément.

### 7.5 Mines et champs

- Les mines et les champs sont des **bâtiments publics** (indestructibles), générés procéduralement.
- Chaque zone de farm correspond à **un certain nombre de cases**.
- Elles rapportent des **écus par case par tour** au royaume qui les occupe exclusivement.
- Elles ne fonctionnent pas si les deux royaumes y sont présents simultanément (voir section 5.3).

### 7.6 Destruction des bâtiments

**Règle générale :**
- Les **bâtiments publics** (église, arène, mines, champs) sont **indestructibles**.
- Les **bâtiments privés** (casernes, murs) peuvent être détruits par l'ennemi.

**Mécanisme de destruction :**
- Un bâtiment occupe **plusieurs cases** sur la carte.
- Pour detruire un bâtiment, il faut **détruire l'ensemble de ses cases** (via le principe d'attaque des échecs : une pièce ennemie se place sur la case à détruire).
- Chaque case du bâtiment a ses **propres points de vie**.
- Lorsqu'une case est détruite, elle est **grisée** visuellement.
- Lorsque **toutes les cases** du bâtiment ont été détruites, le **bâtiment est entièrement détruit**.

---

## 8. Système de gameplay — Tour par tour

### 8.1 Structure d'un tour

- Le jeu fonctionne en **tour par tour strict** : les deux royaumes ne peuvent **pas jouer en même temps**.
- **Un seul royaume joue par tour**, en alternance.
- Pendant son tour, le royaume effectue toutes ses actions.
- Lorsqu'il a terminé, il appuie sur un **bouton "Suivant"** (ou équivalent) pour consommer le tour et passer la main à l'adversaire.

### 8.2 Actions disponibles par tour

Pendant un tour, un royaume peut effectuer les actions suivantes :

1. **Lancer des fabrications** dans une ou plusieurs casernes (choisir la prochaine unité à produire).
2. **Faire un déplacement** : bouger une pièce ou une formation.
3. **Célébrer un mariage** : transformer une tour en reine à l'église, si les conditions sont réunies.
4. **Construire un bâtiment** : le roi utilise son outil de construction pour poser un bâtiment dans son rayon proche (consomme un tour).
5. **Passer le tour** en cliquant sur le bouton "Suivant".

### 8.3 Système de formations

- Des pièces qui sont **adjacentes les unes aux autres** sont considérées comme étant **en formation**.
- Une formation peut être **déplacée ensemble** sur la carte.
- Le déplacement d'une formation suit le **pattern de déplacement du type de pièce** qu'elle représente (par exemple, un groupe de fous se déplace selon le mouvement diagonal des fous).

### 8.4 Système de points d'action (proposition)

Une proposition a été faite d'introduire un **système de points d'action (PA)** par tour, à la manière des RPG classiques (Baldur's Gate, etc.) :

| Action                        | Coût en PA |
|-------------------------------|------------|
| Bouger un pion                | 1 PA       |
| Bouger un cavalier ou un fou  | 2 PA       |
| Bouger une tour               | 3 PA       |

**Idées associées aux PA :**
- Des **bâtiments** pourraient augmenter le nombre de PA disponibles au début d'un tour.
- Des **items à ramasser** sur la carte pourraient accorder des PA supplémentaires.
- Certaines **pièces en vie** pourraient générer des PA tant qu'elles sont vivantes, créant un enjeu à protéger des pièces sans valeur offensive directe.

---

## 9. Événements aléatoires

### 9.1 Météo

La météo correspond à des **événements aléatoires** pouvant affecter la carte et les pièces :

- **Brouillard** : cache des pièces, permet de faire des **embuscades**. Il a été suggéré que le brouillard pourrait faire jouer les pièces **toutes seules** (comportement autonome).
- **Pluie** : réduit la **portée des pièces**.

### 9.2 Cases piégées et événements sur cases

- Certaines cases peuvent être **piégées** (événements aléatoires).
- Des **événements peuvent apparaître sur des cases** : selon les pièces du joueur et selon comment il résout l'événement, des **dialogues** sont proposés, avec des choix qui affectent le déroulement de la partie.

### 9.3 Mobs neutres

- Des **pièces neutres** peuvent être générées aléatoirement sur la carte.
- Elles sont gérées par l'**IA** mais **n'appartiennent à aucun des deux royaumes**.
- Leur comportement est **défensif** (elles ne chassent pas, elles ne se déplacent pas pour aller chercher les joueurs).
- Elles ont une **portée de vision** : elles mangent les pièces ennemies qu'elles détectent dans leur champ de vision.

---

## 10. Système de déité

### 10.1 Concept général

- Un **dieu** (ou plusieurs divinités) **observe les actions des joueurs** et intervient dans la partie.
- Le dieu n'agit pas de **manière arbitraire** : ses interventions sont **basées sur ce qui s'est passé pendant la partie** (actions des joueurs).
- Exemple évoqué : si les cavaliers n'ont jamais (ou très rarement) mangé de pièces, le dieu va les **favoriser d'une manière ou d'une autre**.
- Ce dieu a été décrit avec humour comme un **"dieu woke"** qui tend à rééquilibrer les situations sous-exploitées.
- Le dieu peut **sanctionner** des comportements ou **accentuer** des choses qui ne se sont jamais produites.

### 10.2 Système de sacrifice

- Le concept d'un **quota de sacrifice** a été proposé, en cohérence avec l'univers médiéval du jeu.
- Le joueur peut **sacrifier des pions** pour **augmenter ses chances** sur tous les événements à probabilité du jeu.
- Exemple : sacrifier 3 pions augmente de X% le taux de chance sur les événements aléatoires (brouillard, événements sur cases, etc.).
- Le dieu est ainsi assimilé à une **représentation des probabilités** : on lui offre des sacrifices pour obtenir de meilleures chances.

### 10.3 Dieu et Diable — affiliation dynamique

Une idée complémentaire a été proposée : introduire à la fois un **Dieu** et un **Diable**, avec une **affiliation dynamique** :

- Le joueur **ne choisit pas son affiliation au départ**.
- Selon ses **actions en jeu**, il se rapproche progressivement du Dieu ou du Diable, qui lui accordent des faveurs en retour.
- Exemples d'affiliation évoqués :
  - Miser sur une **grande armée** → tend vers le **Diable**.
  - Miser sur les **ressources et la construction** → tend vers un **Dieu**.
- Il a été suggéré que ce système ne soit **pas binaire** : on pourrait avoir **plusieurs dieux** avec des caractéristiques différentes plutôt qu'un simple axe bien/mal.
- La notion de **bien et de mal** dans un jeu d'échecs a été identifiée comme une **question à travailler** : comment définir ce qui est "bien" ou "mal" dans ce contexte ? Une idée a été évoquée d'introduire des **pièces passives "citoyens"** qui seraient des cibles potentielles (mais non développée).

---

## 11. Idées explorées et questions ouvertes

### 11.1 Système de chunks (idée explorée)

Une idée a été proposée consistant à découper la grande carte en **chunks de 8×8** (correspondant à un échiquier classique) :

- Chaque chunk représenterait une **zone de la carte**.
- Pour **conquérir un chunk** occupé par une autre faction, une **partie d'échecs complète** se lancerait sur ce chunk.
- Les pièces disponibles pour cette partie pourraient varier selon ce que chaque joueur possède (pas forcément toutes les pièces).
- Dans ce concept, l'IA ne contrôlerait pas seulement une partie d'échecs mais une **stratégie diplomatique globale**.
- La carte procedural permettrait une vraie **IA diplomatique** de haut niveau.

Cette idée a été **partiellement absorbée** dans le concept de grande carte avec zones de farm et zones d'affrontement. Elle n'est pas retenue comme mécanisme central dans le scope actuel.

### 11.2 Multiples factions (idée explorée)

Une idée a été proposée d'avoir **plusieurs factions** sur la carte (plus de deux royaumes) avec des systèmes de :

- **Alliances** et de **non-alliances** entre factions.
- **Factions qui ne meurent jamais** : si un roi est vaincu, il devient **prisonnier** d'une autre nation. Des **événements aléatoires** (ou l'intervention du Dieu/Diable) lui permettraient de se **libérer** et de **refonder une armée**.
- **Villageois** : des pièces neutres qui spawnent sur la carte (sorties de maisons, apparitions aléatoires) et que les royaumes peuvent **recruter/embaucher** lorsqu'ils passent à côté.

Cette idée a été **mise de côté** pour le scope actuel : il est décidé de se limiter à **deux royaumes** (joueur vs IA) lors de l'implémentation.

### 11.3 Victoire territoriale (idée explorée)

Une idée de **fin de jeu territoriale** a été évoquée, inspirée de Bannerlord :

- Le jeu n'aurait **pas de fin définie** : l'objectif serait de **conquérir des zones/villages** et d'agrandir son empire.
- La fin serait déterminée par la **taille de l'empire** (nombre de cases contrôlées).
- Conquérir un village rapporterait un **nombre de cases à l'empire**.

Cette idée a été **abandonnée** au profit de la fin classique des échecs : **l'échec et mat du roi adverse**.

### 11.4 Questions ouvertes identifiées en séance

Les questions suivantes ont été explicitement identifiées comme devant être résolues mais n'ont pas encore eu de réponse définitive :

- **Représentation des bâtiments sur la carte :** un bâtiment = une case ou plusieurs cases ? (La réponse retenue : plusieurs cases, avec des blocs assemblés formant le bâtiment.)
- **Définition de "bien" et "mal" dans le contexte d'un jeu d'échecs**, pour le système d'affiliation Dieu/Diable.
- **Position de départ du joueur :** où exactement apparaît-il sur la carte ?
- **Détails exacts des ressources de départ** (nombre d'écus initial, etc.).

---

*Document généré à partir des sessions de brainstorming tenues autour du projet "A Normal Chess Game".*
