const GRID_SIZE = 9;

const TERRAIN_VOID = 0;
const TERRAIN_GRASS = 1;
const TERRAIN_DIRT = 2;
const TERRAIN_WATER = 3;

const PIECE_PAWN = 0;
const PIECE_KNIGHT = 1;
const PIECE_BISHOP = 2;
const PIECE_ROOK = 3;
const PIECE_QUEEN = 4;
const PIECE_KING = 5;

const BUILDING_CHURCH = 0;
const BUILDING_MINE = 1;
const BUILDING_FARM = 2;
const BUILDING_BARRACKS = 3;

const CHEST_REWARD_GOLD = 0;
const CHEST_REWARD_MOVEMENT_MAX_BONUS = 1;
const CHEST_REWARD_BUILD_MAX_BONUS = 2;

const REFERENCE_DATA = Object.freeze({
  kingdoms: [
    { id: 0, key: "white", label: "White" },
    { id: 1, key: "black", label: "Black" }
  ],
  pieceTypes: [
    { id: PIECE_PAWN, key: "pawn", label: "Pawn" },
    { id: PIECE_KNIGHT, key: "knight", label: "Knight" },
    { id: PIECE_BISHOP, key: "bishop", label: "Bishop" },
    { id: PIECE_ROOK, key: "rook", label: "Rook" },
    { id: PIECE_QUEEN, key: "queen", label: "Queen" },
    { id: PIECE_KING, key: "king", label: "King" }
  ],
  buildingTypes: [
    { id: BUILDING_CHURCH, key: "church", label: "Church" },
    { id: BUILDING_MINE, key: "mine", label: "Mine" },
    { id: BUILDING_FARM, key: "farm", label: "Farm" },
    { id: BUILDING_BARRACKS, key: "barracks", label: "Barracks" }
  ]
});

const TERRAIN_TOKEN_MAP = Object.freeze({
  g: { t: TERRAIN_GRASS, b: 232 },
  d: { t: TERRAIN_DIRT, b: 222 },
  w: { t: TERRAIN_WATER, b: 255 },
  v: { t: TERRAIN_VOID, b: 255 }
});

const WEATHER_TOKEN_MAP = Object.freeze({
  ".": { alpha: 0, shade: 0 },
  m: { alpha: 96, shade: 168 },
  d: { alpha: 172, shade: 208 }
});

const CHEST_REWARD_TYPES = Object.freeze({
  gold: {
    id: CHEST_REWARD_GOLD,
    key: "gold",
    label: "Gold",
    buildDescription(amount) {
      return `+${amount} gold`;
    },
    buildMessage(kingdomLabel, amount) {
      return `${kingdomLabel} gained +${amount} gold.`;
    }
  },
  movement_points_max_bonus: {
    id: CHEST_REWARD_MOVEMENT_MAX_BONUS,
    key: "movement_points_max_bonus",
    label: "Movement Points",
    buildDescription(amount) {
      return `+${amount} max movement point per turn`;
    },
    buildMessage(kingdomLabel, amount) {
      return `${kingdomLabel} permanently gained +${amount} max movement point per turn.`;
    }
  },
  build_points_max_bonus: {
    id: CHEST_REWARD_BUILD_MAX_BONUS,
    key: "build_points_max_bonus",
    label: "Build Points",
    buildDescription(amount) {
      return `+${amount} max build point per turn`;
    },
    buildMessage(kingdomLabel, amount) {
      return `${kingdomLabel} permanently gained +${amount} max build point per turn.`;
    }
  }
});

function deepClone(value) {
  return JSON.parse(JSON.stringify(value));
}

function createCell(token) {
  const config = TERRAIN_TOKEN_MAP[token] || TERRAIN_TOKEN_MAP.g;
  return {
    t: config.t,
    c: config.t === TERRAIN_VOID ? 0 : 1,
    f: 0,
    b: config.b
  };
}

