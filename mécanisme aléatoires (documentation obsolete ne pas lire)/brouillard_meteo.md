# Brouillard meteorologique dynamique

## Resume de la feature

Un brouillard volumineux traverse occasionnellement la carte comme un front meteo.

Il :

- apparait hors de la zone visible puis entre progressivement dans la carte
- se deplace selon une direction choisie aleatoirement
- occupe une grande zone, environ 25% a 35% de la carte visible
- masque visuellement les pieces et les parties de batiments ennemis qui se trouvent dessous
- laisse visibles le terrain, les pieces alliees et les batiments allies
- disparait une fois sorti de la carte

## Fonctionnement propose

1. Tant qu'aucun brouillard n'est actif, on prepare le prochain front via une loi d'inter-arrivee.
2. Quand le front nait, on choisit sa direction, son point d'entree et sa duree.
3. La forme globale est construite comme une grande masse organique.
4. A l'interieur de cette masse, l'opacite et la teinte grise varient localement.
5. Le front avance a vitesse quasi constante jusqu'a sortir de la carte.

## Lois de probabilite retenues

### 1. Loi Gamma / Erlang pour le temps entre deux brouillards

Variable proposee :

- D_arrivee ~ Gamma(k_arrivee, theta_arrivee)

Puis :

- next_fog_delay = cooldown_min + ceil(D_arrivee)

Pourquoi cette loi est pertinente :

- elle est positive
- avec k_arrivee > 1, elle reduit fortement les re-apparitions trop immediates
- elle modele bien une accumulation progressive de conditions meteorologiques

Parametres recommandes :

- cooldown_min entre 6 et 10 tours
- k_arrivee entre 3 et 5
- theta_arrivee entre 2 et 4

Effet :

- les brouillards restent episodiques
- les doubles apparitions quasi consecutives deviennent tres rares

### 2. Loi Gamma / Erlang pour la duree du front

Variable :

- D_duree ~ Gamma(k_duree, theta_duree)

Interpretation :

- D_duree fixe pendant combien de tours le brouillard reste present et traverse la carte

Parametres recommandes :

- k_duree entre 2 et 4
- theta_duree entre 2 et 3

Effet :

- la plupart des brouillards ont une duree moyenne stable
- certains sont un peu plus longs, mais pas de maniere delirante

### 3. Loi par morceaux via std::piecewise_constant_distribution pour la direction

On decoupe l'espace des directions en huit secteurs :

- N, S, E, W, NE, NW, SE, SW

Puis on attribue des poids par secteur.

Exemple de poids :

- E, W, N, S : 1.0 chacun
- NE, NW, SE, SW : 0.65 chacun

Pourquoi cette loi a ete choisie :

- elle permet de rester creatif sans se limiter a une simple uniforme sur 8 cas
- elle permet de rendre les directions cardinales un peu plus probables que les diagonales
- elle reste lisible et facile a regler

### 4. Loi par morceaux via std::piecewise_linear_distribution pour le point d'entree sur le bord

Une fois le bord d'entree deduit de la direction, on echantillonne la position du centre du brouillard sur ce bord.

Idee :

- preferer les zones centrales du bord plutot que les coins extrêmes

Pourquoi cette loi a ete choisie :

- si le brouillard entre trop souvent par les coins, il traverse peu la carte
- une piecewise linear permet de faire culminer la densite vers le milieu du bord, tout en gardant la possibilite d'entrer ailleurs

Regle conseillee :

- densite maximale sur le tiers central du bord
- densite plus faible sur les coins

### 5. Loi log-normale pour l'opacite et l'epaisseur locale du brouillard

Pour chaque cellule de la masse de brouillard, on calcule un multiplicateur local de densite.

Variable :

- M ~ LogNormal(mu_log, sigma_log)

Puis :

- alpha_local = clamp(alpha_base * M, alpha_min, alpha_max)

Pourquoi cette loi est pertinente :

- elle produit une variable positive
- la plupart des cellules restent proches d'une opacite moyenne
- quelques poches deviennent plus denses, ce qui donne un aspect organique et nuageux

Parametres recommandes :

- alpha_base autour de 0.40
- alpha_min entre 0.22 et 0.28
- alpha_max entre 0.50 et 0.60
- mu_log proche de 0 pour garder une moyenne controlee
- sigma_log entre 0.20 et 0.35 pour une variation visible sans etre chaotique

## Forme organique du brouillard

La forme globale ne doit pas etre un rectangle plein.

Proposition :

- utiliser un masque procedural large
- puis moduler ses bords et ses poches internes avec le multiplicateur log-normal

La loi probabiliste ne remplace pas le masque procedural ; elle enrichit la densite visuelle a l'interieur de la forme.

## Modifications et garde-fous

- un seul brouillard actif a la fois
- pas de brouillard permanent : delai minimal entre deux fronts
- la taille du front doit etre clamp entre des bornes de couverture, par exemple 25% et 35% de la carte
- les regles de vision sont purement visuelles : aucun impact sur les deplacements, attaques ou economies
- l'overlay du brouillard doit etre rendu au-dessus du terrain, des batiments et des pieces

## Pourquoi cet ensemble est pertinent

- Gamma/Erlang gere bien l'arrivee et la duree des episodes meteorologiques
- piecewise constant gere proprement la direction de deplacement
- piecewise linear donne un point d'entree plus interessant visuellement
- log-normal donne un nuage plus vivant, avec des zones legeres et des poches plus opaques

## Note d'implementation

- utiliser l'API C++ <random>
- ne pas utiliser rand() / srand()
