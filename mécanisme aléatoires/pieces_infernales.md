# Pieces infernales

## Resume de la feature

Une piece rouge speciale, hostile aux royaumes, apparait de temps en temps sur le bord de la carte.

Elle :

- spawn uniquement sur une cellule en bord de carte
- choisit un royaume cible une seule fois
- choisit ensuite une piece cible dans ce royaume, en favorisant les pieces de haut rang hors roi
- poursuit cette cible jusqu'a la detruire ou jusqu'a devoir retarget une autre piece du meme royaume
- une fois la cible detruite, retourne vers le bord le plus proche et disparait

Nom anglais recommande :

- Infernal Piece

## Fonctionnement propose

1. Chaque royaume accumule une dette infernale, ou blood debt, basee sur son comportement recent.
2. Tant qu'aucune piece infernale n'est active, le jeu teste un spawn a chaque tour.
3. L'apparition est gouvernee par une loi de Poisson rare, dont l'intensite depend de la dette infernale des royaumes.
4. Si un spawn a lieu, le royaume cible est choisi par une loi de Bernoulli biaisee par les dettes infernales respectives.
5. La categorie de piece ciblee dans ce royaume est ensuite tiree via une loi discrete ponderee.
6. L'infernal piece choisit ensuite une case de bord valide et apparait.
7. Si sa cible meurt avant le contact, elle choisit une nouvelle cible dans le meme royaume.
8. Si aucune cible atteignable n'existe, elle peut retenter sur une autre piece du meme royaume, puis disparaitre si tout est impossible.

## Lois de probabilite retenues

### 1. Loi de Poisson pour le declenchement de l'evenement

Variable proposee a chaque tour :

- N_t ~ Poisson(lambda_t)

Regle :

- si N_t >= 1, une piece infernale apparait
- sinon, rien ne se passe

Pourquoi cette loi est pertinente :

- elle modelise bien des evenements rares par unite de temps
- elle permet de rendre la feature tres occasionnelle tout en la reliant a un niveau de tension ou de violence dans la partie

Exemple de dette infernale :

- captures de pieces non-roi recentes
- destruction de cellules de murs ou de batiments
- domination militaire trop forte d'un royaume

Exemple d'intensite :

- lambda_t = lambda_base + a * bloodDebtWhite + b * bloodDebtBlack

Bornes conseillees :

- lambda_base tres faible, par exemple 0.01 a 0.03
- lambda_t capee par une borne haute, par exemple 0.25 ou 0.30, pour que l'evenement reste rare

Modification apportee a la loi pure :

- si une piece infernale existe deja, alors lambda_t est ignoree et le spawn est bloque
- un cooldown dur apres disparition peut etre ajoute

### 2. Loi de Bernoulli pour choisir le royaume cible

Comme il n'y a que deux royaumes, la Bernoulli est adaptee.

Variable :

- K ~ Bernoulli(p)

Interpretation :

- K = 1 : royaume blanc cible
- K = 0 : royaume noir cible

Parametrage conseille :

- p = bloodDebtWhite / (bloodDebtWhite + bloodDebtBlack)

Effet :

- plus un royaume accumule de dette infernale, plus il attire la piece infernale

Pourquoi cette loi a ete choisie :

- simple, parfaitement adaptee a un choix binaire
- facilement justifiable en game design

### 3. Loi discrete generale via std::discrete_distribution pour la categorie de piece ciblee

Categories admissibles, en excluant le roi :

- queen
- rook
- bishop
- knight
- pawn

Poids recommandes :

- queen : 0.38
- rook : 0.26
- bishop : 0.14
- knight : 0.14
- pawn : 0.08

Regle de repli :

- si une categorie tiree n'existe pas dans le royaume cible, on renormalise sur les categories encore presentes

Pourquoi cette loi a ete choisie :

- elle permet de viser prioritairement les elites sans interdire completement les pions
- elle colle bien a ton idee : haute priorite aux meilleurs rangs, mais possibilite de viser aussi des unites modestes

## Regles de parametrage proposees

### Dette infernale

Exemple de score :

- +5 pour une reine adverse detruite
- +3 pour une tour
- +2 pour un fou ou cavalier
- +1 pour un pion
- +0.5 par cellule de mur ou de batiment detruite

Avec oubli progressif :

- a chaque tour, multiplier la dette par 0.92 a 0.97

### Spawn sur le bord

Parmi les cellules de bord valides, on peut soit :

- tirer uniformement
- soit preferer les cellules dont le plus court chemin vers une cible potentielle existe deja

La probabilite n'est utile ici que pour choisir parmi les bords valides ; le coeur du comportement reste la pathfinding logique.

## Modifications et garde-fous

- un seul infernal piece a la fois
- le royaume cible ne change jamais apres le spawn
- seule la piece ciblee peut changer, a l'interieur de ce royaume
- le roi est exclu du support probabiliste
- si aucun chemin n'existe meme avec capture ou destruction de mur, la cible est changee
- si toutes les cibles sont impossibles, la piece infernale disparait

## Pourquoi cet ensemble est pertinent

- Poisson rend l'evenement rare et dramatique
- Bernoulli convient idealement au choix entre deux royaumes
- discrete_distribution donne un ciblage de rang flexible et tres lisible en equilibrage

## Note d'implementation

- utiliser l'API C++ <random>
- ne pas utiliser rand() / srand()