function createGrid(rows) {
  if (!Array.isArray(rows) || rows.length !== GRID_SIZE) {
    throw new Error(`A vignette grid must contain exactly ${GRID_SIZE} rows.`);
  }

  return rows.map((row) => {
    if (typeof row !== "string" || row.length !== GRID_SIZE) {
      throw new Error(`A vignette grid row must contain exactly ${GRID_SIZE} cells.`);
    }

    return Array.from(row, createCell);
  });
}

function createWeatherState(rows) {
  if (!rows) {
    return null;
  }

  const alphaByCell = [];
  const shadeByCell = [];
  let hasActiveFront = 0;

  for (const row of rows) {
    if (typeof row !== "string" || row.length !== GRID_SIZE) {
      throw new Error(`A weather mask row must contain exactly ${GRID_SIZE} cells.`);
    }

    for (const token of row) {
      const config = WEATHER_TOKEN_MAP[token] || WEATHER_TOKEN_MAP["."];
      alphaByCell.push(config.alpha);
      shadeByCell.push(config.shade);
      if (config.alpha > 0) {
        hasActiveFront = 1;
      }
    }
  }

  return {
    nextSpawnTurnStep: 0,
    hasActiveFront,
    rngCounter: 0,
    revision: 1,
    activeFront: null,
    mask: {
      revision: 1,
      diameter: GRID_SIZE,
      hasActiveFront,
      alphaByCell,
      shadeByCell
    }
  };
}

function createPiece({ id, type, kingdom, x, y, xp = 0 }) {
  return {
    id,
    type,
    kingdom,
    x,
    y,
    xp,
    formationId: -1
  };
}

function createBuilding({ id, type, owner = 0, isNeutral = false, ox, oy, w, h, rot = 0, fm = 0, producingType = 0 }) {
  const cellCount = w * h;
  return {
    id,
    type,
    owner,
    isNeutral,
    ox,
    oy,
    w,
    h,
    rot,
    fm,
    state: 0,
    isProducing: false,
    producingType,
    turnsRemaining: 0,
    hp: Array(cellCount).fill(isNeutral ? 999 : 1),
    breach: Array(cellCount).fill(0)
  };
}

function createProducingBuilding({ turnsRemaining = 0, ...buildingConfig }) {
  const building = createBuilding(buildingConfig);
  building.isProducing = turnsRemaining > 0;
  building.turnsRemaining = turnsRemaining;
  return building;
}

function createChest({ id, x, y }) {
  return {
    id,
    type: 0,
    x,
    y
  };
}

function createInfernalUnit({ id, pieceType, x, y, targetKingdom = 0 }) {
  return {
    id,
    x,
    y,
    pieceType,
    targetKingdom,
    phase: 0,
    spawnTurn: 0
  };
}

function createKingdomState({
  gold = 0,
  movementPointsMaxBonus = 0,
  buildPointsMaxBonus = 0,
  pieces = [],
  buildings = []
} = {}) {
  return {
    gold,
    movementPointsMaxBonus,
    buildPointsMaxBonus,
    hasSpawnedBishop: 1,
    lastBishopSpawnParity: 0,
    pieces,
    buildings
  };
}

function createSnapshot({
  saveName,
  turnNumber = 0,
  activeKingdom = 0,
  grid,
  whiteGold = 0,
  blackGold = 0,
  whiteMovementPointsMaxBonus = 0,
  blackMovementPointsMaxBonus = 0,
  whiteBuildPointsMaxBonus = 0,
  blackBuildPointsMaxBonus = 0,
  whitePieces = [],
  blackPieces = [],
  whiteBuildings = [],
  blackBuildings = [],
  publicBuildings = [],
  mapObjects = [],
  autonomousUnits = [],
  weatherState = null,
  events = []
}) {
  return {
    gameName: saveName,
    turnNumber,
    activeKingdom,
    grid: deepClone(grid),
    whiteKingdom: createKingdomState({
      gold: whiteGold,
      movementPointsMaxBonus: whiteMovementPointsMaxBonus,
      buildPointsMaxBonus: whiteBuildPointsMaxBonus,
      pieces: whitePieces,
      buildings: whiteBuildings
    }),
    blackKingdom: createKingdomState({
      gold: blackGold,
      movementPointsMaxBonus: blackMovementPointsMaxBonus,
      buildPointsMaxBonus: blackBuildPointsMaxBonus,
      pieces: blackPieces,
      buildings: blackBuildings
    }),
    publicBuildings,
    mapObjects,
    autonomousUnits,
    weatherState: weatherState ? deepClone(weatherState) : null,
    events
  };
}

