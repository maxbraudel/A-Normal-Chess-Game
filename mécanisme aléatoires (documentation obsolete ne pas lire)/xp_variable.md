# XP variable autour d'une moyenne

## Resume de la feature

Les gains d'XP ne sont plus fixes.

Chaque source d'XP conserve une moyenne de reference, mais la valeur finale fluctue legerement autour de cette moyenne.

Objectif :

- garder le ressenti actuel de progression
- ajouter une petite incertitude
- eviter les ecarts trop violents

## Fonctionnement propose

Pour chaque source d'XP :

- on prend la valeur fixe actuelle comme moyenne mu
- on echantillonne une variable gaussienne autour de mu
- on tronque la valeur a un intervalle raisonnable
- on arrondit a l'entier le plus proche

Exemples de sources concernees :

- kill de pion
- kill de cavalier
- kill de fou
- kill de tour
- kill de reine
- destruction de bloc
- XP donnee par l'arene a la fin du tour

## Loi de probabilite retenue

### Loi normale / gaussienne

Variable :

- X_raw ~ Normal(mu, sigma)

Puis transformation pour le gameplay :

- X = round(clamp(X_raw, mu - delta, mu + delta))

avec :

- delta = 2 * sigma, ou une borne fixe similaire

Pourquoi cette loi a ete choisie :

- elle concentre la masse autour de la moyenne
- elle rend les valeurs centrales tres probables
- elle autorise un peu plus ou un peu moins, ce qui colle exactement a ton besoin

## Regles de parametrage conseillees

### Moyenne

- mu = valeur fixe actuelle du systeme

Exemples :

- si l'arene donne 10 XP actuellement, alors mu = 10
- si tuer une tour donne 100 XP actuellement, alors mu = 100

### Ecart-type

Regle simple et robuste :

- sigma = max(1, c * mu)

avec :

- c entre 0.10 et 0.20 selon la variance voulue

Regle plus fine recommandee :

- petites sources d'XP : sigma = max(1, 0.18 * mu)
- grosses sources d'XP : sigma = max(1, 0.10 * mu)

Cela evite que les grosses recompenses deviennent trop instables.

### Troncature

Regle conseillee :

- X final dans [mu - 2 sigma, mu + 2 sigma]
- borne basse finale toujours >= 0

Cette troncature est importante pour eviter :

- les valeurs negatives
- les swings trop visibles ou absurdes

## Modifications apportees a la loi pure

- discretisation par arrondi, car l'XP finale doit rester entiere
- troncature pour garder le systeme lisible et equilibrable
- possibilite de minorer certaines sources a 1 XP minimum si necessaire

## Pourquoi cette loi est la bonne ici

- la gaussienne est naturellement centree sur une moyenne
- elle produit surtout des valeurs proches de l'attendu
- elle est intuitive a expliquer au joueur et simple a calibrer pour toi

## Exemple concret

Pour une arene a 10 XP de moyenne :

- mu = 10
- sigma = 1.5
- troncature sur [7, 13]

Alors :

- 10 sort tres souvent
- 9 et 11 sortent souvent
- 8 ou 12 restent possibles
- 7 et 13 sont rares

## Note d'implementation

- utiliser std::normal_distribution
- ne pas utiliser rand() / srand()
