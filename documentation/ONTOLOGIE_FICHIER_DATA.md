# Ontologie du fichier Data de partie

## 1. Objet du document

Ce document decrit l'ontologie du fichier `Data/<saveName>.json` genere en parallele d'une sauvegarde de partie.

Objectif:

- expliquer a quelqu'un qui ne connait pas le code ce que contient le fichier
- expliquer comment les donnees sont organisees
- distinguer les donnees brutes autoritaires des donnees derivees pour l'analyse
- expliciter les conventions de representation pour de futures etudes statistiques, comparatives et longitudinales

Ce document correspond au schema actuellement ecrit par `GameDataRecorder`.

Version de schema actuelle:

- `schemaVersion = 2`

## 2. Philosophie generale du modele

Le fichier Data suit quatre principes structurants.

### 2.1 Un meme fichier sert deux usages differents

Le fichier sert a la fois:

- d'archive autoritaire de l'etat de jeu a plusieurs instants
- de base analytique directement exploitable sans avoir a reparcourir tout le snapshot brut a chaque requete

En consequence, le fichier contient toujours deux couches:

- une couche brute: `initialSnapshot`, `turnHistory[].snapshot`, `currentStateSummary`
- une couche derivee: `initialMetrics`, `initialAnalytics`, `turnHistory[].snapshotMetrics`, `turnHistory[].analytics`, `currentMetrics`, `currentAnalytics`

### 2.2 Le snapshot reste la source de verite finale

Si une ambiguite apparait entre une metrique derivee et le snapshot, le snapshot est l'autorite.

Les blocs derives sont la pour:

- accelerer l'analyse
- fournir des vues denormalisees et indexees
- eviter de recompiler a la main certaines notions metier, par exemple l'economie projetee, la visibilite par observateur ou l'index global des entites

### 2.3 Le schema essaye d'etre stable pour l'analyse

Quand c'est pertinent, une meme notion est representee par trois formes:

- `id`: entier technique compact
- `key`: cle machine stable, prevue pour les scripts et ETL
- `label`: libelle humain lisible

Exemple:

- `pieceTypeId = 3`
- `pieceTypeKey = "rook"`
- `pieceTypeLabel = "Rook"`

Pour une exploitation analytique durable, il faut privilegier `key` plutot que `label`.

### 2.4 Le fichier porte a la fois l'etat de partie et le contexte de comparaison

Le fichier ne contient pas seulement la partie en cours. Il contient aussi le contexte qui permet de comparer correctement deux parties:

- le `worldSeed`
- le mode et les participants
- les options de session
- un `configContext` resumant les parametres de gameplay actifs au moment de l'ecriture

Cela permet d'eviter de comparer aveuglement deux parties produites avec des regles differentes.

## 3. Vue d'ensemble de la structure racine

Le fichier racine suit cette forme generale:

```json
{
  "schemaVersion": 2,
  "saveName": "ExampleSave",
  "dataCollectionEnabled": true,
  "historyContinuityComplete": true,
  "loadedFromExistingCompanion": false,
  "createdAtUnix": 1770000000,
  "lastUpdatedAtUnix": 1770000100,
  "referenceData": { ... },
  "sessionContext": { ... },
  "configContext": { ... },
  "initialSnapshotReason": "initial_state_new_game",
  "initialMetrics": { ... },
  "initialAnalytics": { ... },
  "initialSnapshot": { ... },
  "turnHistory": [
    {
      "committedTurnNumber": 1,
      "committedActiveKingdom": 0,
      "gameOver": false,
      "winner": 0,
      "capturedAtUnix": 1770000050,
      "activeValidation": { ... },
      "nextTurnValidation": { ... },
      "queuedCommands": [ ... ],
      "notifications": [ ... ],
      "newEvents": [ ... ],
      "snapshotMetrics": { ... },
      "analytics": { ... },
      "snapshot": { ... }
    }
  ],
  "currentMetrics": { ... },
  "currentAnalytics": { ... },
  "currentStateSummary": { ... }
}
```

## 4. Conventions transversales de representation

### 4.1 Temps