function createReplayFrame({ snapshot, committedTurnNumber, committedActiveKingdom = 0, notifications = [] }) {
  return {
    snapshot,
    committedTurnNumber,
    committedActiveKingdom,
    notifications,
    newEvents: []
  };
}

function createChestRewardNotification({ kingdom = 0, rewardTypeKey = "gold", amount = 0 }) {
  const rewardType = CHEST_REWARD_TYPES[rewardTypeKey] || CHEST_REWARD_TYPES.gold;
  const kingdomKey = kingdom === 1 ? "black" : "white";
  const kingdomLabel = kingdomKey === "black" ? "Black" : "White";

  return {
    kind: 0,
    kindKey: "chest_reward",
    kindLabel: "Chest Reward",
    kingdom,
    kingdomKey,
    kingdomLabel,
    chestRewardType: rewardType.id,
    chestRewardTypeKey: rewardType.key,
    chestRewardAmount: amount,
    chestReward: {
      typeId: rewardType.id,
      typeKey: rewardType.key,
      typeLabel: rewardType.label,
      amount,
      description: rewardType.buildDescription(amount)
    },
    title: "Chest Opened",
    message: rewardType.buildMessage(kingdomLabel, amount)
  };
}

function createChestCycleFrames({ saveName, grid, weatherState }) {
  const whiteKing = createPiece({ id: 101, type: PIECE_KING, kingdom: 0, x: 1, y: 6 });
  const blackPawn = createPiece({ id: 111, type: PIECE_PAWN, kingdom: 1, x: 7, y: 2 });
  const buildSnapshot = ({
    turnNumber,
    pawnX,
    chestVisible,
    whiteGold = 0,
    whiteMovementPointsMaxBonus = 0,
    whiteBuildPointsMaxBonus = 0
  }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whiteGold,
    whiteMovementPointsMaxBonus,
    whiteBuildPointsMaxBonus,
    whitePieces: [
      whiteKing,
      createPiece({ id: 102, type: PIECE_PAWN, kingdom: 0, x: pawnX, y: 4 })
    ],
    blackPieces: [blackPawn],
    mapObjects: chestVisible ? [createChest({ id: 121, x: 4, y: 4 })] : []
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, pawnX: 3, chestVisible: true })
    }),
    createReplayFrame({
      committedTurnNumber: 2,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 2, pawnX: 4, chestVisible: false, whiteGold: 25 }),
      notifications: [createChestRewardNotification({ kingdom: 0, rewardTypeKey: "gold", amount: 25 })]
    }),
    createReplayFrame({
      committedTurnNumber: 3,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 3, pawnX: 2, chestVisible: true })
    }),
    createReplayFrame({
      committedTurnNumber: 4,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 4, pawnX: 3, chestVisible: true })
    }),
    createReplayFrame({
      committedTurnNumber: 5,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({
        turnNumber: 5,
        pawnX: 4,
        chestVisible: false,
        whiteMovementPointsMaxBonus: 1
      }),
      notifications: [createChestRewardNotification({ kingdom: 0, rewardTypeKey: "movement_points_max_bonus", amount: 1 })]
    }),
    createReplayFrame({
      committedTurnNumber: 6,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 6, pawnX: 2, chestVisible: true })
    }),
    createReplayFrame({
      committedTurnNumber: 7,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 7, pawnX: 3, chestVisible: true })
    }),
    createReplayFrame({
      committedTurnNumber: 8,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({
        turnNumber: 8,
        pawnX: 4,
        chestVisible: false,
        whiteBuildPointsMaxBonus: 1
      }),
      notifications: [createChestRewardNotification({ kingdom: 0, rewardTypeKey: "build_points_max_bonus", amount: 1 })]
    })
  ];
}

