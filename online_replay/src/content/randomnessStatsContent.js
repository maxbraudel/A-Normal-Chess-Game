import batchStats from "../data/randomness_statistics_500.json";

const COLORS = {
  ink: "#1f1f1f",
  graphite: "#5f5f5f",
  mist: "#d8d8d8",
  moss: "#5f6f58",
  sand: "#b7925b",
  brick: "#9b6958",
  slate: "#76869a"
};

function formatNumber(value, digits = 1) {
  return new Intl.NumberFormat("fr-FR", {
    minimumFractionDigits: digits,
    maximumFractionDigits: digits
  }).format(value);
}

function formatPercent(value, digits = 1) {
  return `${formatNumber(value, digits)} %`;
}

function formatRange(minValue, maxValue, digits = 1) {
  return `${formatNumber(minValue, digits)} a ${formatNumber(maxValue, digits)}`;
}

function sortNumericEntries(histogram) {
  return Object.entries(histogram)
    .map(([key, count]) => [Number(key), count])
    .sort((left, right) => left[0] - right[0]);
}

function sumCounts(values) {
  return values.reduce((sum, value) => sum + value, 0);
}

function firstOccupiedBucket(values) {
  return values.findIndex((value) => value > 0);
}

function lastOccupiedBucket(values) {
  for (let index = values.length - 1; index >= 0; index -= 1) {
    if (values[index] > 0) {
      return index;
    }
  }

  return -1;
}

function aggregateColumns(heatmap) {
  const columns = new Map();
  for (const [key, count] of Object.entries(heatmap)) {
    const [x] = key.split(",").map(Number);
    columns.set(x, (columns.get(x) || 0) + count);
  }

  return Array.from(columns.entries()).sort((left, right) => left[0] - right[0]);
}

function mergeHistograms(...histograms) {
  const merged = new Map();
  for (const histogram of histograms) {
    for (const [key, count] of Object.entries(histogram || {})) {
      merged.set(key, (merged.get(key) || 0) + count);
    }
  }

  return Object.fromEntries(
    Array.from(merged.entries()).sort((left, right) => Number(left[0]) - Number(right[0]))
  );
}

function baseGridOption() {
  return {
    animation: false,
    textStyle: {
      fontFamily: 'Georgia, "Times New Roman", serif',
      color: COLORS.graphite
    },
    tooltip: {
      trigger: "axis",
      backgroundColor: "rgba(255, 255, 255, 0.96)",
      borderColor: "#d8d8d8",
      borderWidth: 1,
      textStyle: {
        color: COLORS.ink,
        fontFamily: 'Georgia, "Times New Roman", serif'
      },
      axisPointer: {
        type: "shadow"
      }
    },
    legend: {
      top: 0,
      textStyle: {
        color: COLORS.graphite,
        fontFamily: 'Georgia, "Times New Roman", serif'
      }
    },
    grid: {
      top: 46,
      left: 74,
      right: 18,
      bottom: 76,
      containLabel: true
    },
    xAxis: {
      axisLine: {
        lineStyle: {
          color: "#cfcfcf"
        }
      },
      axisTick: {
        show: false
      },
      axisLabel: {
        color: COLORS.graphite
      }
    },
    yAxis: {
      splitLine: {
        lineStyle: {
          color: "#ececec"
        }
      },
      axisLine: {
        show: false
      },
      axisTick: {
        show: false
      },
      axisLabel: {
        color: COLORS.graphite
      }
    }
  };
}

