# Rapport d'audit sur l'exhaustivite du fichier Data

## 1. Question posee

Question a laquelle ce document repond:

- le fichier `Data/<saveName>.json` est-il exhaustif ?
- est-il deja suffisant pour des etudes statistiques et comparatives serieuses ?
- a-t-on oublie des donnees qui pourraient devenir importantes plus tard ?

## 2. Methode d'audit

L'audit a ete mene en croisant quatre sources:

- le modele autoritaire sauvegarde dans `SaveData`
- le pipeline runtime qui applique un turn et fait vivre les systemes de jeu
- le generateur `GameDataRecorder`
- un fichier reel genere par le jeu

Concretement, l'analyse a porte sur:

- `src/Save/SaveData.hpp`
- `src/Core/GameEngine.cpp`
- `src/Runtime/TurnCoordinator.cpp`
- `src/Data/GameDataRecorder.cpp`
- `src/Systems/TurnSystem.cpp`
- les etats systeme `ChestSystem`, `InfernalSystem`, `WeatherSystem`, `XPSystem`
- `build/Data/pepo.json`

Le critere principal est le suivant:

- une donnee est consideree comme couverte si elle est soit exportee directement, soit reconstructible de maniere fiable et stable a partir des snapshots, des commandes commitees et du contexte de configuration

Point de verification important:

- le fichier reel `build/Data/pepo.json` actuellement present dans le workspace est encore un artifact en schema V2
- le generateur present dans le code source est, lui, deja passe en schema V5
- il existe donc un ecart temporaire entre l'etat du code et ce companion precis, qui s'explique par le fait que ce fichier n'a pas ete regenere apres les derniers changements du recorder

## 3. Verdict court

Verdict synthetique:

- oui, le fichier Data est deja tres riche et globalement suffisant pour des analyses statistiques serieuses au niveau etat de partie / evolution par turn
- non, il n'est pas totalement exhaustif si l'objectif futur est de faire de l'analyse causale moteur atomique ou de la capture brute tres haute frequence des gestes utilisateur

Important pour interpreter correctement ce verdict:

- le verdict porte sur le generateur actuel de la codebase, pas uniquement sur le companion `build/Data/pepo.json` ouvert dans le workspace
- sur disque, `build/Data/pepo.json` ne montre pas encore `provenance`, `turnDelta`, `structuredEvents`, `commandAuditTrail`, `xpAuditTrail`, `behavioralTelemetry` ni `pendingTurnTelemetry`, parce qu'il a ete produit avant la mise a jour V5
- des qu'une sauvegarde est regeneree avec le binaire actuel, le companion attendu doit embarquer ces nouveaux blocs

Autrement dit:

- pour repondre a des questions du type `que vaut l'etat du jeu a tel turn ?`, `comment evoluent l'economie, le brouillard, les pieces, les batiments, les coffres, l'infernal ?`, le fichier est deja solide
- pour repondre a des questions du type `pourquoi exactement cette variable a bouge ?`, `quelle action systeme precise a cause tel changement ?`, `quelles actions ont ete tentees puis refusees ?`, il manque encore quelques couches semantiques

Conclusion courte:

- exhaustif pour l'etat autoritaire par turn: presque oui
- suffisant pour des statistiques descriptives, longitudinales et comparatives: oui
- exhaustif pour l'analyse comportementale semantique et l'orchestration reseau de turn: oui, largement
- exhaustif pour la causalite moteur atomique la plus basse: pas encore

## 4. Ce qui est deja tres bien couvert

### 4.1 Etat autoritaire complet de la partie

Le point fort principal du systeme actuel est qu'il ne se limite pas a des agregats. Il exporte aussi le snapshot brut autoritaire.

Cela couvre notamment:

- la grille complete et son terrain
- tous les royaumes
- toutes les pieces avec id, type, position, xp et formation
- tous les batiments, y compris les cellules detaillees, les degats et l'etat de production
- tous les objets de carte
- toutes les unites autonomes
- les etats runtime des systemes coffre, meteo, XP et infernal
- le cache de brouillard meteorologique
- l'historique d'evenements runtime connu par le moteur

En pratique, cela veut dire qu'on peut revenir a un etat de jeu donne sans perte importante d'information structurelle.

### 4.2 Capture au bon moment du cycle de turn

Le snapshot de `turnHistory[]` est pris apres le commit autoritaire complet du turn, c'est-a-dire apres:

- l'execution des commandes commitees
- les effets de structure et de reparation
- les revenus et XP d'arena
- le spawn eventuel de coffre
- le spawn eventuel de l'infernal
- l'action post-turn de l'infernal
- l'avancement / spawn meteo