function createMovementPointsCycleFrames({ saveName, grid, weatherState }) {
  const buildSnapshot = ({ turnNumber, rookX, pawnX }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whitePieces: [
      createPiece({ id: 201, type: PIECE_KING, kingdom: 0, x: 1, y: 7 }),
      createPiece({ id: 202, type: PIECE_PAWN, kingdom: 0, x: pawnX, y: 4 }),
      createPiece({ id: 203, type: PIECE_ROOK, kingdom: 0, x: rookX, y: 6 })
    ],
    blackPieces: [
      createPiece({ id: 211, type: PIECE_KING, kingdom: 1, x: 7, y: 1 }),
      createPiece({ id: 212, type: PIECE_PAWN, kingdom: 1, x: 7, y: 5 })
    ]
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, rookX: 6, pawnX: 1 })
    }),
    createReplayFrame({
      committedTurnNumber: 2,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 2, rookX: 6, pawnX: 2 })
    })
  ];
}

function createChurchPromotionCycleFrames({ saveName, grid, weatherState }) {
  const buildSnapshot = ({ turnNumber, promoted = false }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whitePieces: promoted
      ? [
          createPiece({ id: 131, type: PIECE_KING, kingdom: 0, x: 2, y: 4 }),
          createPiece({ id: 132, type: PIECE_BISHOP, kingdom: 0, x: 3, y: 4 }),
          createPiece({ id: 133, type: PIECE_QUEEN, kingdom: 0, x: 4, y: 4 })
        ]
      : [
          createPiece({ id: 131, type: PIECE_KING, kingdom: 0, x: 2, y: 4 }),
          createPiece({ id: 132, type: PIECE_BISHOP, kingdom: 0, x: 3, y: 4 }),
          createPiece({ id: 133, type: PIECE_ROOK, kingdom: 0, x: 4, y: 4 })
        ],
    publicBuildings: [
      createBuilding({ id: 151, type: BUILDING_CHURCH, owner: 0, isNeutral: true, ox: 2, oy: 3, w: 4, h: 3 })
    ]
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, promoted: true })
    })
  ];
}

function createEconomyGoldCycleFrames({ saveName, grid, weatherState }) {
  const buildSnapshot = ({ turnNumber, gold, pawnX, pawnY }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whiteGold: gold,
    whitePieces: [
      createPiece({ id: 41, type: PIECE_KING, kingdom: 0, x: 1, y: 8 }),
      createPiece({ id: 42, type: PIECE_PAWN, kingdom: 0, x: pawnX, y: pawnY })
    ],
    blackPieces: [
      createPiece({ id: 51, type: PIECE_PAWN, kingdom: 1, x: 7, y: 5 })
    ],
    publicBuildings: [
      createBuilding({ id: 61, type: BUILDING_MINE, owner: 0, isNeutral: true, ox: 1, oy: 2, w: 6, h: 6 })
    ]
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, gold: 10, pawnX: 1, pawnY: 3 })
    }),
    createReplayFrame({
      committedTurnNumber: 2,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 2, gold: 20, pawnX: 1, pawnY: 3 })
    }),
    createReplayFrame({
      committedTurnNumber: 3,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 3, gold: 30, pawnX: 1, pawnY: 3 })
    })
  ];
}

