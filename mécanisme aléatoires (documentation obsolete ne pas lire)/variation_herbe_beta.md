# Variation d'herbe plus sombre

## Fonctionnalite

Certaines cases d'herbe du terrain recoivent une luminosite legerement plus faible que les autres pour casser l'uniformite visuelle de la carte.

Le mecanisme est :

- seed-driven, donc reproductible pour une meme worldSeed
- applique uniquement aux cellules de type Grass
- sans effet sur la jouabilite, seulement sur le rendu

## Loi de probabilite utilisee

La loi utilisee est une loi Beta, echantillonnee de maniere deterministe a partir de la worldSeed et de la position de la cellule.

Implementation pratique :

- un echantillon Beta est obtenu via le rapport de deux variables Gamma
- le resultat est ensuite converti en facteur de luminosite

## Parametres retenus

- alpha = 7.0
- beta = 2.0
- seuil de conservation de la luminosite normale = 0.90
- luminosite minimale = 0.68
- exponent de contraste du remappage = 1.8

## Modifications par rapport a une Beta brute

Le mecanisme n'utilise pas la Beta de facon totalement brute.

Modifications appliquees :

- si l'echantillon Beta est au-dessus du seuil 0.90, la case reste a luminosite normale
- si l'echantillon est en dessous, il est remappe sur l'intervalle de luminosite [0.68, 1.0]
- le remappage applique ensuite une puissance 1.8 pour accentuer le contraste et rendre les zones sombres plus visibles
- la luminosite finale est discretisee sur 255 niveaux pour le rendu SFML

## Pourquoi ce choix

Cette loi a ete choisie parce qu'elle est bornee et facile a biaiser vers 1.0.

Concretement, cela permet :

- de garder la majorite des cases d'herbe visuellement normales
- d'introduire davantage de nuances intermediaires et quelques cases franchement plus sombres
- d'eviter l'effet trop mecanique d'une loi uniforme
- d'obtenir un rendu plus naturel et plus proche d'un terrain organique
