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

const observedDataLabel = `Données observées sur ${run.sampleCount} parties simulées`;

const processStatsByTitle = {
  "Choix de position des bâtiments publics": [
    {
      title: "Distances observées entre bâtiments publics et lacs",
      description:
        "Les placements publics ne sont pas seulement des tirages admissibles: le batch montre aussi la distance effective à l'eau après génération complète du terrain, ce qui renseigne mieux la jouabilité que la seule loi uniforme conditionnelle.",
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
        "Ces valeurs donnent un résumé concret du placement final des structures, au lieu de laisser croire qu'un support admissible uniforme suffit à décrire toute la carte.",
        "La distance moyenne initiale entre royaumes est déjà visible ici car le placement des structures et celui des apparitions restent géométriquement liés au plateau généré."
      ]
    }
  ],
  "Apparition des royaumes": [
    {
      title: "Support observé des zones d'apparition blanche et noire",
      description:
        "Les deux royaumes restent confinés à des bandes latérales opposées. Le batch montre leurs supports effectifs sur le même graphe, ce qui rend la symétrie et la séparation initiale beaucoup plus lisibles qu'une lecture par fiches séparées.",
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
        "La bonne lecture mathématique est celle d'une **uniforme conditionnelle sur un support géométrique déjà contraint**, appliquée symétriquement aux deux royaumes.",
        "Les colonnes centrales restent hors support ou quasi hors support, ce qui rend la séparation d'ouverture visible dès le premier coup d'œil."
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
  "Récompenses d'XP": [
    {
      title: "Les moyennes XP empiriques retombent bien sur les moyennes de conception",
      description:
        "Le batch confirme bien le comportement attendu de la famille normale tronquée discrétisée: sur toutes les sources XP principales, les moyennes observées restent très proches des moyennes de configuration, malgré clamp et arrondi.",
      metrics: [
        {
          label: "Pion observé / attendu",
          value: `${formatNumber(xpRewards.sources.KillPawn.summary.amount.mean, 1)} / ${xpRewards.sources.KillPawn.expected.inputMean}`
        },
        {
          label: "Tour observé / attendu",
          value: `${formatNumber(xpRewards.sources.KillRook.summary.amount.mean, 1)} / ${xpRewards.sources.KillRook.expected.inputMean}`
        },
        {
          label: "Reine observée / attendu",
          value: `${formatNumber(xpRewards.sources.KillQueen.summary.amount.mean, 1)} / ${xpRewards.sources.KillQueen.expected.inputMean}`
        }
      ],
      insights: [
        "C'est une statistique pertinente parce qu'elle valide directement la **cohérence entre paramètres théoriques et runtime effectif**.",
        "Les rotations de bâtiments ou les flips pseudo-uniformes apportent beaucoup moins d'information sur la partie que ces moyennes XP."
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
            name: "Moyenne observée",
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
      title: "L'or des coffres partage le même patron gaussien tronqué",
      description:
        "Le même schéma statistique réapparaît pour l'or des coffres. Le batch ne le montre pas comme une loi abstraite, mais comme une distribution entière effectivement tombée après clamp et arrondi.",
      metrics: [
        {
          label: "Moyenne d'entrée du profil or",
          value: chestSystem.expected.goldInputMean.toString()
        },
        {
          label: "Moyenne observée early",
          value: formatNumber(chestSystem.rewardPhases.early_phase.goldAmountSummary.gold_amount.mean, 1)
        },
        {
          label: "Moyenne observée late",
          value: formatNumber(chestSystem.rewardPhases.late_phase.goldAmountSummary.gold_amount.mean, 1)
        }
      ],
      insights: [
        "Cette figure montre surtout que l'XP et l'or ne sont pas deux mécanismes différents: c'est **la même famille probabiliste** avec des paramètres différents.",
        run.sampleCount < 100
          ? `Le fichier intégré correspond actuellement à ${run.sampleCount} simulations: la structure est bonne, mais la version finale gagnera à être régénérée en 500 pour stabiliser l'histogramme.`
          : `Le batch actuellement intégré est déjà suffisamment large pour que la forme de l'histogramme soit interprétable.`
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
  "Type de récompense du coffre": [
    {
      title: "Le basculement early/late des coffres est lisible",
      description:
        "Le signal le plus utile ici n'est pas une moyenne, mais le déplacement des probabilités nominales entre début et fin de partie. Le batch montre bien que l'or perd du poids relatif quand la phase tardive s'installe.",
      metrics: [
        {
          label: "Or observé en début de partie",
          value: formatPercent(earlyObserved.Gold * 100, 0)
        },
        {
          label: "Or observé en fin de partie",
          value: formatPercent(lateObserved.Gold * 100, 0)
        },
        {
          label: "Scénario 100 tours",
          value: `${formatNumber(chestSystem.summary.timeline_collected_chests.mean, 1)} coffres`
        }
      ],
      insights: [
        "L'illustration la plus pertinente n'est donc pas seulement le type de récompense brut, mais **l'écart entre les probabilités early et late** face aux poids configurés.",
        `Dans le scénario synthétique de collecte immédiate sur ${run.turnBudget} tours, le batch observe en moyenne ${formatNumber(chestSystem.summary.timeline_total_gold.mean, 1)} d'or, ${formatNumber(chestSystem.summary.timeline_total_movement_bonus.mean, 1)} points de mouvement max et ${formatNumber(chestSystem.summary.timeline_total_build_bonus.mean, 1)} points de construction max.`
      ],
      chartHeight: 300,
      chartLabel: "Comparaison des probabilités de récompense de coffre entre phase early et late",
      chartOption: buildGroupedBarOption({
        categories: chestRewardCategories,
        xAxisName: "Type de récompense",
        yAxisName: "Probabilité observée",
        series: [
          {
            name: "Début observé",
            color: COLORS.ink,
            data: [earlyObserved.Gold, earlyObserved["Movement Points"], earlyObserved["Build Points"]]
          },
          {
            name: "Début attendu",
            color: COLORS.mist,
            data: [earlyExpected.Gold, earlyExpected["Movement Points"], earlyExpected["Build Points"]]
          },
          {
            name: "Fin observée",
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
  "Délai de réapparition d'un coffre": [
    {
      title: "Le délai minimal de 4 tours se voit immédiatement dans la Weibull des coffres",
      description:
        "C'est une des statistiques les plus parlantes du batch: la loi continue seule ne raconte pas le gameplay. La masse au seuil de `4` tours prouve que le cooldown plancher modifie fortement la forme observée.",
      metrics: [
        {
          label: "Moyenne continue de référence",
          value: formatNumber(chestSystem.expected.spawnDelayContinuousReferenceMean, 2)
        },
        {
          label: "Moyenne observée",
          value: formatNumber(chestSystem.summary.spawn_delay_turns.mean, 2)
        },
        {
          label: "Plancher imposé",
          value: `${chestSystem.expected.spawnDelayCooldownFloor} tours`
        }
      ],
      insights: [
        "**La Weibull parente existe, mais la loi effective est celle d'une Weibull discrétisée puis bornée inférieurement**.",
        "L'histogramme et l'écart à la moyenne rendent cet écart théorie/runtime directement visible."
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des délais de réapparition des coffres",
      chartOption: buildHistogramOption({
        categories: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([value]) => String(value)),
        values: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([, count]) => count),
        color: COLORS.brick,
        xAxisName: "Délai (tours)",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Délai entre deux brouillards": [
    {
      title: "Les inter-arrivées météo restent compatibles avec la Gamma paramétrée",
      description:
        "La comparaison la plus propre ici porte sur les temps d'attente entre brouillards. L'écart entre la moyenne empirique et la moyenne théorique reste faible dans le batch intégré, ce qui valide bien le cœur du générateur temporel de météo.",
      metrics: [
        {
          label: "Moyenne théorique",
          value: formatNumber(weatherSystem.expected.arrivalContinuousReferenceMean, 1)
        },
        {
          label: "Moyenne observée",
          value: formatNumber(weatherSystem.summary.arrival_delay_turns.mean, 1)
        },
        {
          label: "Amplitude observée",
          value: formatRange(
            weatherSystem.summary.arrival_delay_turns.min,
            weatherSystem.summary.arrival_delay_turns.max,
            0
          )
        }
      ],
      insights: [
        "Cette statistique relie directement `E[T]=kθ+m` à un histogramme de runtime lisible.",
        "Les indicateurs comme les directions cardinales sont moins prioritaires ici: ils montrent surtout l'absence de biais, pas une structure gameplay profonde."
      ],
      chartHeight: 300,
      chartLabel: "Histogramme des délais d'arrivée des brouillards météo",
      chartOption: buildHistogramOption({
        categories: weatherArrivalEntries.map(([value]) => String(value)),
        values: weatherArrivalEntries.map(([, count]) => count),
        color: COLORS.slate,
        xAxisName: "Délai d'arrivée (tours)",
        yAxisName: "Occurrences"
      })
    }
  ],
  "Durée visible d'un brouillard": [
    {
      title: "Les brouillards se chevauchent parfois, mais l'état dominant reste 0 ou 1 brouillard actif",
      description:
        "La distribution du nombre de nuages simultanés capture une conséquence gameplay immédiate de la dynamique temporelle, sans dépendre des joueurs.",
      metrics: [
        {
          label: "Moyenne de brouillards actifs par pas",
          value: formatNumber(weatherSystem.summary.active_front_count_per_step.mean, 2)
        },
        {
          label: "Maximum observé par monde",
          value: formatNumber(weatherSystem.summary.max_active_fronts_per_world.max, 0)
        },
        {
          label: "Moyenne de brouillards apparus",
          value: formatNumber(weatherSystem.summary.spawned_fronts_per_world.mean, 1)
        }
      ],
      insights: [
        `Dans l'export intégré, le chevauchement à deux brouillards existe déjà (${weatherSystem.histograms.active_front_count[2] || 0} pas observés), mais reste nettement minoritaire face aux états 0 et 1.`,
        "C'est un bon exemple de statistique émergente qui reste pourtant **entièrement indépendante des actions des joueurs**."
      ],
      chartHeight: 290,
      chartLabel: "Distribution du nombre de brouillards météo simultanément actifs",
      chartOption: buildHistogramOption({
        categories: weatherActiveFrontEntries.map(([value]) => String(value)),
        values: weatherActiveFrontEntries.map(([, count]) => count),
        color: COLORS.moss,
        xAxisName: "Brouillards actifs",
        yAxisName: "Pas simulés"
      })
    }
  ],
  "Luminosité de l'herbe": [
    {
      title: "La Beta de luminosité pousse presque toute l'herbe vers les classes claires",
      description:
        "Le batch montre qu'elle concentre effectivement la masse dans les classes de luminosité les plus élevées après remappage runtime.",
      metrics: [
        {
          label: "Part des deux classes les plus claires",
          value: formatPercent((grassBrightTopTwo / grassBrightnessTotal) * 100, 1)
        },
        {
          label: "Masse totale observée",
          value: new Intl.NumberFormat("fr-FR").format(grassBrightnessTotal)
        },
        {
          label: "Classes sombres utilisées",
          value: "Quasi nuls"
        }
      ],
      insights: [
        "Cette figure est pertinente parce qu'elle montre la conséquence visuelle concrète d'une variable mathématiquement bornée dans `[0,1]` avant remappage en luminance.",
        "La Beta se lit ici sur un histogramme vraiment interprétable, pas seulement sur la densité théorique."
      ],
      chartHeight: 300,
      chartLabel: "Histogramme des niveaux de luminosité de l'herbe",
      chartOption: buildHistogramOption({
        categories: grassBrightnessCategories,
        values: grassBrightnessValues,
        color: COLORS.moss,
        rotateLabels: true,
        xAxisName: "Classe de luminosité",
        yAxisName: "Cellules"
      })
    }
  ],
  "Champ spatial de la terre": [
    {
      title: "La couverture finale de la terre est inférieure à la cible nominale",
      description:
        "Le pourcentage configuré de la terre ne se retrouve pas tel quel sur la carte finale. Le pipeline procédural applique ensuite des contraintes de composantes et de connectivité qui déforment la couverture initialement visée.",
      metrics: [
        {
          label: "Couverture ciblée",
          value: formatPercent(run.config.dirtCoveragePercent, 0)
        },
        {
          label: "Couverture observée",
          value: formatPercent(mapGeneration.summary.dirt_coverage_percent.mean, 1)
        },
        {
          label: "Écart absolu",
          value: formatPercent(run.config.dirtCoveragePercent - mapGeneration.summary.dirt_coverage_percent.mean, 1)
        }
      ],
      insights: [
        "Autrement dit: **la variable de configuration est une intention de génération, pas une promesse de couverture finale**.",
        "C'est un vrai résultat empirique utile, pas un simple rappel de paramètre de config."
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture de la terre ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observée"],
        values: dirtCoverageComparison,
        color: COLORS.brick,
        xAxisName: "Mesure",
        yAxisName: "Pourcentage",
      })
    }
  ],
  "Champ spatial de l'eau": [
    {
      title: "La couverture finale d'eau est inférieure à la cible nominale",
      description:
        "Comme pour la terre, la couverture eau finale subit les post-traitements topologiques de la génération. La valeur nominale sert de point de départ, pas de valeur finale garantie.",
      metrics: [
        {
          label: "Couverture ciblée",
          value: formatPercent(run.config.waterCoveragePercent, 0)
        },
        {
          label: "Couverture observée",
          value: formatPercent(mapGeneration.summary.water_coverage_percent.mean, 1)
        },
        {
          label: "Composantes moyennes",
          value: formatNumber(mapGeneration.summary.water_component_count.mean, 1)
        }
      ],
      insights: [
        "La variable de commande ne se confond donc pas avec la morphologie finale effectivement jouée.",
        "L'eau est un bon exemple de champ procédural pour lequel le résultat topologique compte plus que la simple densité brute."
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture d'eau ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observée"],
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
          label: "Maximum observé",
          value: `${formatNumber(mapGeneration.summary.largest_water_component.max, 0)} cellules`
        },
        {
          label: "Moyenne des lacs proches des mines",
          value: `${formatNumber(mapGeneration.summary.mine_to_nearest_lake_distance.mean, 1)} cases`
        }
      ],
      insights: [
        "Cette statistique est bien plus pertinente qu'un masque uniforme, parce qu'elle décrit directement la morphologie jouable du terrain.",
        "Elle illustre bien la phrase selon laquelle un champ procédural doit être résumé par des composantes, de la rugosité et de la topologie, pas par une loi scalaire i.i.d. fictive."
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
      title: "La couverture visible finale des brouillards dépasse parfois la consigne nominale",
      description:
        "La couverture configurée de la météo est paramétrée entre 5 % et 20 %, mais la forme réelle du brouillard après déformation de contour peut pousser la couverture visible un peu au-delà de la borne haute nominale.",
      metrics: [
        {
          label: "Couverture de pic moyenne",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.mean, 1)
        },
        {
          label: "q90 observé",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.q90, 1)
        },
        {
          label: "Maximum observé",
          value: formatPercent(weatherSystem.summary.isolated_peak_visible_coverage_percent.max, 1)
        }
      ],
      insights: [
        "La couverture uniforme de config ne décrit que l'aire cible brute; la **forme procédurale finale** peut la déformer sensiblement.",
        "Cela justifie de traiter le brouillard comme un champ spatial corrélé plutôt que comme une simple variable d'aire."
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des couvertures visibles maximales des brouillards météo",
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
      title: "Jeu de données statistique intégré",
      description:
        "Parmi toutes les sorties du batch, seules certaines apportent une information mathématique ou gameplay claire. Les signaux triviaux, constants ou purement cosmétiques sont laissés de côté.",
      metrics: [
        {
          label: "Seeds intégrées",
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
        "Les figures retenues sont celles qui montrent soit un **écart théorie/runtime** (couverture réelle, cooldown, discrétisation), soit une **vérification empirique utile** (moyennes XP, Gamma météo).",
        "Les sorties les moins informatives sont celles qui n'ajoutent presque rien à l'analyse: nombre de cellules valides constant, flips de terrain quasi-uniformes, rotations de bâtiments publics et catégories sans signal lisible.",
        run.sampleCount < 100
          ? `Le dernier export disponible intégré actuellement ${run.sampleCount} simulations. La page est déjà structurée correctement, mais la version finale devra être rafraîchie avec un batch 500 pour stabiliser les conclusions.`
          : `Le volume d'échantillons intégré est déjà suffisant pour donner des figures stables et directement exploitables.`
      ]
    }
  ],
  observedDataLabel,
  processStatsByTitle
};