function createProductionCycleFrames({ saveName, grid, weatherState }) {
  const buildSnapshot = ({ turnNumber, turnsRemaining, bishopReady = false }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whitePieces: [
      createPiece({ id: 71, type: PIECE_KING, kingdom: 0, x: 1, y: 4 }),
      ...(bishopReady ? [createPiece({ id: 74, type: PIECE_BISHOP, kingdom: 0, x: 4, y: 4 })] : [])
    ],
    blackPieces: [
      createPiece({ id: 81, type: PIECE_PAWN, kingdom: 1, x: 7, y: 1 })
    ],
    whiteBuildings: [
      createProducingBuilding({
        id: 91,
        type: BUILDING_BARRACKS,
        owner: 0,
        ox: 3,
        oy: 3,
        w: 4,
        h: 3,
        producingType: PIECE_BISHOP,
        turnsRemaining
      })
    ]
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, turnsRemaining: 2 })
    }),
    createReplayFrame({
      committedTurnNumber: 2,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 2, turnsRemaining: 1 })
    }),
    createReplayFrame({
      committedTurnNumber: 3,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 3, turnsRemaining: 0, bishopReady: true })
    })
  ];
}

function createInfernalHuntCycleFrames({ saveName, grid, weatherState }) {
  const buildSnapshot = ({ turnNumber, rookX, bishopAlive, pawnAlive }) => createSnapshot({
    saveName,
    turnNumber,
    activeKingdom: 0,
    grid,
    weatherState,
    whitePieces: [
      createPiece({ id: 191, type: PIECE_KING, kingdom: 0, x: 1, y: 7 }),
      ...(bishopAlive ? [createPiece({ id: 192, type: PIECE_BISHOP, kingdom: 0, x: 5, y: 4 })] : []),
      ...(pawnAlive ? [createPiece({ id: 193, type: PIECE_PAWN, kingdom: 0, x: 3, y: 4 })] : [])
    ],
    autonomousUnits: [
      createInfernalUnit({ id: 211, pieceType: PIECE_ROOK, x: rookX, y: 4, targetKingdom: 0 })
    ]
  });

  return [
    createReplayFrame({
      committedTurnNumber: 1,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 1, rookX: 5, bishopAlive: false, pawnAlive: true })
    }),
    createReplayFrame({
      committedTurnNumber: 2,
      committedActiveKingdom: 0,
      snapshot: buildSnapshot({ turnNumber: 2, rookX: 3, bishopAlive: false, pawnAlive: false })
    })
  ];
}

function createReplayData({
  saveName,
  grid,
  activeKingdom = 0,
  whiteGold = 0,
  blackGold = 0,
  whiteMovementPointsMaxBonus = 0,
  blackMovementPointsMaxBonus = 0,
  whiteBuildPointsMaxBonus = 0,
  blackBuildPointsMaxBonus = 0,
  whitePieces = [],
  blackPieces = [],
  whiteBuildings = [],
  blackBuildings = [],
  publicBuildings = [],
  mapObjects = [],
  autonomousUnits = [],
  weatherState = null,
  turnHistory = []
}) {
  return {
    schemaVersion: 5,
    saveName,
    createdAtUnix: 0,
    referenceData: deepClone(REFERENCE_DATA),
    turnHistory,
    initialSnapshot: createSnapshot({
      saveName,
      turnNumber: 0,
      activeKingdom,
      grid,
      whiteGold,
      blackGold,
      whiteMovementPointsMaxBonus,
      blackMovementPointsMaxBonus,
      whiteBuildPointsMaxBonus,
      blackBuildPointsMaxBonus,
      whitePieces,
      blackPieces,
      whiteBuildings,
      blackBuildings,
      publicBuildings,
      mapObjects,
      autonomousUnits,
      weatherState
    })
  };
}

