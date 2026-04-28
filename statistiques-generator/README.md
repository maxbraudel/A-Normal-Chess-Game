# Statistiques Generator

Viewer HTML/CSS/JS autonome pour les companions `Data/*.json` de A Normal Chess Game.

## Scope

- cible uniquement `schemaVersion = 5`
- charge automatiquement la source definie en haut de `app.js`
- rend trois blocs principaux: meteo/visibilite, infernal, XP
- expose aussi une vue schema/topologie et plusieurs tables d'audit

## Ouvrir le viewer

1. ouvrir `statistiques-generator/app.js`
2. definir `DATA_COMPANION_URL` tout en haut du fichier
3. lancer `python statistiques-generator/serve.py` depuis la racine du projet
4. ouvrir `http://127.0.0.1:8765/statistiques-generator/index.html`

Exemple:

```js
const DATA_COMPANION_URL = "../build/Data/KAZIMIRIUM%201.json";
```

Important:

- la page ne propose plus d'input utilisateur pour choisir le JSON
- un chemin Windows brut comme `C:\...\fichier.json` ne marche pas dans `fetch`
- un `file://...` direct ne marche pas non plus dans un navigateur standard
- `DATA_COMPANION_URL` doit pointer vers une URL servie par le mini serveur local ou un autre serveur HTTP(S)

## Sections rendues

### Meteo / visibilite

- courbes de pieces ennemies masquees pour blanc et noir
- couverture de brouillard dissimulant exposee par `analytics.visibility`
- marqueurs verticaux pour `weather_front_spawned` et `weather_front_ended`
- stats: couverture moyenne approx., intervalle moyen entre spawns, fronts simultanes, pic de pieces masquees

### Infernal

- courbes de dette de sang blanche et noire
- marqueurs de spawn colores par royaume cible
- badge texte pour la piece manifestee sous le marqueur
- stats: dette moyenne, delai moyen entre spawns, lifetime moyen observe

### XP

- XP total, moyenne, mediane, moyenne tronquee
- histogramme par turn et repartition par source
- table des sources et table brute des grants filtrable

## Sources de verite utilisees

- `turnHistory[].analytics.visibility`
- `turnHistory[].analytics.weather`
- `turnHistory[].analytics.infernal`
- `turnHistory[].analytics.entities.autonomousUnitIndex`
- `turnHistory[].structuredEvents`
- `turnHistory[].xpAuditTrail`
- `currentMetrics`, `currentAnalytics`, `currentStateSummary`

## Limites connues

- la "taille moyenne des nuages" est approchee a partir des cellules de brouillard exposees par le recorder, pas depuis une aire de front deja agregee
- si `historyContinuityComplete` vaut `false`, les stats temporelles ne couvrent pas forcement le vrai debut de partie
- si le CDN Chart.js est indisponible, la page reste lisible mais les graphiques ne seront pas traces