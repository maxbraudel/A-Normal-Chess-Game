# Coffre de loot progressif

## Resume de la feature

Un coffre de loot peut apparaitre sur la carte et offrir un bonus au royaume qui le recupere.

Contraintes fonctionnelles :

- un seul coffre maximum a la fois
- si un coffre est deja present, aucun nouveau coffre ne peut apparaitre
- plus le temps passe depuis le dernier coffre, plus l'apparition suivante devient probable
- deux apparitions sur deux tours consecutifs doivent etre quasiment impossibles, ou interdites explicitement
- le coffre est recupere lorsqu'une piece marche sur sa case
- le bonus peut etre de l'or, un point d'action ou un point de construction

## Fonctionnement propose

1. Tant qu'un coffre est actif sur la carte, le systeme d'apparition est bloque.
2. Lorsqu'un coffre est collecte, on lance un delai minimal incompressible de securite.
3. Une fois ce delai minimal ecoule, l'instant d'apparition suivant est gouverne par une loi de Weibull, ce qui fait monter la probabilite d'apparition avec le temps ecoule depuis le dernier coffre.
4. Quand un spawn est declenche, la case d'apparition est choisie parmi les cases valides via une loi discrete ponderee.
5. Au moment de l'ouverture, le type de recompense est choisi via une loi discrete ponderee dependante de la phase de jeu.

## Lois de probabilite retenues

### 1. Loi de Weibull pour le temps d'attente entre deux coffres

Variable proposee :

- W ~ Weibull(k_chest, lambda_chest)

Transformation pour l'implementation :

- delay_spawn = cooldown_min + ceil(W)

Interpretation :

- si k_chest > 1, le taux de risque augmente avec le temps
- c'est exactement la propriete recherchee ici : juste apres un coffre, la proba du suivant est faible, puis elle augmente progressivement

Parametres recommandes :

- cooldown_min entre 4 et 6 tours
- k_chest entre 1.8 et 2.5
- lambda_chest entre 6 et 12 tours selon le rythme voulu

Regle de parametrage conseillee :

- en debut de partie, prendre un lambda plus grand pour retarder les premiers coffres
- plus la partie avance, plus lambda peut diminuer legerement
- exemple de regle : lambda_chest(turn) = max(lambda_min, lambda_0 - a * turn)

Modification apportee a la loi pure :

- on ajoute un cooldown dur avant d'appliquer la loi
- cela interdit pratiquement les apparitions trop rapprochees
- si un coffre est deja present, la probabilite est forcee a 0

Pourquoi cette loi a ete choisie :

- une loi exponentielle ou geometrique serait memoryless, donc moins adaptee
- la Weibull permet naturellement une probabilite d'arrivee croissante avec le temps

### 2. Loi discrete generale via std::discrete_distribution pour la case d'apparition

On filtre d'abord les cases admissibles :

- pas d'eau
- pas de batiment occupant la case
- pas de piece presente sur la case
- pas trop pres d'un spawn initial si on veut eviter les cadeaux gratuits

Ensuite, on attribue un poids a chaque case candidate.

Exemple de poids pertinent :

- poids plus fort pour les zones contestables par les deux royaumes
- poids plus faible pour les zones trop proches d'un camp
- poids nul pour les cases non atteignables ou incoherentes

Exemple de schema :

- w(cell) = base + bonus_contestation + bonus_centralite - malus_proximite_spawn

Pourquoi cette loi a ete choisie :

- elle donne un controle precis sur la geographie du spawn
- elle reste simple a ajuster sans changer la structure du systeme

### 3. Loi discrete generale via std::discrete_distribution pour le type de recompense

Recompenses possibles :

- or
- point d'action
- point de construction

Exemple de poids par defaut :

- or : 0.55
- point d'action : 0.25
- point de construction : 0.20

Regle de parametrage conseillee :

- en debut de partie, favoriser l'or
- plus tard, rehausser un peu les points d'action et de construction

Pourquoi cette loi a ete choisie :

- elle permet un loot lisible, controlable et facilement equilibrable

## Modifications et garde-fous

- hard cap : un seul coffre actif
- cooldown dur apres collecte ou disparition
- possibilite d'imposer un tour minimal avant le premier coffre, par exemple turn >= 8
- si aucun point de spawn valide n'existe au tour prevu, on repousse l'apparition d'un tour

## Pourquoi cet ensemble est pertinent

- la Weibull gere bien la montee progressive de probabilite
- la discrete_distribution gere proprement la case et le bonus
- l'ensemble donne un systeme rare, lisible, reglable, et beaucoup plus naturel qu'un simple pile ou face a chaque tour

## Note d'implementation

- utiliser l'API C++ <random>
- ne pas utiliser rand() / srand()