function createVignette({
  saveName,
  ariaLabel,
  grid,
  weatherMask = null,
  buildTurnHistory = null,
  autoplayIntervalMs,
  autoplayOnMount = false,
  loopPlayback = false,
  toastCooldownMs = 0,
  enablePerspective = false,
  perspectiveKingdom = "white",
  perspectiveSequence = undefined,
  perspectiveSwapIntervalMs = 2000,
  showPerspectiveOverlay = false,
  showToasts = false,
  allowZoom = false,
  showBuildingLabels = undefined,
  hiddenBuildingLabelKeys = undefined,
  productionOverlay = undefined,
  statusOverlay = undefined,
  ...entities
}) {
  const replayGrid = createGrid(grid);
  const replayWeatherState = createWeatherState(weatherMask);
  const hasBuildings = [entities.whiteBuildings, entities.blackBuildings, entities.publicBuildings]
    .some((buildingCollection) => Array.isArray(buildingCollection) && buildingCollection.length > 0);
  const turnHistory = typeof buildTurnHistory === "function"
    ? buildTurnHistory({ saveName, grid: replayGrid, weatherState: replayWeatherState })
    : [];

  const vignette = {
    ariaLabel,
    replayData: createReplayData({
      saveName,
      grid: replayGrid,
      weatherState: replayWeatherState,
      turnHistory,
      ...entities
    }),
    autoplayOnMount,
    loopPlayback,
    toastCooldownMs,
    enablePerspective,
    perspectiveKingdom,
    perspectiveSwapIntervalMs,
    showPerspectiveOverlay,
    showToasts,
    allowZoom,
    showBuildingLabels: typeof showBuildingLabels === "boolean" ? showBuildingLabels : hasBuildings,
    hiddenBuildingLabelKeys: Array.isArray(hiddenBuildingLabelKeys) ? hiddenBuildingLabelKeys : [],
    statusOverlay
  };

  if (typeof autoplayIntervalMs === "number") {
    vignette.autoplayIntervalMs = autoplayIntervalMs;
  }

  if (Array.isArray(perspectiveSequence) && perspectiveSequence.length) {
    vignette.perspectiveSequence = perspectiveSequence;
  }

  if (productionOverlay && typeof productionOverlay === "object") {
    vignette.productionOverlay = productionOverlay;
  }

  return Object.freeze(vignette);
}