function buildGroupedBarOption({ categories, series, rotateLabels = false, xAxisName, yAxisName }) {
  const option = baseGridOption();
  option.xAxis = {
    ...option.xAxis,
    type: "category",
    name: xAxisName,
    nameLocation: "middle",
    nameGap: rotateLabels ? 54 : 42,
    nameTextStyle: {
      color: COLORS.ink,
      fontFamily: 'Georgia, "Times New Roman", serif',
      fontSize: 13,
      fontWeight: 600
    },
    data: categories,
    axisLabel: {
      ...option.xAxis.axisLabel,
      interval: 0,
      rotate: rotateLabels ? 24 : 0
    }
  };
  option.yAxis = {
    ...option.yAxis,
    type: "value",
    name: yAxisName,
    nameLocation: "middle",
    nameGap: 58,
    nameRotate: 90,
    nameTextStyle: {
      color: COLORS.ink,
      fontFamily: 'Georgia, "Times New Roman", serif',
      fontSize: 13,
      fontWeight: 600
    }
  };
  option.series = series.map((entry) => ({
    name: entry.name,
    type: "bar",
    data: entry.data,
    barMaxWidth: 30,
    itemStyle: {
      color: entry.color,
      borderRadius: [3, 3, 0, 0]
    }
  }));
  return option;
}

function buildHistogramOption({ categories, values, color, rotateLabels = false, xAxisName, yAxisName }) {
  const option = baseGridOption();
  option.legend = undefined;
  option.xAxis = {
    ...option.xAxis,
    type: "category",
    name: xAxisName,
    nameLocation: "middle",
    nameGap: rotateLabels ? 58 : 42,
    nameTextStyle: {
      color: COLORS.ink,
      fontFamily: 'Georgia, "Times New Roman", serif',
      fontSize: 13,
      fontWeight: 600
    },
    data: categories,
    axisLabel: {
      ...option.xAxis.axisLabel,
      interval: 0,
      rotate: rotateLabels ? 28 : 0
    }
  };
  option.yAxis = {
    ...option.yAxis,
    type: "value",
    name: yAxisName,
    nameLocation: "middle",
    nameGap: 58,
    nameRotate: 90,
    nameTextStyle: {
      color: COLORS.ink,
      fontFamily: 'Georgia, "Times New Roman", serif',
      fontSize: 13,
      fontWeight: 600
    }
  };
  option.series = [
    {
      type: "bar",
      data: values,
      barMaxWidth: 34,
      itemStyle: {
        color,
        borderRadius: [3, 3, 0, 0]
      }
    }
  ];
  return option;
}

const run = batchStats.run;
const mapGeneration = batchStats.suites.map_generation;
const xpRewards = batchStats.suites.xp_rewards;
const chestSystem = batchStats.suites.chest_system;
const weatherSystem = batchStats.suites.weather_system;

const playerSpawnColumns = aggregateColumns(mapGeneration.heatmaps.player_spawn);
const aiSpawnColumns = aggregateColumns(mapGeneration.heatmaps.ai_spawn);
const playerSpawnColumnMap = new Map(playerSpawnColumns);
const aiSpawnColumnMap = new Map(aiSpawnColumns);

const grassBrightness = mapGeneration.histograms.terrain_brightness_by_type.Grass;
const grassBrightnessCategories = grassBrightness.map((entry) => `${entry.binStart}-${entry.binEnd}`);
const grassBrightnessValues = grassBrightness.map((entry) => entry.count);
const grassBrightnessTotal = sumCounts(grassBrightnessValues);
const grassBrightTopTwo = grassBrightness.slice(-2).reduce((sum, entry) => sum + entry.count, 0);

const terrainCoverageObserved = [
  mapGeneration.summary.dirt_coverage_percent.mean,
  mapGeneration.summary.water_coverage_percent.mean
];
const terrainCoverageTarget = [run.config.dirtCoveragePercent, run.config.waterCoveragePercent];

const waterComponentSizes = sortNumericEntries(mapGeneration.histograms.water_component_size);

const xpSourceOrder = [
  ["KillPawn", "Pion"],
  ["KillKnight", "Cavalier"],
  ["KillBishop", "Fou"],
  ["KillRook", "Tour"],
  ["KillQueen", "Reine"],
  ["DestroyBlock", "Bloc"],
  ["ArenaPerTurn", "Arène"]
];