Les timestamps `createdAtUnix`, `lastUpdatedAtUnix` et `capturedAtUnix` sont des secondes Unix.

### 4.2 Coordonnees

Les positions sont normalisees sous la forme:

```json
{ "x": 12, "y": 8 }
```

Le schema peut parfois fournir aussi des champs decomposes comme `originX`, `originY`, `destinationX`, `destinationY` pour faciliter certains traitements lineaires simples.

### 4.3 Boolens

Les drapeaux sont stockes comme de vrais booleens JSON `true` ou `false`.

### 4.4 Null explicite

Quand une entite facultative n'existe pas, le schema utilise `null` plutot qu'un objet vide.

Exemples:

- `currentAnalytics.chest.activeChest`
- `currentAnalytics.infernal.activeInfernal`

### 4.5 Tableaux vides preferes a l'omission

Quand une categorie de donnees existe mais ne contient aucun element, le schema privilegie un tableau vide `[]` plutot que l'absence de la cle.

### 4.6 Visibilite et brouillard

Les champs `hiddenFromWhite`, `hiddenFromBlack`, `destinationHiddenWhite`, `destinationHiddenBlack` sont calcules selon la logique actuelle de brouillard meteorologique.

Ils signifient:

- l'entite ou la destination est cachee si on observe la partie depuis le royaume indique

Ils ne veulent pas dire qu'une entite est globalement inconnue par un systeme de fog of war plus large. Ils suivent la logique runtime actuelle de `WeatherVisibility`.

### 4.7 Numerique brut versus cle semantique

Certaines zones du schema utilisent encore des entiers seuls a la racine d'un enregistrement, par exemple:

- `committedActiveKingdom`
- `winner`

Quand cela arrive, il faut decodet la valeur avec `referenceData`.

## 5. Champs top-level et leur semantique

| Champ | Type | Signification |
| --- | --- | --- |
| `schemaVersion` | entier | version du contrat de donnees |
| `saveName` | string | nom logique de la sauvegarde |
| `dataCollectionEnabled` | booleen | confirme si la collecte etait active |
| `historyContinuityComplete` | booleen | indique si l'historique est complet depuis le debut de la partie |
| `loadedFromExistingCompanion` | booleen | indique si le fichier courant prolonge un ancien companion existant |
| `createdAtUnix` | entier | date de creation initiale du companion |
| `lastUpdatedAtUnix` | entier | date de derniere ecriture du companion |
| `referenceData` | objet | dictionnaires de reference pour decoder les enums et ids |
| `sessionContext` | objet | contexte metier de la partie |
| `configContext` | objet | contexte de regles et de parametres |
| `initialSnapshotReason` | string | raison de construction du snapshot initial |
| `initialMetrics` | objet | agregats derives du snapshot initial |
| `initialAnalytics` | objet | vues analytiques derivees du snapshot initial |
| `initialSnapshot` | objet `SaveData` | etat autoritaire de depart |
| `turnHistory` | tableau | historique par turn commite |
| `currentMetrics` | objet | agregats derives de l'etat courant |
| `currentAnalytics` | objet | vues analytiques derivees de l'etat courant |
| `currentStateSummary` | objet `SaveData` | etat autoritaire courant |

## 6. Statut de continuite historique

Les deux champs suivants sont centraux pour l'analyse temporelle.

### 6.1 `historyContinuityComplete`

- `true`: l'historique couvre toute la partie depuis sa creation Data
- `false`: le fichier a ete reconstruit partiellement depuis un chargement de save sans ancien companion exploitable

Quand ce champ vaut `false`, il faut considerer que:

- `turnHistory` ne couvre pas necessairement tout le passe de la partie
- `initialSnapshot` est un point de reprise, pas forcement le vrai debut historique de la game

### 6.2 `loadedFromExistingCompanion`

- `true`: le recorder a relu un ancien companion existant
- `false`: le recorder a redemarre son archive depuis un snapshot bootstrap

### 6.3 `initialSnapshotReason`

Valeurs actuellement observees ou attendues:

- `initial_state_new_game`: nouvelle partie suivie des son debut
- `initial_state_loaded_game_partial`: partie chargee depuis une save sans ancien companion reutilisable

## 7. `referenceData`: dictionnaires de reference

`referenceData` est la dimension de reference du fichier.

Il contient des listes d'objets `{id, key, label}` pour:

- `kingdoms`
- `controllers`
- `gameModes`
- `pieceTypes`
- `buildingTypes`
- `chestRewardTypes`
- `turnCommandTypes`
- `eventKinds`
- `gameplayNotificationKinds`
- `autonomousUnitTypes`
- `infernalPhases`
- `weatherDirections`
- `xpRewardSources`

Usage recommande:

- utiliser `referenceData` comme table de dimensions en ETL
- decoder les champs numeriques qui ne repetent pas toujours `key` et `label`
- valider les hypothese de mapping de type dans les scripts d'analyse

## 8. `sessionContext`: contexte de session

`sessionContext` decrit la partie telle qu'elle est jouee, independamment des calculs derives.

Champs principaux:

| Champ | Type | Signification |
| --- | --- | --- |
| `saveName` | string | nom de la partie |
| `worldSeed` | entier | seed principale de la partie |
| `mapRadius` | entier | rayon logique de carte |
| `turnNumber` | entier | numero de turn du snapshot courant |
| `activeKingdomId`, `activeKingdomKey`, `activeKingdomLabel` | mixte | royaume actif dans le snapshot courant |
| `gameModeId`, `gameModeKey`, `gameModeLabel` | mixte | mode de partie courant |
| `participants[]` | tableau | participants par royaume |
| `multiplayer` | objet | contexte reseau |
| `options` | objet | options de session |

### 8.1 `participants[]`

Chaque participant contient:

- `kingdomId`, `kingdomKey`
- `controllerId`, `controllerKey`, `controllerLabel`
- `participantName`

### 8.2 `multiplayer`

Champs:

- `enabled`
- `port`
- `hasPassword`
- `protocolVersion`

### 8.3 `options`

Champs:

- `tacticalGridEnabled`
- `sharedTurnPreviewEnabled`
- `dataCollectionEnabled`

## 9. `configContext`: contexte des regles actives

`configContext` capture un resume des regles de gameplay actives lors de l'ecriture. C'est la cle de comparabilite inter-parties.

Sous-sections presentes:

- `map`
- `economy`
- `combat`
- `xp`
- `chest`
- `infernal`
- `weather`

### 9.1 `map`

Expose les parametres de generation et de structure globale de carte, par exemple:

- `mapRadius`
- `numMines`
- `numFarms`
- `minPublicBuildingDistance`
- `playerSpawnZonePercent`
- `aiSpawnZonePercent`

### 9.2 `economy`

Expose les parametres de simulation economique:

- `startingGold`
- `movementPointsPerTurn`
- `buildPointsPerTurn`
- `mineIncomeProfile`
- `farmIncomeProfile`
- `pieceRules[]`
- `upgradeRules[]`
- `buildingRules[]`

`pieceRules[]` fournit, pour chaque type de piece:

- cout de mouvement
- allowance de mouvement
- upkeep
- cout de recrutement
- temps de production

`upgradeRules[]` formalise explicitement les transitions d'upgrade actuellement exposees.

`buildingRules[]` fournit, pour chaque type de batiment:

- `buildPointCost`
- `repairCostPerCell`
- `destroyedCellsRequired`
- `width`
- `height`

### 9.3 `combat`

Expose notamment:

- `woodWallHP`
- `stoneWallHP`
- `barracksCellHP`
- `globalMaxRange`

### 9.4 `xp`

Expose:

- `thresholdPawnToKnightOrBishop`
- `thresholdToRook`
- `rewardSources[]`

Chaque entree de `rewardSources[]` contient:

- `sourceId`, `sourceKey`, `sourceLabel`
- `profile` avec `mean`, `sigmaMultiplierTimes100`, `clampSigmaMultiplierTimes100`, `minimum`

### 9.5 `chest`

Expose le contexte complet du systeme de coffre:

- fenetres de spawn
- profils de recompense
- poids early et late
- catch-up flag

### 9.6 `infernal`

Expose:

- timing de spawn
- parametres de lambda poisson
- dettes de sang
- comportement de recherche
- `targetWeights[]`

### 9.7 `weather`

Expose:

- timing de cooldown
- blocage ou non pendant un front actif
- parametres gamma d'arrivee et de duree
- vitesse du front
- poids de directions
- parametres geometriques et optiques de couverture et de densite

## 10. Le bloc initial: `initialMetrics`, `initialAnalytics`, `initialSnapshot`

Le fichier conserve toujours un triplet initial.

### 10.1 `initialSnapshot`

`initialSnapshot` est un objet `SaveData` complet.

Il represente:

- soit l'etat initial d'une nouvelle partie
- soit le point de reprise d'une partie chargee si l'historique complet n'etait pas disponible

### 10.2 `initialMetrics`

`initialMetrics` est un resume agrege de haut niveau du snapshot initial.

Champs principaux:

- `turnNumber`
- `activeKingdomId`, `activeKingdomKey`
- `whiteGold`, `blackGold`
- `whiteGrossIncome`, `blackGrossIncome`
- `whiteUpkeepCost`, `blackUpkeepCost`
- `whiteNetIncome`, `blackNetIncome`
- `whitePieceCount`, `blackPieceCount`
- `whiteBuildingCount`, `blackBuildingCount`
- `publicBuildingCount`
- `mapObjectCount`
- `chestCount`
- `autonomousUnitCount`
- `weatherFrontCount`
- `fogCellCount`
- `concealingFogCellCount`
- `whiteBloodDebt`, `blackBloodDebt`
- `activeInfernalUnitId`
- `activeChestObjectId`
- `recordedEventCount`

Cette couche est destinee aux statistiques rapides et aux dashboards.

### 10.3 `initialAnalytics`

`initialAnalytics` est une vue analytiques denormalisee du snapshot initial.

Sa structure est identique a `currentAnalytics` et a `turnHistory[].analytics`.

## 11. `turnHistory`: ontologie d'un enregistrement de turn

Chaque element de `turnHistory` represente un turn commite.

Structure generale:

| Champ | Type | Signification |
| --- | --- | --- |
| `committedTurnNumber` | entier | numero du turn commite |
| `committedActiveKingdom` | entier | royaume dont le turn vient d'etre commite |
| `gameOver` | booleen | la partie est terminee apres ce commit |
| `winner` | entier | vainqueur si connu |
| `capturedAtUnix` | entier | date de capture de l'enregistrement |
| `activeValidation` | objet | validation du turn du royaume actif avant commit |
| `nextTurnValidation` | objet | validation calculee pour l'etat suivant |
| `queuedCommands` | tableau | commandes qui composaient le turn soumis |
| `notifications` | tableau | notifications gameplay generees par le commit |
| `newEvents` | tableau | nouveaux evenements ajoutes au journal entre le precedent point enregistre et ce snapshot |
| `snapshotMetrics` | objet | resume agrege du snapshot post-commit |
| `analytics` | objet | vue analytique post-commit |
| `snapshot` | objet `SaveData` | snapshot autoritaire post-commit |

### 11.1 `activeValidation` et `nextTurnValidation`

Ces deux objets ont la meme structure:

- `valid`
- `activeKingInCheck`
- `projectedKingInCheck`
- `hasAnyLegalResponse`
- `requiresSingleResponseMove`
- `hasQueuedMove`
- `bankrupt`
- `projectedEndingGold`
- `errorMessage`

Interpretation:

- `activeValidation` decrit le statut de validation du turn en cours de commit
- `nextTurnValidation` decrit le statut de validation calcule sur l'etat resultant

### 11.2 `queuedCommands`

`queuedCommands` contient la file de commandes qui composait le tour au moment du commit.

Chaque commande porte un superset de champs pour couvrir tous les types de commande. Certains champs sont donc pertinents seulement pour certains `typeKey`.

Champs communs:

- `type`, `typeKey`, `typeLabel`
- `pieceId`
- `originX`, `originY`, `origin`
- `destinationX`, `destinationY`, `destination`
- `buildId`
- `buildingType`, `buildingTypeKey`, `buildingTypeLabel`
- `buildOriginX`, `buildOriginY`, `buildOrigin`
- `buildRotationQuarterTurns`
- `barracksId`
- `produceType`, `produceTypeKey`, `produceTypeLabel`
- `upgradePieceId`
- `upgradeTarget`, `upgradeTargetKey`, `upgradeTargetLabel`
- `formationId`

Lecture pratique par type:

- `move`: utiliser surtout `pieceId`, `origin`, `destination`
- `build`: utiliser surtout `buildId`, `buildingType*`, `buildOrigin`, `buildRotationQuarterTurns`
- `produce`: utiliser surtout `barracksId`, `produceType*`
- `upgrade`: utiliser surtout `upgradePieceId`, `upgradeTarget*`
- `form_group`, `break_group`, `disband`, `marry`: utiliser selon la logique metier, principalement `pieceId`, `formationId`, ou la position cible selon le cas

Important:

- `queuedCommands` represente l'intention soumise au moteur
- ce n'est pas encore une trace causale fine de sous-resolution interne commande par commande

### 11.3 `notifications`

`notifications` contient les notifications gameplay user-facing ou quasi user-facing generees par le commit.

Structure actuelle:

- `kind`, `kindKey`, `kindLabel`
- `kingdom`, `kingdomKey`, `kingdomLabel`
- `chestRewardType`, `chestRewardTypeKey`
- `chestRewardAmount`
- `chestReward`
- `title`
- `message`

Usage principal actuel:

- recompenses de coffre

### 11.4 `newEvents`

`newEvents` est un delta d'evenements par rapport au dernier point d'enregistrement Data.

Chaque evenement contient:

- `turnNumber`
- `kingdom`, `kingdomKey`, `kingdomLabel`
- `message`
- `kind`, `kindKey`, `kindLabel`
- `pieceType`, `pieceTypeKey`, `pieceTypeLabel`
- `destinationX`, `destinationY`, `destination`
- `destinationHiddenWhite`
- `destinationHiddenBlack`

Important:

- `newEvents` est un delta local a l'enregistrement Data
- `snapshot.events` dans le `SaveData` reste la version cumulative du journal

### 11.5 `snapshotMetrics`, `analytics`, `snapshot`

Ces trois champs doivent etre lus ensemble.

- `snapshot`: etat exact post-commit
- `snapshotMetrics`: resume rapide post-commit
- `analytics`: vue denormalisee post-commit

## 12. Le bloc courant: `currentMetrics`, `currentAnalytics`, `currentStateSummary`

Le bloc courant represente l'etat le plus recent connu du recorder.

Regle de construction:

- si `turnHistory` n'est pas vide, l'etat courant est celui du dernier `turnHistory[].snapshot`
- sinon, l'etat courant est `initialSnapshot`

En pratique:

- `currentStateSummary` = snapshot autoritaire courant
- `currentMetrics` = resume agrege de `currentStateSummary`
- `currentAnalytics` = vue analytique derivee de `currentStateSummary`

## 13. Ontologie detaillee de `analytics`

Le bloc `analytics` existe sous trois formes:

- `initialAnalytics`
- `turnHistory[].analytics`
- `currentAnalytics`

La structure interne est la meme.

```json
{
  "turnNumber": 12,
  "activeKingdomId": 1,
  "activeKingdomKey": "black",
  "economy": { ... },
  "visibility": { ... },
  "weather": { ... },
  "chest": { ... },
  "infernal": { ... },
  "entities": {
    "pieceIndex": [ ... ],
    "buildingIndex": [ ... ],
    "mapObjectIndex": [ ... ],
    "autonomousUnitIndex": [ ... ]
  }
}
```

### 13.1 `analytics.economy`

Deux sous-niveaux:

- `byKingdom[]`
- `publicResourceBuildings[]`

#### 13.1.1 `byKingdom[]`

Pour chaque royaume:

- `kingdomId`, `kingdomKey`
- `gold`
- `movementPointsMaxBonus`
- `buildPointsMaxBonus`
- `grossIncome`
- `upkeepCost`
- `netIncome`
- `projectedEndingGold`
- `wouldBeBankrupt`
- `pieceCountsByType[]`
- `buildingCountsByType[]`

#### 13.1.2 `publicResourceBuildings[]`

Pour chaque batiment public de type ressource actif:

- `buildingId`
- `buildingTypeId`, `buildingTypeKey`, `buildingTypeLabel`
- `whiteOccupiedCells`, `blackOccupiedCells`
- `whiteIncome`, `blackIncome`

Cette structure est ideale pour des etudes sur la valeur strategique des ressources publiques.

### 13.2 `analytics.visibility`

Champs globaux:

- `maskDiameter`
- `hasActiveFront`
- `fogCellCount`
- `concealingFogCellCount`
- `maxAlpha`
- `maxShade`
- `averageAlphaOnFogCells`
- `averageShadeOnFogCells`

Sous-bloc `byObserver[]`:

- `observerKingdomId`, `observerKingdomKey`
- `hiddenEnemyPieceIds[]`
- `hiddenEnemyBuildingIds[]`
- `hiddenAutonomousUnitIds[]`

Interpretation:

- ce bloc ne recopie pas tous les objets caches
- il donne, pour chaque observateur, la liste des identifiants actuellement caches par le brouillard meteorologique

### 13.3 `analytics.weather`

Expose l'etat analytique du systeme meteo:

- `nextSpawnTurnStep`
- `rngCounter`
- `revision`
- `frontCount`
- `fronts[]`

Chaque front contient:

- `directionId`, `directionKey`, `directionLabel`
- `currentTurnStep`
- `totalTurnSteps`
- `centerStartXTimes1000`, `centerStartYTimes1000`
- `stepXTimes1000`, `stepYTimes1000`
- `radiusAlongTimes1000`, `radiusAcrossTimes1000`
- `shapeSeed`
- `densitySeed`

Ce bloc est central pour des etudes sur l'impact de la meteo et la reproductibilite des fronts actifs.

### 13.4 `analytics.chest`

Expose l'etat analytique du systeme coffre:

- `activeChestObjectId`
- `nextSpawnTurn`
- `rngCounter`
- `rewardRngCounter`
- `lootProgression`
- `activeChest`

`lootProgression` contient:

- `hasCurrentReward`
- `currentRewardGeneration`
- `currentReward`
- `lastCollectedGenerationByKingdom[]`

`activeChest` est soit `null`, soit une entree de map object denormalisee.

### 13.5 `analytics.infernal`

Expose l'etat analytique du systeme infernal:

- `activeInfernalUnitId`
- `nextSpawnTurn`
- `whiteBloodDebt`
- `blackBloodDebt`
- `rngCounter`
- `activeInfernal`

`activeInfernal` est soit `null`, soit une entree denormalisee issue de l'index des unites autonomes.

### 13.6 `analytics.entities`

Ce bloc fournit des index denormalises globaux pour eviter d'aller chercher les entites en profondeur dans le snapshot.

Il contient:

- `pieceIndex[]`
- `buildingIndex[]`
- `mapObjectIndex[]`
- `autonomousUnitIndex[]`

#### 13.6.1 `pieceIndex[]`

Chaque piece contient:

- `id`
- `pieceTypeId`, `pieceTypeKey`, `pieceTypeLabel`
- `kingdomId`, `kingdomKey`, `kingdomLabel`
- `position`
- `xp`
- `formationId`
- `hiddenFromWhite`
- `hiddenFromBlack`

Usage analytique:

- suivi des pieces par id au fil des turns
- etudes sur l'XP, la survie, la visibilite et les formations

#### 13.6.2 `buildingIndex[]`

Cet index fusionne:

- les batiments appartenant aux royaumes
- les `publicBuildings`

Chaque batiment contient:

- `id`
- `buildingTypeId`, `buildingTypeKey`, `buildingTypeLabel`
- `isPublic`
- `isNeutral`
- `ownerKingdomId`, `ownerKingdomKey`
- `origin`
- `footprintWidth`, `footprintHeight`
- `rotationQuarterTurns`
- `flipMask`
- `state`
- `destroyed`
- `destroyedCellCount`
- `destroyedCellsRequired`
- `isProducing`
- `producingTypeId`
- `turnsRemaining`
- `hiddenFromWhite`
- `hiddenFromBlack`
- `cells[]`

Chaque entree de `cells[]` contient:

- `footprintLocal`
- `sourceLocal`
- `worldCell`
- `destroyed`
- `breached`
- `hp`
- `hiddenFromWhite`
- `hiddenFromBlack`

Cette granularite permet des etudes tres fines sur:

- l'endommagement cellule par cellule
- les breches persistantes
- la geometrie des structures
- la visibilite locale d'un batiment multi-cellules

#### 13.6.3 `mapObjectIndex[]`

Actuellement, cet index represente les objets de carte de type coffre.

Chaque entree contient:

- `id`
- `typeId`, `typeKey`, `typeLabel`
- `position`
- `isActiveChest`
- `chest`

Le sous-bloc `chest` contient:

- `spawnTurn`
- `reward`

#### 13.6.4 `autonomousUnitIndex[]`

Actuellement, cet index represente les unites autonomes infernales.

Chaque entree contient:

- `id`
- `typeId`, `typeKey`, `typeLabel`
- `position`
- `isActiveInfernal`
- `hiddenFromWhite`
- `hiddenFromBlack`
- `infernal`

Le sous-bloc `infernal` contient:

- `targetKingdomId`, `targetKingdomKey`
- `targetPieceId`
- `manifestedPieceTypeId`, `manifestedPieceTypeKey`
- `preferredTargetTypeId`, `preferredTargetTypeKey`
- `phaseId`, `phaseLabel`
- `returnBorderCell`
- `spawnTurn`

## 14. Ontologie de `SaveData` dans les snapshots autoritaires

`initialSnapshot`, `turnHistory[].snapshot` et `currentStateSummary` sont des `SaveData` serialises.

Leur structure de haut niveau est la suivante:

| Champ | Type | Signification |
| --- | --- | --- |
| `gameName` | string | nom de partie |
| `turnNumber` | entier | turn courant |
| `activeKingdom` | entier | royaume actif |
| `mapRadius` | entier | rayon logique de carte |
| `worldSeed` | entier | seed de partie |
| `sessionKingdoms[]` | tableau | participants par royaume |
| `multiplayer` | objet | contexte reseau |
| `tacticalGridEnabled` | booleen | option active |
| `sharedTurnPreviewEnabled` | booleen | option active |
| `dataCollectionEnabled` | booleen | option Data active |
| `grid[][]` | matrice | etat complet du terrain |
| `kingdoms[]` | tableau | etat complet des royaumes |
| `publicBuildings[]` | tableau | batiments publics |
| `mapObjects[]` | tableau | objets de carte |
| `chestSystemState` | objet | etat interne du systeme coffre |
| `weatherSystemState` | objet | etat interne du systeme meteo |
| `weatherMaskCache` | objet | cache du masque de brouillard |
| `xpSystemState` | objet | etat interne XP |
| `autonomousUnits[]` | tableau | unites autonomes |
| `infernalSystemState` | objet | etat interne infernal |
| `events[]` | tableau | journal cumulatif |
| `commandHistory[]` | tableau | reserve pour replay futur |

### 14.1 `grid[][]`

Chaque cellule contient:

- `type`
- `isInCircle`
- `terrainFlipMask`
- `terrainBrightness`

### 14.2 `kingdoms[]`

Chaque royaume contient:

- `id`
- `gold`
- `movementPointsMaxBonus`
- `buildPointsMaxBonus`
- `hasSpawnedBishop`
- `lastBishopSpawnParity`
- `pieces[]`
- `buildings[]`

### 14.3 Pourquoi conserver aussi `SaveData` si `analytics` existe deja

Parce que `SaveData` permet:

- la restauration exacte du runtime
- les analyses ad hoc qui n'etaient pas prevues a l'avance
- la verification des divergences eventuelles avec les couches derivees

## 15. Comment exploiter correctement le fichier pour des etudes futures

### 15.1 Pour des statistiques rapides

Privilegier:

- `initialMetrics`
- `turnHistory[].snapshotMetrics`
- `currentMetrics`

Ces blocs sont faits pour des agregations rapides sans reparcourir tous les objets.

### 15.2 Pour des analyses systemiques

Privilegier:

- `initialAnalytics`
- `turnHistory[].analytics`
- `currentAnalytics`

Ces blocs sont adaptes aux etudes par sous-systeme:

- economie
- visibilite
- meteo
- coffre
- infernal
- evolution des entites

### 15.3 Pour des analyses exactes ou nouvelles

Privilegier:

- `initialSnapshot`
- `turnHistory[].snapshot`
- `currentStateSummary`

Ces blocs sont indispensables si l'on veut:

- reconstruire des features qui n'existent pas encore dans `analytics`
- auditer la coherence des metriques derivees
- developper de nouveaux indicateurs sans changer l'historique deja produit

### 15.4 Clefs d'exploitation recommandees

Bonnes pratiques:

- utiliser `schemaVersion` pour gerer les migrations de parseur
- utiliser `key` plutot que `label` dans les pipelines analytiques
- utiliser `referenceData` comme dictionnaire source
- verifier `historyContinuityComplete` avant toute etude temporelle complete
- utiliser les `id` d'entites comme identifiants intra-partie
- comparer des parties seulement apres avoir controle `configContext`

### 15.5 Strategie de jointure conseillee

Pour suivre une entite a travers le temps dans une meme partie:

- pieces: joindre par `pieceIndex[].id`
- batiments: joindre par `buildingIndex[].id`
- objets de carte: joindre par `mapObjectIndex[].id`
- unites autonomes: joindre par `autonomousUnitIndex[].id`

Attention:

- ces ids sont pertinents a l'interieur d'une partie donnee
- il ne faut pas supposer qu'un id numerique a une signification globale entre deux parties differentes

## 16. Limites connues du schema actuel

Le schema est deja tres exploitable, mais il a encore des limites explicites.

### 16.1 Pas encore de trace causale fine commande par commande

`queuedCommands` enregistre l'intention du turn, mais pas encore:

- le succes ou l'echec detaille de chaque sous-effet interne
- l'ordre exact de toutes les resolutions internes du moteur
- un journal exhaustif des deltas atomiques cause par une commande donnee

### 16.2 `commandHistory` dans `SaveData` n'est pas la surface historique principale

L'historique temporel a utiliser pour l'analyse est `turnHistory`, pas `snapshot.commandHistory`.

### 16.3 La visibilite actuelle est centree sur le brouillard meteo

Les champs `hiddenFromWhite` et `hiddenFromBlack` suivent la logique actuelle de `WeatherVisibility`. Ils ne constituent pas un systeme de vision general abstrait independant du runtime courant.

## 17. Resume final

Le fichier Data n'est pas un simple dump JSON. C'est un document hybride structure pour la recherche et l'analyse.

Il combine:

- un historique de turns
- des snapshots autoritaires complets
- des dictionnaires de reference
- un contexte de configuration
- des vues analytiques denormalisees par sous-systeme
- des index globaux d'entites faciles a joindre dans le temps

La lecture recommandee est la suivante:

1. commencer par `schemaVersion`, `historyContinuityComplete`, `sessionContext`, `configContext`
2. utiliser `currentMetrics` ou `turnHistory[].snapshotMetrics` pour les analyses rapides
3. utiliser `currentAnalytics` ou `turnHistory[].analytics` pour les analyses metier structurees
4. retomber sur les snapshots `SaveData` quand on veut l'exactitude totale ou une nouvelle feature analytique

Dans cet etat, le schema est deja adapte a des etudes statistiques et comparatives precises, a condition de respecter la distinction entre couches derivees et snapshots autoritaires.