Ce point est important: l'enregistrement correspond a l'etat autoritaire final du turn, pas a une photo intermediaire trompeuse.

### 4.3 Variables cachees importantes deja presentes

Les variables qui auraient typiquement ete oubliees dans une premiere version sont, ici, majoritairement deja presentes:

- dette infernale blanche et noire
- prochain spawn infernal
- rng counters des systemes stochastiques
- progression de loot des coffres
- fronts meteo actifs
- masque de brouillard meteorologique
- bonus permanents de mouvement et de construction
- contexte de configuration de gameplay

Sur ce point, le generateur est deja nettement au-dessus d'un simple export de sauvegarde.

### 4.4 Reconstructibilite du hasard

Le hasard n'est pas stocke sous la forme d'un etat opaque de generateur pseudo-aleatoire, mais sous une combinaison deterministic-friendly:

- `worldSeed`
- compteurs RNG de systeme
- seeds materialisees quand necessaire, par exemple `shapeSeed` et `densitySeed` pour la meteo

Pour la meteo, l'XP, les coffres et l'infernal, le code montre que les tirages utilisent `worldSeed` et des compteurs incrementaux. Cela veut dire qu'il n'y a pas, a ce stade, d'etat aleatoire cache evident manquant pour la reproductibilite.

### 4.5 Couverture analytique deja exploitable sans retraitement lourd

Le schema actuel ne stocke pas seulement `snapshot`, il ajoute aussi des couches derivees deja tres utiles:

- `snapshotMetrics`
- `analytics.economy`
- `analytics.visibility`
- `analytics.weather`
- `analytics.chest`
- `analytics.infernal`
- indexes denormalises des pieces, batiments, objets et unites autonomes

Cela rend possible des etudes directes sur:

- l'economie par royaume
- la pression de brouillard
- la dynamique des fronts meteo
- la progression des coffres
- l'evolution de la dette infernale
- la densite materielle et structurelle de la carte

## 5. Ce qui est suffisant des maintenant

Le systeme actuel est deja suffisant pour les familles d'etudes suivantes.

### 5.1 Etudes longitudinales par turn

Exemples:

- evolution de l'or par royaume
- evolution du materiel par type de piece
- evolution du nombre de batiments par type
- evolution de la dette infernale
- evolution de la couverture de brouillard
- evolution de la pression economique des batiments publics

### 5.2 Etudes comparatives inter-parties

Exemples:

- comparer des parties issues de seeds differents
- comparer des parties jouees sous regles differentes
- comparer des parties avec densites meteo ou infernales differentes
- comparer des trajectoires economiques ou structurelles

Le `configContext` joue ici un role central.

### 5.3 Etudes de reconstruction et d'audit runtime

Exemples:

- verifier qu'un coffre a bien apparu puis ete ouvert
- verifier qu'une unite infernale etait bien active a tel moment
- verifier qu'un roi etait en echec ou mat via les validations enregistrees
- verifier l'etat detaille d'un batiment, y compris ses cellules

### 5.4 Etudes sur les systemes stochastiques

Exemples:

- impact de la meteo sur la partie
- rythme des apparitions de coffres
- distribution des recompenses de coffres
- dynamique de la dette et des apparitions infernales
- progression XP dans le temps

Le jeu exporte deja assez de contexte pour des analyses statistiques solides sur ces sujets.

## 6. Ce qui a ete ajoute suite a l'audit, et ce qui reste vraiment

Depuis l'audit initial, les principaux manques structurels identifies ont ete traites dans le schema V5.

### 6.1 Journal analytique type: ajoute

Le fichier expose maintenant `structuredEvents`, c'est-a-dire une vue lineaire d'evenements types construite a partir:

- des commandes commitees
- du `commandAuditTrail`
- des diffs entre snapshots successifs

Cela couvre des classes d'evenements qui n'etaient auparavant presentes qu'en texte libre ou implicites dans les snapshots:

- commandes acceptees, rejetees, remplacees ou annulees
- apparition, deplacement, upgrade et retrait des pieces
- pose et retrait de batiments
- debuts et fins de production
- apparition et ouverture de coffres
- spawn, deplacement, changement de phase et retrait de l'infernal
- apparition et fin des fronts meteo

Verdict actuel:

- le manque de journal typé n'est plus un manque bloquant pour l'analyse statistique future
- il reste en revanche une limite sur l'attribution atomique parfaite de chaque micro-effet interne
- le fichier `build/Data/pepo.json` actuellement observe ne contient pas encore cette couche, mais le generateur source l'ecrit bien

### 6.2 Deltas causes: ajoutes de maniere derivee

Le fichier expose maintenant `turnDelta`, qui structure explicitement les changements entre deux snapshots successifs.