const xpExpectedMeans = xpSourceOrder.map(([key]) => xpRewards.sources[key].expected.inputMean);
const xpObservedMeans = xpSourceOrder.map(([key]) => xpRewards.sources[key].summary.amount.mean);

const combinedChestGoldHistogram = mergeHistograms(
  chestSystem.rewardPhases.early_phase.goldAmountHistogram,
  chestSystem.rewardPhases.late_phase.goldAmountHistogram
);
const combinedChestGoldEntries = sortNumericEntries(combinedChestGoldHistogram);

const chestRewardCategories = ["Or", "Mouvement", "Construction"];
const earlyObserved = chestSystem.rewardPhases.early_phase.observedRewardTypeProbabilities;
const earlyExpected = chestSystem.rewardPhases.early_phase.expectedRewardTypeProbabilities;
const lateObserved = chestSystem.rewardPhases.late_phase.observedRewardTypeProbabilities;
const lateExpected = chestSystem.rewardPhases.late_phase.expectedRewardTypeProbabilities;

const weatherArrivalEntries = sortNumericEntries(weatherSystem.histograms.arrival_delay_turns);
const weatherActiveFrontEntries = sortNumericEntries(weatherSystem.histograms.active_front_count);
const weatherPeakCoverageEntries = sortNumericEntries(weatherSystem.histograms.visible_coverage_percent);
const spawnColumns = Array.from({ length: 50 }, (_, index) => index);
const playerSpawnValues = spawnColumns.map((index) => playerSpawnColumnMap.get(index) || 0);
const aiSpawnValues = spawnColumns.map((index) => aiSpawnColumnMap.get(index) || 0);
const dirtCoverageComparison = [run.config.dirtCoveragePercent, mapGeneration.summary.dirt_coverage_percent.mean];
const waterCoverageComparison = [run.config.waterCoveragePercent, mapGeneration.summary.water_coverage_percent.mean];

const observedDataLabel = `Donnees observees sur ${run.sampleCount} parties simulees`;

