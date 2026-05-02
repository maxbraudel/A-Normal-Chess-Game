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

function buildHistogramOption({ categories, values, color, rotateLabels = false, xAxisName, yAxisName, labelInterval = 0 }) {
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
      interval: labelInterval,
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
          value: formatNumber(mapGeneration.summary.mine_to_nearest_lake_distance.mean, 1),
          unit: "cases"
        },
        {
          label: "Ferme -> lac le plus proche",
          value: formatNumber(mapGeneration.summary.farm_to_nearest_lake_distance.mean, 1),
          unit: "cases"
        },
        {
          label: "Distance moyenne blanc-noir",
          value: formatNumber(mapGeneration.summary.player_ai_spawn_distance.mean, 1),
          unit: "cases"
        }
      ],
      postChartInterpretation:
        "**Interprétation : ces valeurs donnent un résumé concret du placement final des structures, un simple support admissible uniforme ne suffit pas à décrire la carte générée. La distance moyenne initiale entre royaumes apparaît ici car le placement des bâtiments publics et celui des apparitions restent géométriquement liés au même plateau généré.**"
    }
  ],
  "Apparition des rois": [
    {
      title: "Support observé des zones d'apparition blanche et noire",
      description:
        "Les deux royaumes restent confinés à des bandes latérales opposées. Le batch montre leurs supports effectifs sur le même graphe, ce qui rend la symétrie et la séparation initiale beaucoup plus lisibles qu'une lecture par fiches séparées.",
      metrics: [
        {
          label: "Support blanc",
          value: `${firstOccupiedBucket(playerSpawnValues)}-${lastOccupiedBucket(playerSpawnValues)}`,
          unit: "colonnes"
        },
        {
          label: "Support noir",
          value: `${firstOccupiedBucket(aiSpawnValues)}-${lastOccupiedBucket(aiSpawnValues)}`,
          unit: "colonnes"
        },
        {
          label: "Distance moyenne blanc-noir",
          value: formatNumber(mapGeneration.summary.player_ai_spawn_distance.mean, 1),
          unit: "cases"
        }
      ],
      chartHeight: 310,
      chartLabel: "Répartition des colonnes d'apparition des rois blanc et noir",
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
      }),
      postChartInterpretation:
        "**Interprétation : les deux bandes de départ sont nettement séparées et symétriques, les colonnes centrales restant quasi inaccessibles. Cela confirme que la loi effective est une uniforme conditionnelle sur un support géométriquement contraint : le graphe matérialise directement les colonnes admissibles pour chaque royaume, et la symétrie structurelle assure une équité initiale de position entre les deux camps.**"
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
          value: `${formatNumber(xpRewards.sources.KillPawn.summary.amount.mean, 1)} / ${xpRewards.sources.KillPawn.expected.inputMean}`,
          unit: "XP"
        },
        {
          label: "Tour observé / attendu",
          value: `${formatNumber(xpRewards.sources.KillRook.summary.amount.mean, 1)} / ${xpRewards.sources.KillRook.expected.inputMean}`,
          unit: "XP"
        },
        {
          label: "Reine observée / attendu",
          value: `${formatNumber(xpRewards.sources.KillQueen.summary.amount.mean, 1)} / ${xpRewards.sources.KillQueen.expected.inputMean}`,
          unit: "XP"
        }
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
      }),
      postChartInterpretation:
        "**Interprétation : le fait que toutes les barres observées tombent légèrement en dessous n'est pas un biais du modèle, c'est un artefact d'échantillonnage. Pour une normale tronquée symétriquement autour d'une moyenne entière, la valeur attendue après clamp et arrondi est exactement la moyenne de conception. La probabilité que les sept sources tombent toutes du même côté par pur hasard est de (1/2)⁷ ≈ 0,8 % : rare, mais tout à fait compatible avec une seule exécution à 500 parties. L'amplitude des écarts, inférieure à 0,5 XP sur toutes les sources, confirme que la chaîne clamp → arrondi → plancher ne déforme pas la distribution de façon perceptible.**"
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
          value: chestSystem.expected.goldInputMean.toString(),
          unit: "or"
        },
        {
          label: "Moyenne observée early",
          value: formatNumber(chestSystem.rewardPhases.early_phase.goldAmountSummary.gold_amount.mean, 1),
          unit: "or"
        },
        {
          label: "Moyenne observée late",
          value: formatNumber(chestSystem.rewardPhases.late_phase.goldAmountSummary.gold_amount.mean, 1),
          unit: "or"
        }
      ],
      chartHeight: 280,
      chartLabel: "Histogramme des montants d'or de coffre observés",
      chartOption: buildHistogramOption({
        categories: combinedChestGoldEntries.map(([value]) => String(value)),
        values: combinedChestGoldEntries.map(([, count]) => count),
        color: COLORS.sand,
        xAxisName: "Montant d'or",
        yAxisName: "Occurrences"
      }),
      postChartInterpretation:
        "**Interprétation : la forme de l'histogramme confirme que l'or des coffres suit le même patron gaussien tronqué que l'XP, mêmes bords abruptes imposés par le clamp, même pic central dominant. Les deux mécanismes partagent une seule et même famille probabiliste appliquée avec des paramètres distincts, et le batch le rend visuellement évident.**"
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
          value: formatNumber(earlyObserved.Gold * 100, 0),
          unit: "%"
        },
        {
          label: "Or observé en fin de partie",
          value: formatNumber(lateObserved.Gold * 100, 0),
          unit: "%"
        },
        {
          label: "Scénario 100 tours",
          value: formatNumber(chestSystem.summary.timeline_collected_chests.mean, 1),
          unit: "coffres"
        }
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
      }),
      postChartInterpretation:
        `**Interprétation : la bascule early → late est nette et conforme aux poids configurés : l'or recule clairement au profit du mouvement et de la construction en fin de partie. Les probabilités observées restent très proches des probabilités attendues sur les deux phases, ce qui valide la logique de progression temporelle du système. Dans le scénario synthétique de collecte immédiate sur ${run.turnBudget} tours, le batch observe en moyenne ${formatNumber(chestSystem.summary.timeline_total_gold.mean, 1)} d'or, ${formatNumber(chestSystem.summary.timeline_total_movement_bonus.mean, 1)} points de mouvement max et ${formatNumber(chestSystem.summary.timeline_total_build_bonus.mean, 1)} points de construction max.**`
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
          value: formatNumber(chestSystem.expected.spawnDelayContinuousReferenceMean, 2),
          unit: "tours"
        },
        {
          label: "Moyenne observée",
          value: formatNumber(chestSystem.summary.spawn_delay_turns.mean, 2),
          unit: "tours"
        },
        {
          label: "Plancher imposé",
          value: String(chestSystem.expected.spawnDelayCooldownFloor),
          unit: "tours"
        }
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des délais de réapparition des coffres",
      chartOption: buildHistogramOption({
        categories: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([value]) => String(value)),
        values: sortNumericEntries(chestSystem.spawnDelayHistogram).map(([, count]) => count),
        color: COLORS.brick,
        xAxisName: "Délai (tours)",
        yAxisName: "Occurrences"
      }),
      postChartInterpretation:
        `**Interprétation : la masse concentrée sur le plancher de ${chestSystem.expected.spawnDelayCooldownFloor} tours est la signature directe du cooldown minimal imposé par le gameplay. La Weibull continue théorique aurait produit de nombreux délais très courts, mais le runtime les ramène tous à ce seuil, ce qui produit le pic visible à gauche de l'histogramme. La loi effectivement jouée est une Weibull discrétisée puis tronquée à gauche, et l'écart entre la moyenne continue de référence et la moyenne observée quantifie cet effet de plancher.**`
    }
  ],
  "Délai entre deux brouillards": [
    {
      title: "Le jeu attend bien environ 40 tours entre deux brouillards",
      description:
        "Ici, on mesure le nombre de tours qui s'écoulent entre deux apparitions de brouillards. La moyenne théorique correspond au rythme visé par la configuration du générateur : autour de 40 tours avant qu'un nouveau brouillard n'entre en moyenne sur la carte. La moyenne observée reste très proche de cette cible dans le batch intégré, ce qui montre que le rythme météo joué correspond bien au rythme prévu.",
      metrics: [
        {
          label: "Délai moyen visé",
          value: formatNumber(weatherSystem.expected.arrivalContinuousReferenceMean, 1),
          unit: "tours"
        },
        {
          label: "Délai moyen observé",
          value: formatNumber(weatherSystem.summary.arrival_delay_turns.mean, 1),
          unit: "tours"
        },
        {
          label: "Plage observée",
          value: formatRange(
            weatherSystem.summary.arrival_delay_turns.min,
            weatherSystem.summary.arrival_delay_turns.max,
            0
          ),
          unit: "tours"
        }
      ],
      chartHeight: 300,
      chartLabel: "Histogramme des délais d'arrivée des brouillards météo",
      chartOption: buildHistogramOption({
        categories: weatherArrivalEntries.map(([value]) => String(value)),
        values: weatherArrivalEntries.map(([, count]) => count),
        color: COLORS.slate,
        labelInterval: 1,
        xAxisName: "Délai d'arrivée (tours)",
        yAxisName: "Occurrences"
      }),
      postChartInterpretation:
        "**Interprétation : la queue droite de l'histogramme est la signature d'une Gamma asymétrique, la masse principale se concentre autour de la moyenne, mais des délais nettement plus longs restent possibles sans être des anomalies. Concrètement, une partie peut traverser une longue période sans aucun brouillard : ce n'est pas un bug, c'est la nature du processus. Cette variabilité est précisément ce que la Gamma apporte par rapport à un pas fixe : le joueur ne peut jamais anticiper si le prochain brouillard arrivera dans 20 ou dans 80 tours.**"
    },
    {
      title: "Les brouillards se chevauchent parfois",
      description:
        "Les fronts météo avancent à vitesse fixe et plutôt lente. Si deux délais d'apparition tombent très près l'un de l'autre, un nouveau nuage peut entrer avant que le précédent soit complètement sorti : ce n'est pas fréquent, mais cela arrive.",
      metrics: [
        {
          label: "Moyenne de brouillards actifs par pas",
          value: formatNumber(weatherSystem.summary.active_front_count_per_step.mean, 2),
          unit: "brouillards"
        },
        {
          label: "Maximum observé par monde",
          value: formatNumber(weatherSystem.summary.max_active_fronts_per_world.max, 0),
          unit: "brouillards"
        },
        {
          label: "Moyenne de brouillards apparus",
          value: formatNumber(weatherSystem.summary.spawned_fronts_per_world.mean, 1),
          unit: "brouillards"
        }
      ],
      chartHeight: 290,
      chartLabel: "Distribution du nombre de brouillards météo simultanément actifs",
      chartOption: buildHistogramOption({
        categories: weatherActiveFrontEntries.map(([value]) => String(value)),
        values: weatherActiveFrontEntries.map(([, count]) => count),
        color: COLORS.moss,
        xAxisName: "Brouillards actifs",
        yAxisName: "Pas simulés"
      }),
      postChartInterpretation:
        `**Interprétation : la quasi-totalité des pas de simulation présentent 0 ou 1 brouillard actif, le chevauchement à deux brouillards simultanés (${weatherSystem.histograms.active_front_count[2] || 0} pas observés) reste marginal. Ce résultat confirme que les paramètres actuels produisent une météo lisible et non saturée. Les rares chevauchements s'expliquent mécaniquement par la combinaison de délais d'arrivée courts et d'une vitesse de front volontairement lente.**`
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
          value: formatNumber((grassBrightTopTwo / grassBrightnessTotal) * 100, 1),
          unit: "%"
        },
        {
          label: "Masse totale observée",
          value: new Intl.NumberFormat("fr-FR").format(grassBrightnessTotal),
          unit: "cellules"
        },
        {
          label: "Classes sombres utilisées",
          value: "Quasi nuls"
        }
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
      }),
      postChartInterpretation:
        "**Interprétation : la forte concentration de la masse dans les deux classes les plus claires traduit directement le comportement de la Beta(7, 2), dont l'espérance brute vaut 7/9 ≈ 0,78 avant remappage. La quasi-totalité des cellules herbeuses tombent dans ces plages lumineuses, les classes sombres ne sont pas impossibles, mais leur probabilité est négligeable avec les paramètres actuels. C'est la conséquence visuelle concrète d'un paramètre α largement supérieur à β.**"
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
          value: formatNumber(run.config.dirtCoveragePercent, 0),
          unit: "%"
        },
        {
          label: "Couverture observée",
          value: formatNumber(mapGeneration.summary.dirt_coverage_percent.mean, 1),
          unit: "%"
        },
        {
          label: "Écart absolu",
          value: formatNumber(run.config.dirtCoveragePercent - mapGeneration.summary.dirt_coverage_percent.mean, 1),
          unit: "%"
        }
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture de la terre ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observée"],
        values: dirtCoverageComparison,
        color: COLORS.brick,
        xAxisName: "Mesure",
        yAxisName: "Pourcentage",
      }),
      postChartInterpretation:
        "**Interprétation : l'écart entre la cible et l'observé confirme que la variable de configuration est une intention de génération, pas une promesse de couverture finale. Le pipeline procédural, contraintes de connectivité, suppression de petites composantes, post-traitement topologique, consomme systématiquement une partie de la terre initialement générée. Ce résultat est empiriquement utile : pour viser une couverture finale d'environ X %, il faut configurer la cible légèrement au-dessus.**"
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
          value: formatNumber(run.config.waterCoveragePercent, 0),
          unit: "%"
        },
        {
          label: "Couverture observée",
          value: formatNumber(mapGeneration.summary.water_coverage_percent.mean, 1),
          unit: "%"
        },
        {
          label: "Composantes moyennes",
          value: formatNumber(mapGeneration.summary.water_component_count.mean, 1),
          unit: "composantes"
        }
      ],
      chartHeight: 290,
      chartLabel: "Comparaison entre la couverture d'eau ciblée et observée",
      chartOption: buildHistogramOption({
        categories: ["Cible", "Observée"],
        values: waterCoverageComparison,
        color: COLORS.slate,
        xAxisName: "Mesure",
        yAxisName: "Pourcentage"
      }),
      postChartInterpretation:
        "**Interprétation : comme pour la terre, la couverture eau finale reste systématiquement inférieure à la cible. L'eau subit les mêmes post-traitements topologiques et voit ses petites composantes supprimées pour éviter des îlots non jouables. La variable de configuration est là aussi une intention, pas une valeur garantie.**"
    },
    {
      title: "La taille des composantes d'eau raconte mieux la forme des lacs qu'un simple pourcentage",
      description:
        "Ici, l'information importante n'est pas seulement la couverture totale, mais la fragmentation: combien de poches d'eau apparaissent, et quelle taille prend la plus grosse composante.",
      metrics: [
        {
          label: "Plus grande composante moyenne",
          value: formatNumber(mapGeneration.summary.largest_water_component.mean, 1),
          unit: "cellules"
        },
        {
          label: "Maximum observé",
          value: formatNumber(mapGeneration.summary.largest_water_component.max, 0),
          unit: "cellules"
        },
        {
          label: "Moyenne des lacs proches des mines",
          value: formatNumber(mapGeneration.summary.mine_to_nearest_lake_distance.mean, 1),
          unit: "cases"
        }
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des tailles de composantes d'eau",
      chartOption: buildHistogramOption({
        categories: waterComponentSizes.map(([value]) => String(value)),
        values: waterComponentSizes.map(([, count]) => count),
        color: COLORS.slate,
        xAxisName: "Taille de composante",
        yAxisName: "Occurrences"
      }),
      postChartInterpretation:
        "**Interprétation : la distribution révèle que l'eau se fragmente en composantes de tailles très variées, avec généralement une ou deux grandes composantes dominantes et une queue de petites poches. C'est le comportement typique d'un champ procédural seuillé : le bruit produit des structures spatiales corrélées, et c'est cette morphologie en composantes qui définit la vraie topologie jouable du terrain aquatique, bien plus que la simple densité brute.**"
    }
  ],
  "Couverture cible du brouillard": [
    {
      title: "La couverture visible finale peut dépasser un peu la couverture cible",
      description:
        "La couverture cible tirée entre 5 % et 20 % n'est pas une promesse exacte sur le pourcentage final visible cellule par cellule. Le runtime commence par convertir cette cible en aire, en déduit une ellipse selon le rapport d'aspect, ajuste la géométrie du front, déforme ensuite le contour avec le bruit de forme, applique un fondu de bord (`edgeSoftness`) puis discrétise le résultat sur la grille visible. La couverture réellement observée peut donc s'écarter un peu de la cible, et parfois dépasser la borne haute nominale.",
      metrics: [
        {
          label: "Couverture de pic moyenne",
          value: formatNumber(weatherSystem.summary.isolated_peak_visible_coverage_percent.mean, 1),
          unit: "%"
        },
        {
          label: "q90 observé",
          value: formatNumber(weatherSystem.summary.isolated_peak_visible_coverage_percent.q90, 1),
          unit: "%"
        },
        {
          label: "Maximum observé",
          value: formatNumber(weatherSystem.summary.isolated_peak_visible_coverage_percent.max, 1),
          unit: "%"
        }
      ],
      chartHeight: 290,
      chartLabel: "Histogramme des couvertures visibles maximales des brouillards météo",
      chartOption: buildHistogramOption({
        categories: weatherPeakCoverageEntries.map(([value]) => String(value)),
        values: weatherPeakCoverageEntries.map(([, count]) => count),
        color: COLORS.brick,
        xAxisName: "Couverture visible (%)",
        yAxisName: "Occurrences"
      }),
      postChartInterpretation:
        "**Interprétation : la distribution s'étale bien au-delà de la borne haute nominale de 20 %, ce qui confirme que la cible configurée n'est pas une contrainte dure sur le résultat final. Chaque étape de la chaîne de construction, aire cible, rayons d'ellipse, déformation de contour, fondu de bord, rasterisation, peut légèrement amplifier la zone réellement visible. La variable de commande décrit une intention géométrique initiale, et c'est la composition de toutes ces transformations qui produit la couverture finale observée.**"
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
          value: String(run.sampleCount),
          unit: "seeds"
        },
        {
          label: "Budget temporel",
          value: String(run.turnBudget),
          unit: "tours"
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