On y trouve notamment:

- les budgets de turn et les points depenses / inutilises
- les deltas economiques par royaume
- les deltas de dette infernale
- les captures, apparitions et retraits d'entites
- les changements de production et de cellules de batiment
- les transitions d'objets de carte, d'unites autonomes et de fronts meteo

Verdict actuel:

- les deltas metier importants sont maintenant directement requetables
- ce qui manque encore est surtout la cause atomique par sous-effet moteur, pas le diff structure lui-meme
- la meme reserve operationnelle vaut ici: le companion V2 observe ne les expose pas encore, mais la version source du recorder oui

### 6.3 Intentions non abouties: ajoutees

Le fichier expose maintenant `commandAuditTrail`, qui enregistre:

- les tentatives de `queue`
- les `replace` de mouvements
- les `cancel`
- les `reset` de draft
- les raisons de rejet ou d'acceptation associees

Cela ferme le principal angle mort sur l'intention joueur avant commit.

Verdict actuel:

- suffisant pour analyser ce qui a ete tente, refuse, annule ou finalement retenu dans le draft
- pas encore exhaustif pour des rejets purement reseau / orchestration hors `TurnSystem`
- cette trace n'est pas encore visible dans `build/Data/pepo.json` pour la meme raison de non-regeneration du fichier reel

### 6.4 Provenance: ajoutee et desormais largement suffisante

Le fichier expose maintenant un bloc `provenance` qui fournit a la fois des empreintes stables du contrat exporte et des metadonnees logicielles de build:

- version de schema
- date de configuration CMake
- type de build
- generateur CMake
- identite et version du compilateur
- systeme cible
- branche git, commit git et etat dirty
- hash du bloc `referenceData`
- hash du bloc `sessionContext`
- hash du bloc `configContext`

Verdict actuel:

- suffisante pour distinguer de maniere robuste deux exports produits par des builds ou revisions differents
- suffisante pour tracer la famille de format et les empreintes des grands blocs de contexte

### 6.5 Telemetrie comportementale et orchestration LAN: ajoutees

Le fichier expose maintenant une couche temporelle complementaire, structuree pour l'etude de la preparation de turn humaine:

- `turnHistory[].behavioralTelemetry` pour le turn finalement commite
- `pendingTurnTelemetry` pour l'etat courant du turn encore en preparation au moment du save

Cette couche enregistre notamment:

- les interactions significatives de preparation: selections, ouvertures de preview, rotations de build et changements semantiques du draft
- une chronologie exploitable via `interactionTimeline` et `orchestrationEvents`
- les etapes d'orchestration: preview partagee, soumission client, reception host, validation, acceptation ou rejet, debut et fin de commit
- la provenance des signaux: `local_host`, `local_client`, `remote_client_reported`, `host_observed`

Verdict actuel:

- le principal angle mort sur l'intention et le rythme de preparation de turn est maintenant ferme
- la compatibilite LAN est bonne: le protocole multijoueur V3 transporte la telemetrie dans `MultiplayerTurnPreview` et `MultiplayerTurnSubmission`
- le systeme reste volontairement non intrusif: il ne capture pas les flux bruts de souris ou hover a haute frequence
- il ne capture pas encore une couche dediee de performance reseau telle que la latence, le round-trip time, la duree de mise en file ou les delais de retransmission

### 6.6 Attribution XP: ajoutee sous forme de piste d'audit brute

Le fichier expose maintenant `xpAuditTrail`, qui journalise chaque gain d'XP autoritaire avec:

- source exacte du gain
- montant gagne
- piece receptrice, royaume et position
- XP avant et apres attribution
- type de victime si pertinent
- compteurs RNG avant et apres tirage

Cette piste est ensuite reprojectee dans `structuredEvents` sous une forme requetable de type `xp_granted`.

Verdict actuel:

- le principal angle mort sur l'attribution d'XP est ferme
- on peut maintenant etudier la progression XP par source sans inference fragile a partir des seuls snapshots
- il ne manque plus qu'une causalite moteur encore plus atomique si l'on voulait relier chaque gain a une sous-phase interne fine du commit

### 6.7 Conclusion operationnelle de la verification finale

La verification finale fait apparaitre deux niveaux de verite complementaires:

- au niveau codebase et pipeline runtime/save, le systeme Data correspond maintenant au schema V5 et couvre l'essentiel des besoins statistiques futurs
- au niveau artifact observe, `build/Data/pepo.json` est un ancien export V2 et ne doit pas etre pris comme preuve que les nouveaux champs manquent encore dans le generateur

Autrement dit:

- pour juger l'exhaustivite du systeme, il faut regarder le code actuel et le pipeline runtime/save
- pour juger l'exhaustivite d'un fichier precis deja present sur disque, il faut aussi tenir compte de sa date et de sa `schemaVersion`

### 6.8 Ce qui reste vraiment ouvert

Les manques residuels ne sont plus des trous majeurs de couverture de l'etat du jeu. Ils sont concentres sur des besoins d'analyse plus fins et plus specialises:

- causalite moteur atomique: il n'existe pas encore de chronologie interne qui rattache chaque sous-effet de commit a la regle exacte qui l'a produit
- deliberation IA: le fichier n'exporte pas encore les candidats evalues, les scores internes, la justification du plan retenu ou les raisons d'abandon d'autres plans
- performance reseau: il n'existe pas encore de metriques dediees de latence, de round-trip, de delai entre preview et reception, ou de temps de reconciliation
- micro-gestes utilisateur: les trajectoires de pointeur, survols continus et autres signaux haute frequence sont volontairement exclus

## 7. Ce qui n'est pas un oubli critique

Il est important de ne pas sur-auditer a tort. Certaines donnees pourraient sembler manquantes mais ne constituent pas, aujourd'hui, un vrai trou critique.

### 7.1 Le mouvement des pieces n'est pas oublie

Les mouvements commites sont deja couverts par:

- `queuedCommands`
- `structuredEvents`
- `turnDelta`
- les snapshots successifs

Donc, pour les pieces joueuses, l'historique de mouvement est deja exploitable.

### 7.2 Les variables cachees de l'infernal ne sont pas oubliees

Le systeme exporte deja:

- la dette par royaume
- l'unite infernale active
- sa phase
- sa cible
- son type manifeste
- son point de retour
- ses compteurs et son calendrier de spawn

Le coeur de l'etat cache infernal est donc bien present.

### 7.3 Le brouillard meteorologique n'est pas reduit a un simple resume

Le jeu n'exporte pas seulement des compteurs.

Il exporte aussi:

- les fronts meteo
- les masques de brouillard
- des analyses de visibilite par observateur

Pour l'etat de visibilite, la couverture est deja serieuse.

## 8. Priorite des ajouts recommandes

Si l'objectif est de pousser encore plus loin la valeur analytique future, les priorites restantes sont maintenant les suivantes.

### Priorite 1: causalite interne encore plus fine

Si necessaire un jour pour la recherche ou le replay analytique, ajouter:

- une chronologie interne des sous-effets de commit
- un rattachement stable commande -> sous-effets -> deltas atomiques

### Priorite 2: deliberation IA exploitable

Si l'on veut etudier plus tard le comportement de l'IA elle-meme, ajouter:

- les plans candidats consideres
- les scores ou heuristiques dominantes
- la raison du plan selectionne

### Priorite 3: performance reseau et orchestration

Si l'on veut etudier la qualite de l'experience LAN, ajouter:

- latence preview -> reception
- latence submission -> acceptation ou rejet
- round-trip time, retries et delais de reconciliation

### Priorite 4: telemetrie gestuelle haute frequence

Seulement si une etude UX le justifie un jour, ajouter:

- trajectoires de pointeur
- survols continus
- granularite temporelle plus fine entre interactions significatives

## 9. Reponse finale a la question

Reponse courte, mise a jour apres verification finale du schema V5:

- oui, le generateur Data actuel est suffisant pour des etudes statistiques, comparatives, longitudinales et comportementales serieuses
- oui, les principaux oublis identifies pendant les audits precedents ont bien ete corriges: `provenance`, `turnDelta`, `structuredEvents`, `commandAuditTrail`, `xpAuditTrail`, `behavioralTelemetry`, `pendingTurnTelemetry`
- non, il n'est pas exhaustif au sens absolu pour toutes les etudes imaginables

Ce qui manque encore, si l'on vise des etudes plus specialisees, est clairement localise:

- la causalite moteur atomique la plus basse
- la deliberation et les scores internes de l'IA
- les metriques dediees de transport reseau
- la telemetrie brute de micro-gestes utilisateur a haute frequence

Reserve pratique:

- le companion `build/Data/pepo.json` actuellement visible dans le workspace reste un echantillon V2
- il ne reflete donc pas a lui seul l'etat final du generateur analyse

Verdict final:

- pour l'etat autoritaire et les systemes de jeu: oui, c'est largement suffisant
- pour les deltas de turn et les evenements metier: oui
- pour l'intention joueur, les rejets de commande et la preparation de turn: oui
- pour l'orchestration LAN fonctionnelle: oui
- pour la micro-causalite moteur, la perf reseau et l'UX gestuelle brute: pas encore