const processStatsByTitle = {
  "Choix de position des batiments publics": [
    {
      title: "Distances observees entre batiments publics et lacs",
      description:
        "Les placements publics ne sont pas seulement des tirages admissibles: le batch montre aussi la distance effective a l'eau apres generation complete du terrain, ce qui renseigne mieux la jouabilite que la seule loi uniforme conditionnelle.",
      metrics: [
        {
          label: "Mine -> lac le plus proche",
          value: `${formatNumber(mapGeneration.summary.mine_to_nearest_lake_distance.mean, 1)} cases`
        },
        {
          label: "Ferme -> lac le plus proche",
          value: `${formatNumber(mapGeneration.summary.farm_to_nearest_lake_distance.mean, 1)} cases`
        },
        {
          label: "Distance moyenne blanc-noir",
          value: `${formatNumber(mapGeneration.summary.player_ai_spawn_distance.mean, 1)} cases`
        }
      ],
      insights: [
        "Ces valeurs donnent un resume concret du placement final des structures, au lieu de laisser croire qu'un support admissible uniforme suffit a decrire toute la carte.",
        "La distance moyenne initiale entre royaumes est deja visible ici car le placement des structures et celui des spawns restent geometriquement lies au plateau genere."
      ]
    }
  ],
  "Spawn des royaumes": [
    {
      title: "Support observe des zones d'apparition blanche et noire",
      description:
        "Les deux royaumes restent confines a des bandes laterales opposees. Le batch montre leurs supports effectifs sur le meme graphe, ce qui rend la symetrie et la separation initiale beaucoup plus lisibles qu'une lecture par fiches separees.",
      metrics: [
        {
          label: "Support blanc",
          value: `${firstOccupiedBucket(playerSpawnValues)}-${lastOccupiedBucket(playerSpawnValues)}`
        },
        {
          label: "Support noir",
          value: `${firstOccupiedBucket(aiSpawnValues)}-${lastOccupiedBucket(aiSpawnValues)}`
        },
        {
          label: "Distance moyenne blanc-noir",
          value: `${formatNumber(mapGeneration.summary.player_ai_spawn_distance.mean, 1)} cases`
        }
      ],
      insights: [
        "La bonne lecture mathematique est celle d'une **uniforme conditionnelle sur un support geometrique deja contraint**, appliquee symetriquement aux deux royaumes.",
        "Les colonnes centrales restent hors support ou quasi hors support, ce qui rend la separation d'ouverture visible des le premier coup d'oeil."
      ],
      chartHeight: 310,
      chartLabel: "Répartition des colonnes d'apparition des royaumes blanc et noir",
      chartOption: buildGroupedBarOption({
        categories: spawnColumns.map(String),
        series: [
          {
            name: "Royaume blanc",
            data: playerSpawnValues,
            color: COLORS.moss
          },
          {
            name: "Royaume noir",
            data: aiSpawnValues,
            color: COLORS.sand
          }
        ],
        rotateLabels: true,
        xAxisName: "Colonne de la carte",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Recompenses d'XP": [
    {
      title: "Les moyennes XP empiriques retombent bien sur les moyennes de design",
      description:
        "Le batch confirme bien le comportement attendu de la famille normale tronquee discretisee: sur toutes les sources XP principales, les moyennes observees restent tres proches des moyennes de configuration, malgre clamp et arrondi.",
      metrics: [
        {
          label: "Pion observe / attendu",
          value: `${formatNumber(xpRewards.sources.KillPawn.summary.amount.mean, 1)} / ${xpRewards.sources.KillPawn.expected.inputMean}`
        },
        {
          label: "Tour observe / attendu",
          value: `${formatNumber(xpRewards.sources.KillRook.summary.amount.mean, 1)} / ${xpRewards.sources.KillRook.expected.inputMean}`
        },
        {
          label: "Reine observe / attendu",
          value: `${formatNumber(xpRewards.sources.KillQueen.summary.amount.mean, 1)} / ${xpRewards.sources.KillQueen.expected.inputMean}`
        }
      ],
      insights: [
        "C'est une statistique pertinente parce qu'elle valide directement la **coherence entre parametres de rapport et runtime effectif**.",
        "Les sorties moins parlantes comme les rotations de batiments ou les flips pseudo-uniformes sont volontairement laissees hors du rapport principal: elles n'apportent pas de lecture de gameplay utile."
      ],
      chartHeight: 320,
      chartLabel: "Comparaison des moyennes XP attendues et observées",
      chartOption: buildGroupedBarOption({
        categories: xpSourceOrder.map(([, label]) => label),
        xAxisName: "Source XP",
        yAxisName: "XP moyen",
        series: [
          {
            name: "Moyenne attendue",
            color: COLORS.mist,
            data: xpExpectedMeans
          },
          {
            name: "Moyenne observee",
            color: COLORS.ink,
            data: xpObservedMeans
          }
        ],
        rotateLabels: true
      })
    },
  ],
  "Montant d'or d'un coffre": [
    {
      title: "L'or des coffres partage le meme patron gaussien tronqué",
      description:
        "Le meme schema statistique reapparait pour l'or des coffres. Le batch ne le montre pas comme une loi abstraite, mais comme une distribution entiere effectivement tombee apres clamp et arrondi.",
      metrics: [
        {
          label: "Moyenne d'entree du profil or",
          value: chestSystem.expected.goldInputMean.toString()
        },
        {
          label: "Moyenne observee early",
          value: formatNumber(chestSystem.rewardPhases.early_phase.goldAmountSummary.gold_amount.mean, 1)
        },
        {
          label: "Moyenne observee late",
          value: formatNumber(chestSystem.rewardPhases.late_phase.goldAmountSummary.gold_amount.mean, 1)
        }
      ],
      insights: [
        "Dans le rapport, cette figure sert surtout a montrer que l'XP et l'or ne sont pas deux mecanismes differents: c'est **la meme famille probabiliste** avec des parametres differents.",
        run.sampleCount < 100
          ? `Le fichier integre correspond actuellement a ${run.sampleCount} simulations: la structure est bonne, mais la version finale gagnera a etre regeneree en 500 pour stabiliser l'histogramme.`
          : `Le batch actuellement integre est deja suffisamment large pour que la forme de l'histogramme soit interpretable dans le rapport.`
      ],
      chartHeight: 280,
      chartLabel: "Histogramme des montants d'or de coffre observés",
      chartOption: buildHistogramOption({
        categories: combinedChestGoldEntries.map(([value]) => String(value)),
        values: combinedChestGoldEntries.map(([, count]) => count),
        color: COLORS.sand,
        xAxisName: "Montant d'or",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Type de recompense du coffre": [
    {
      title: "Le basculement early/late des coffres est lisible",
      description:
        "Le signal le plus utile ici n'est pas une moyenne, mais le deplacement des probabilites nominales entre debut et fin de partie. Le batch montre bien que l'or perd du poids relatif quand la phase tardive s'installe.",
      metrics: [
        {
          label: "Or observe en debut de partie",
          value: formatPercent(earlyObserved.Gold * 100, 0)
        },
        {
          label: "Or observe en fin de partie",
          value: formatPercent(lateObserved.Gold * 100, 0)
        },
        {
          label: "Scenario 100 tours",
          value: `${formatNumber(chestSystem.summary.timeline_collected_chests.mean, 1)} coffres`
        }
      ],
      insights: [
        "L'illustration la plus pertinente n'est donc pas seulement le type de reward brut, mais **l'ecart entre les probabilites early et late** face aux poids configures.",
        `Dans le scenario synthetique de collecte immediate sur ${run.turnBudget} tours, le batch observe en moyenne ${formatNumber(chestSystem.summary.timeline_total_gold.mean, 1)} d'or, ${formatNumber(chestSystem.summary.timeline_total_movement_bonus.mean, 1)} points de mouvement max et ${formatNumber(chestSystem.summary.timeline_total_build_bonus.mean, 1)} points de construction max.`
      ],
      chartHeight: 300,
      chartLabel: "Comparaison des probabilités de récompense de coffre entre phase early et late",
      chartOption: buildGroupedBarOption({
        categories: chestRewardCategories,
        xAxisName: "Type de recompense",
        yAxisName: "Probabilite observee",
        series: [
          {
            name: "Debut observe",
            color: COLORS.ink,
            data: [earlyObserved.Gold, earlyObserved["Movement Points"], earlyObserved["Build Points"]]
          },
          {
            name: "Debut attendu",
            color: COLORS.mist,
            data: [earlyExpected.Gold, earlyExpected["Movement Points"], earlyExpected["Build Points"]]
          },
          {
            name: "Fin observee",
            color: COLORS.moss,
            data: [lateObserved.Gold, lateObserved["Movement Points"], lateObserved["Build Points"]]
          },
          {
            name: "Fin attendue",
            color: COLORS.sand,
            data: [lateExpected.Gold, lateExpected["Movement Points"], lateExpected["Build Points"]]
          }
        ]
      })
    }
  ],
  "Delai de reapparition d'un coffre": [
    {
      title: "Le cooldown minimal de 4 tours se voit immediatement dans la Weibull des coffres",
      description:
        "C'est une des statistiques les plus parlantes du batch: la loi continue seule ne raconte pas le gameplay. La masse au seuil de `4` tours prouve que le cooldown plancher modifie fortement la forme observee.",
      metrics: [
        {
          label: "Moyenne continue de reference",
          value: formatNumber(chestSystem.expected.spawnDelayContinuousReferenceMean, 2)
        },
        {
          label: "Moyenne observee",
          value: formatNumber(chestSystem.summary.spawn_delay_turns.mean, 2)
        },
        {
          label: "Plancher impose",
          value: `${chestSystem.expected.spawnDelayCooldownFloor} tours`
        }
      ],
      insights: [
        "Le bon message pour le rapport est donc: **la Weibull parente existe, mais la runtime est celle d'une Weibull discretisee puis bornee inferieurement**.",
        "C'est exactement le type d'ecart theorie/runtime que les consignes demandent d'illustrer par histogramme et ecart a la moyenne."
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des délais de réapparition des coffres",
      chartOption: buildHistogramOption({
        categories: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([value]) => String(value)),
        values: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([, count]) => count),
        color: COLORS.brick,
        xAxisName: "Delai (tours)",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Delai entre deux brouillards meteo": [
    {
      title: "Les inter-arrivees meteo restent compatibles avec la Gamma parametree",
      description:
        "La comparaison la plus propre ici porte sur les temps d'attente entre brouillards. L'ecart entre la moyenne empirique et la moyenne theorique reste faible dans le batch integre, ce qui valide bien le coeur du generateur temporel de meteo.",
      metrics: [
        {
          label: "Moyenne theorique",
          value: formatNumber(weatherSystem.expected.arrivalContinuousReferenceMean, 1)
        },
        {
          label: "Moyenne observee",
          value: formatNumber(weatherSystem.summary.arrival_delay_turns.mean, 1)
        },
        {
          label: "Amplitude observee",
          value: formatRange(
            weatherSystem.summary.arrival_delay_turns.min,
            weatherSystem.summary.arrival_delay_turns.max,
            0
          )
        }
      ],
      insights: [
        "C'est une statistique vraiment utile pour le rapport car elle relie directement `E[T]=kθ+m` a un histogramme de runtime lisible.",
        "Les indicateurs comme les directions cardinales sont moins prioritaires ici: ils montrent surtout l'absence de biais, pas une structure gameplay profonde."
      ],
      chartHeight: 300,
      chartLabel: "Histogramme des délais d'arrivée des brouillards météo",
      chartOption: buildHistogramOption({
        categories: weatherArrivalEntries.map(([value]) => String(value)),
        values: weatherArrivalEntries.map(([, count]) => count),
        color: COLORS.slate,
        xAxisName: "Delai d'arrivee (tours)",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Duree visible d'un brouillard": [
    {
      title: "Les brouillards se chevauchent parfois, mais l'etat dominant reste 0 ou 1 brouillard actif",
      description:
        "Tu voulais explicitement voir le nombre de nuages simultanes. Cette distribution est une excellente figure de rapport parce qu'elle capture une consequence gameplay immediate de la dynamique temporelle, sans dependre des joueurs.",
      metrics: [
        {
          label: "Moyenne de brouillards actifs par pas",
          value: formatNumber(weatherSystem.summary.active_front_count_per_step.mean, 2)
        },
        {
          label: "Maximum observe par monde",
          value: formatNumber(weatherSystem.summary.max_active_fronts_per_world.max, 0)
        },
        {
          label: "Moyenne de brouillards apparus",
          value: formatNumber(weatherSystem.summary.spawned_fronts_per_world.mean, 1)
        }
      ],
      insights: [
        `Dans l'export integre, le chevauchement a deux brouillards existe deja (${weatherSystem.histograms.active_front_count[2] || 0} pas observes), mais reste nettement minoritaire face aux etats 0 et 1.`,
        "C'est un bon exemple de statistique emergente qui reste pourtant **entierement independante des actions de joueurs**."
      ],
      chartHeight: 290,
      chartLabel: "Distribution du nombre de brouillards météo simultanément actifs",
      chartOption: buildHistogramOption({
        categories: weatherActiveFrontEntries.map(([value]) => String(value)),
        values: weatherActiveFrontEntries.map(([, count]) => count),
        color: COLORS.moss,
        xAxisName: "Brouillards actifs",
        yAxisName: "Pas simules"
      })
    }
  ],
  "Luminosite de l'herbe": [
    {
      title: "La Beta de luminosite pousse presque toute l'herbe vers les bins clairs",
      description:
        "La loi `Beta(7,2)` n'est pas seulement une formule du rapport: le batch montre qu'elle concentre effectivement la masse dans les classes de luminosite les plus elevees apres remappage runtime.",
      metrics: [
        {
          label: "Part des deux bins les plus clairs",
          value: formatPercent((grassBrightTopTwo / grassBrightnessTotal) * 100, 1)
        },
        {
          label: "Masse totale observee",
          value: new Intl.NumberFormat("fr-FR").format(grassBrightnessTotal)
        },
        {
          label: "Bins sombres utilises",
          value: "Quasi nuls"
        }
      ],
      insights: [
        "Cette figure est pertinente parce qu'elle montre la consequence visuelle concrete d'une variable mathematiquement bornee dans `[0,1]` avant remappage en luminance.",
        "Le rapport peut donc illustrer la Beta par un histogramme vraiment interpretable, pas seulement par la densite theorique."
      ],
      chartHeight: 300,
      chartLabel: "Histogramme des niveaux de luminosité de l'herbe",
      chartOption: buildHistogramOption({
        categories: grassBrightnessCategories,
        values: grassBrightnessValues,
        color: COLORS.moss,
        rotateLabels: true,
        xAxisName: "Bin de luminosite",
        yAxisName: "Cellules"
      })
    }
  ],
  "Champ spatial de la terre": [
    {
      title: "La couverture finale de la terre est inferieure a la cible nominale",
      description:
        "Le pourcentage configure de la terre ne se retrouve pas tel quel sur la carte finale. La pipeline procedurale applique ensuite des contraintes de composantes et de connectivite qui deforment la couverture initialement visee.",
      metrics: [
        {
          label: "Couverture ciblee",
          value: formatPercent(run.config.dirtCoveragePercent, 0)
        },
        {
          label: "Couverture observee",
          value: formatPercent(mapGeneration.summary.dirt_coverage_percent.mean, 1)
        },
        {
          label: "Ecart absolu",
          value: formatPercent(run.config.dirtCoveragePercent - mapGeneration.summary.dirt_coverage_percent.mean, 1)
        }
      ],
      insights: [
        "Autrement dit: **la variable de configuration est une intention de generation, pas une promesse de couverture finale**.",
        "C'est un vrai resultat empirique utile, pas un simple rappel de parametre de config."
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture la terre ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observee"],
        values: dirtCoverageComparison,
        color: COLORS.brick,
        xAxisName: "Mesure",
        yAxisName: "Pourcentage",
      })
    }
  ],
  "Champ spatial de l'eau": [
    {
      title: "La couverture finale d'eau est inferieure a la cible nominale",
      description:
        "Comme pour la terre, la couverture eau finale subit les post-traitements topologiques de la generation. La valeur nominale sert de point de depart, pas de valeur finale garantie.",
      metrics: [
        {
          label: "Couverture ciblee",
          value: formatPercent(run.config.waterCoveragePercent, 0)
        },
        {
          label: "Couverture observee",
          value: formatPercent(mapGeneration.summary.water_coverage_percent.mean, 1)
        },
        {
          label: "Composantes moyennes",
          value: formatNumber(mapGeneration.summary.water_component_count.mean, 1)
        }
      ],
      insights: [
        "Le rapport doit donc distinguer la variable de commande et la morphologie finale effectivement jouee.",
        "L'eau est un bon exemple de champ procedural pour lequel le resultat topologique compte plus que la simple densite brute."
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture d'eau ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observee"],
        values: waterCoverageComparison,
        color: COLORS.slate,
        xAxisName: "Mesure",
        yAxisName: "Pourcentage"
      })
    },
    {
      title: "La taille des composantes d'eau raconte mieux la forme des lacs qu'un simple pourcentage",
      description:
        "Ici, l'information importante n'est pas seulement la couverture totale, mais la fragmentation: combien de poches d'eau apparaissent, et quelle taille prend la plus grosse composante.",
      metrics: [
        {
          label: "Plus grande composante moyenne",
          value: `${formatNumber(mapGeneration.summary.largest_water_component.mean, 1)} cellules`
        },
        {
          label: "Maximum observe",
          value: `${formatNumber(mapGeneration.summary.largest_water_component.max, 0)} cellules`
        },
        {
          label: "Moyenne des lacs proches des mines",
          value: `${formatNumber(mapGeneration.summary.mine_to_nearest_lake_distance.mean, 1)} cases`
        }
      ],
      insights: [
        "Cette statistique est bien plus pertinente qu'un flip mask uniforme, parce qu'elle decrit directement la morphologie jouable du terrain.",
        "Elle illustre bien la phrase du rapport selon laquelle un champ procedural doit etre resume par des composantes, de la rugosite et de la topologie, pas par une loi scalaire i.i.d. fictive."
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des tailles de composantes d'eau",
      chartOption: buildHistogramOption({
        categories: waterComponentSizes.map(([value]) => String(value)),
        values: waterComponentSizes.map(([, count]) => count),
        color: COLORS.slate,
        xAxisName: "Taille de composante",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Bruit de contour du brouillard": [
    {
      title: "La couverture visible finale des brouillards depasse parfois la consigne nominale",
      description:
        "La couverture ciblee de la meteo est parametree entre 5 % et 20 %, mais la forme reelle du brouillard apres deformation de contour peut pousser la couverture visible un peu au-dela de la borne haute nominale.",
      metrics: [
        {
          label: "Couverture de pic moyenne",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.mean, 1)
        },
        {
          label: "q90 observe",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.q90, 1)
        },
        {
          label: "Maximum observe",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.max, 1)
        }
      ],
      insights: [
        "Le bon commentaire a mettre dans le rapport est donc: la couverture uniforme config ne decrit que l'aire cible brute; la **forme procedurale finale** peut la deformer sensiblement.",
        "Cela justifie de traiter le brouillard comme un champ spatial correle plutot que comme une simple variable d'aire."
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des couvertures visibles maximales des brouillards meteo",
      chartOption: buildHistogramOption({
        categories: weatherPeakCoverageEntries.map(([value]) => String(value)),
        values: weatherPeakCoverageEntries.map(([, count]) => count),
        color: COLORS.brick,
        xAxisName: "Couverture visible (%)",
        yAxisName: "Occurrences"
      })
    }
  ]
};

export const randomnessStatsReport = {
  run,
  overviewBlocks: [
    {
      eyebrow: "Batch statistique",
      title: "Jeu de donnees integre au rapport",
      description:
        "Le rapport n'integre pas toutes les sorties du batch: il ne garde que les statistiques qui apportent une information mathematique ou gameplay claire. Les signaux triviaux, constants ou purement cosmetiques sont laisses de cote.",
      metrics: [
        {
          label: "Seeds integrees",
          value: String(run.sampleCount)
        },
        {
          label: "Budget temporel",
          value: `${run.turnBudget} tours`
        },
        {
          label: "Seed de base",
          value: String(run.baseSeed)
        }
      ],
      insights: [
        "Les figures retenues sont celles qui montrent soit un **ecart theorie/runtime** (couverture reelle, cooldown, discretisation), soit une **verification empirique utile** (moyennes XP, Gamma meteo).",
        "Les sorties ecartes du rapport principal sont celles qui n'ajoutent presque rien a l'analyse: nombre de cellules valides constant, flips de terrain quasi-uniformes, rotations de batiments publics et categories sans signal lisible.",
        run.sampleCount < 100
          ? `Le dernier export disponible integre actuellement ${run.sampleCount} simulations. La page est deja structuree correctement, mais la version finale devra etre rafraichie avec un batch 500 pour stabiliser les conclusions.`
          : `Le volume d'echantillons integre est deja suffisant pour donner des figures stables et directement reutilisables dans le rapport.`
      ]
    }
  ],
  observedDataLabel,
  processStatsByTitle
};