export const reportIntroVignettes = Object.freeze({
  kingdoms: createVignette({
    saveName: "intro-kingdoms",
    ariaLabel: "Deux royaumes et leurs pièces principales",
    grid: [
      "ggggggggg",
      "gggdggggg",
      "ggggggdgg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggdgg",
      "ggggggggg",
      "ggggdgggg"
    ],
    whitePieces: [
      createPiece({ id: 1, type: PIECE_KING, kingdom: 0, x: 1, y: 4 }),
      createPiece({ id: 2, type: PIECE_ROOK, kingdom: 0, x: 2, y: 2 }),
      createPiece({ id: 3, type: PIECE_PAWN, kingdom: 0, x: 2, y: 6 })
    ],
    blackPieces: [
      createPiece({ id: 11, type: PIECE_KING, kingdom: 1, x: 7, y: 4 }),
      createPiece({ id: 12, type: PIECE_QUEEN, kingdom: 1, x: 6, y: 2 }),
      createPiece({ id: 13, type: PIECE_PAWN, kingdom: 1, x: 6, y: 6 })
    ]
  }),
  turnBudget: createVignette({
    saveName: "intro-turn-budget",
    ariaLabel: "Un même tour combine des déplacements et des actions de construction",
    autoplayIntervalMs: 1200,
    autoplayOnMount: true,
    loopPlayback: true,
    buildTurnHistory: createMovementPointsCycleFrames,
    statusOverlay: {
      kingdom: "white",
      label: "Points de mouvement",
      values: [5, 1, 0]
    },
    grid: [
      "ggggggggg",
      "ggdgggggg",
      "ggggggdgg",
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggggggg"
    ],
    whitePieces: [
      createPiece({ id: 201, type: PIECE_KING, kingdom: 0, x: 1, y: 7 }),
      createPiece({ id: 202, type: PIECE_PAWN, kingdom: 0, x: 1, y: 4 }),
      createPiece({ id: 203, type: PIECE_ROOK, kingdom: 0, x: 2, y: 6 })
    ],
    blackPieces: [
      createPiece({ id: 211, type: PIECE_KING, kingdom: 1, x: 7, y: 1 }),
      createPiece({ id: 212, type: PIECE_PAWN, kingdom: 1, x: 7, y: 5 })
    ]
  }),
  terrain: createVignette({
    saveName: "intro-terrain",
    ariaLabel: "Terrain local avec une riviere qui bloque le passage",
    grid: [
      "ggggwgggg",
      "ggggwgggg",
      "ggggwgggg",
      "dddgwgggg",
      "ggggwgggg",
      "ggggwgggg",
      "ggggwgggg",
      "ggggwgggg",
      "ggggwgggg"
    ],
    whitePieces: [
      createPiece({ id: 21, type: PIECE_KING, kingdom: 0, x: 1, y: 7 }),
      createPiece({ id: 22, type: PIECE_PAWN, kingdom: 0, x: 2, y: 4 })
    ],
    blackPieces: [
      createPiece({ id: 31, type: PIECE_PAWN, kingdom: 1, x: 6, y: 4 }),
      createPiece({ id: 32, type: PIECE_BISHOP, kingdom: 1, x: 7, y: 2 })
    ]
  }),
  economy: createVignette({
    saveName: "intro-economy",
    ariaLabel: "Un pion blanc entre sur la mine et l'or augmente de 10 en 10",
    autoplayIntervalMs: 1200,
    autoplayOnMount: true,
    loopPlayback: true,
    buildTurnHistory: createEconomyGoldCycleFrames,
    statusOverlay: {
      label: "Or",
      values: [0, 10, 20, 30],
      showShield: false
    },
    grid: [
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg"
    ],
    whiteGold: 0,
    whitePieces: [
      createPiece({ id: 41, type: PIECE_KING, kingdom: 0, x: 1, y: 8 }),
      createPiece({ id: 42, type: PIECE_PAWN, kingdom: 0, x: 0, y: 3 })
    ],
    blackPieces: [
      createPiece({ id: 51, type: PIECE_PAWN, kingdom: 1, x: 7, y: 5 })
    ],
    publicBuildings: [
      createBuilding({ id: 61, type: BUILDING_MINE, owner: 0, isNeutral: true, ox: 1, oy: 2, w: 6, h: 6 })
    ]
  }),
  production: createVignette({
    saveName: "intro-production",
    ariaLabel: "Une caserne prépare un fou avant de le déployer sur le front local",
    autoplayIntervalMs: 1200,
    autoplayOnMount: true,
    loopPlayback: true,
    buildTurnHistory: createProductionCycleFrames,
    statusOverlay: {
      label: "Point de vue",
      values: ["Blancs"],
      showShield: false
    },
    productionOverlay: {
      buildingId: 91,
      label: "Tours restants",
      values: ["3", "2", "1", "0"],
      progressValues: [0, 1 / 3, 2 / 3, 1],
      pieceType: "bishop",
      kingdom: "white",
      anchorPlacement: "below",
      anchorYOffsetCells: 0.05
    },
    grid: [
      "ggggggggg",
      "gdggggggg",
      "ggggggdgg",
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggggggg"
    ],
    whitePieces: [
      createPiece({ id: 71, type: PIECE_KING, kingdom: 0, x: 1, y: 4 })
    ],
    blackPieces: [
      createPiece({ id: 81, type: PIECE_PAWN, kingdom: 1, x: 7, y: 1 })
    ],
    whiteBuildings: [
      createProducingBuilding({
        id: 91,
        type: BUILDING_BARRACKS,
        owner: 0,
        ox: 3,
        oy: 3,
        w: 4,
        h: 3,
        producingType: PIECE_BISHOP,
        turnsRemaining: 3
      })
    ]
  }),
  chest: createVignette({
    saveName: "intro-chest",
    ariaLabel: "Un pion ouvre un coffre et la récompense change sur chaque boucle",
    autoplayIntervalMs: 1000,
    autoplayOnMount: true,
    loopPlayback: true,
    toastCooldownMs: 1800,
    showToasts: true,
    grid: [
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggggggdgg",
      "ggggggggg"
    ],
    buildTurnHistory: createChestCycleFrames,
    whitePieces: [
      createPiece({ id: 101, type: PIECE_KING, kingdom: 0, x: 1, y: 6 }),
      createPiece({ id: 102, type: PIECE_PAWN, kingdom: 0, x: 2, y: 4 })
    ],
    blackPieces: [
      createPiece({ id: 111, type: PIECE_PAWN, kingdom: 1, x: 7, y: 2 })
    ],
    mapObjects: [
      createChest({ id: 121, x: 4, y: 4 })
    ]
  }),
  progression: createVignette({
    saveName: "intro-progression",
    ariaLabel: "Dans une église, un roi, un fou et une tour deviennent une reine",
    autoplayIntervalMs: 1400,
    autoplayOnMount: true,
    loopPlayback: true,
    buildTurnHistory: createChurchPromotionCycleFrames,
    grid: [
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggggdgg"
    ],
    whitePieces: [
      createPiece({ id: 131, type: PIECE_KING, kingdom: 0, x: 2, y: 4 }),
      createPiece({ id: 132, type: PIECE_BISHOP, kingdom: 0, x: 3, y: 4 }),
      createPiece({ id: 133, type: PIECE_ROOK, kingdom: 0, x: 4, y: 4 })
    ],
    publicBuildings: [
      createBuilding({ id: 151, type: BUILDING_CHURCH, owner: 0, isNeutral: true, ox: 2, oy: 3, w: 4, h: 3 })
    ]
  }),
  weather: createVignette({
    saveName: "intro-weather",
    ariaLabel: "Le point de vue alterne entre les Blancs et les Noirs pour montrer que le brouillard ne cache pas les mêmes pièces",
    enablePerspective: true,
    perspectiveKingdom: "white",
    perspectiveSequence: ["white", "black"],
    perspectiveSwapIntervalMs: 2000,
    showPerspectiveOverlay: true,
    hiddenBuildingLabelKeys: ["barracks"],
    grid: [
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggggdgg",
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg"
    ],
    weatherMask: [
      "....mmmdd",
      "....mmddd",
      "...mmmddd",
      "...mmmddd",
      "...mmmddd",
      "....mmddd",
      "....mmmdd",
      ".....mmmm",
      "......mmm"
    ],
    whitePieces: [
      createPiece({ id: 161, type: PIECE_KING, kingdom: 0, x: 1, y: 4 })
    ],
    blackPieces: [
      createPiece({ id: 171, type: PIECE_KING, kingdom: 1, x: 6, y: 6 }),
      createPiece({ id: 172, type: PIECE_PAWN, kingdom: 1, x: 7, y: 7 })
    ],
    blackBuildings: [
      createBuilding({ id: 181, type: BUILDING_BARRACKS, owner: 1, ox: 5, oy: 2, w: 4, h: 3, producingType: PIECE_PAWN })
    ]
  }),
  infernal: createVignette({
    saveName: "intro-infernal",
    ariaLabel: "Une unite infernale autonome entre par le bord et chasse le royaume cible",
    autoplayIntervalMs: 1200,
    autoplayOnMount: true,
    loopPlayback: true,
    buildTurnHistory: createInfernalHuntCycleFrames,
    grid: [
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg",
      "ggdgggggg",
      "ggggggggg",
      "ggggggdgg",
      "ggggggggg",
      "ggggdgggg",
      "ggggggggg"
    ],
    whitePieces: [
      createPiece({ id: 191, type: PIECE_KING, kingdom: 0, x: 1, y: 7 }),
      createPiece({ id: 192, type: PIECE_BISHOP, kingdom: 0, x: 5, y: 4 }),
      createPiece({ id: 193, type: PIECE_PAWN, kingdom: 0, x: 3, y: 4 })
    ],
    autonomousUnits: [
      createInfernalUnit({ id: 211, pieceType: PIECE_ROOK, x: 8, y: 4, targetKingdom: 0 })
    ]
  })
});