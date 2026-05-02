const DEFAULT_AUTOPLAY_INTERVAL_MS = 1100;
const BOARD_RADIUS = 18;
const BOARD_WIDTH = BOARD_RADIUS * 2;
const BOARD_HEIGHT = BOARD_RADIUS * 2;
const EMPTY_BUILDINGS = [];
const EMPTY_PIECES = [];
const EMPTY_AUTONOMOUS_UNITS = [];
const FLIP_HORIZONTAL_MASK = 1;
const FLIP_VERTICAL_MASK = 2;
const EPSILON = 1e-6;
const K_PI = Math.PI;

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

const CHEST_REWARD_GOLD = 0;
const CHEST_REWARD_MOVEMENT_MAX_BONUS = 1;
const CHEST_REWARD_BUILD_MAX_BONUS = 2;

const WEATHER_DIRECTION_NORTH = 0;
const WEATHER_DIRECTION_SOUTH = 1;
const WEATHER_DIRECTION_EAST = 2;
const WEATHER_DIRECTION_WEST = 3;
const WEATHER_DIRECTION_NORTH_EAST = 4;
const WEATHER_DIRECTION_NORTH_WEST = 5;
const WEATHER_DIRECTION_SOUTH_EAST = 6;
const WEATHER_DIRECTION_SOUTH_WEST = 7;

const ENTRY_EDGE_TOP = "top";
const ENTRY_EDGE_BOTTOM = "bottom";
const ENTRY_EDGE_LEFT = "left";
const ENTRY_EDGE_RIGHT = "right";

const INFERNAL_PHASE_HUNTING = 0;
const INFERNAL_PHASE_SEARCHING = 2;

const GAME_CONFIG = {
  map: {
    radius: BOARD_RADIUS,
    cellSizePx: 16,
    numMines: 2,
    numFarms: 3,
    minPublicBuildingDistance: 10,
    playerSpawnZonePercent: 25,
    aiSpawnZonePercent: 25,
    terrainNoiseScale: 14,
    terrainOctaves: 3,
    dirtCoveragePercent: 14,
    waterCoveragePercent: 4,
    numDirtBlobs: 6,
    dirtBlobMinRadius: 2,
    dirtBlobMaxRadius: 5,
    numLakes: 3,
    lakeMinRadius: 2,
    lakeMaxRadius: 3
  },
  buildings: {
    churchWidth: 4,
    churchHeight: 3,
    mineWidth: 6,
    mineHeight: 6,
    farmWidth: 6,
    farmHeight: 4
  },
  weather: {
    speedBlocksPer100Turns: 200,
    shapeNoiseCellSpan: 6,
    shapeNoiseAmplitudePercent: 100,
    edgeSoftnessPercent: 18,
    alphaBasePercent: 48,
    alphaMinPercent: 22,
    alphaMaxPercent: 82,
    densityMuTimes100: -12,
    densitySigmaTimes100: 35
  }
};

const WEATHER_DIRECTION_STEPS = {
  [WEATHER_DIRECTION_NORTH]: { x: 0, y: -1 },
  [WEATHER_DIRECTION_SOUTH]: { x: 0, y: 1 },
  [WEATHER_DIRECTION_EAST]: { x: 1, y: 0 },
  [WEATHER_DIRECTION_WEST]: { x: -1, y: 0 },
  [WEATHER_DIRECTION_NORTH_EAST]: { x: 1, y: -1 },
  [WEATHER_DIRECTION_NORTH_WEST]: { x: -1, y: -1 },
  [WEATHER_DIRECTION_SOUTH_EAST]: { x: 1, y: 1 },
  [WEATHER_DIRECTION_SOUTH_WEST]: { x: -1, y: 1 }
};

const illustrationReferenceData = {
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
    { id: 0, key: "church", label: "Church" },
    { id: 1, key: "mine", label: "Mine" },
    { id: 2, key: "farm", label: "Farm" }
  ],
  weatherDirections: [
    { id: WEATHER_DIRECTION_NORTH, key: "north", label: "North" },
    { id: WEATHER_DIRECTION_SOUTH, key: "south", label: "South" },
    { id: WEATHER_DIRECTION_EAST, key: "east", label: "East" },
    { id: WEATHER_DIRECTION_WEST, key: "west", label: "West" },
    { id: WEATHER_DIRECTION_NORTH_EAST, key: "north_east", label: "North East" },
    { id: WEATHER_DIRECTION_NORTH_WEST, key: "north_west", label: "North West" },
    { id: WEATHER_DIRECTION_SOUTH_EAST, key: "south_east", label: "South East" },
    { id: WEATHER_DIRECTION_SOUTH_WEST, key: "south_west", label: "South West" }
  ]
};

const publicStructureTemplates = {
  mine: {
    type: 1,
    width: GAME_CONFIG.buildings.mineWidth,
    height: GAME_CONFIG.buildings.mineHeight,
    anchorSourceLocal: { x: 2, y: 2 }
  },
  farm: {
    type: 2,
    width: GAME_CONFIG.buildings.farmWidth,
    height: GAME_CONFIG.buildings.farmHeight,
    anchorSourceLocal: { x: 2, y: 1 }
  }
};

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

const CHEST_EARLY_REWARD_WEIGHTS = Object.freeze([8, 3, 3]);
const CHEST_LATE_REWARD_WEIGHTS = Object.freeze([4, 6, 6]);

function deepClone(value) {
  return JSON.parse(JSON.stringify(value));
}

function clamp(value, min, max) {
  return Math.min(max, Math.max(min, value));
}

function clampInt(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function lerp(start, end, amount) {
  return start + ((end - start) * amount);
}

function smoothstep01(value) {
  const amount = clamp(value, 0, 1);
  return amount * amount * (3 - (2 * amount));
}

function regionCellCountFromRadius(radius) {
  const clampedRadius = Math.max(1, radius);
  return Math.max(1, Math.round(K_PI * clampedRadius * clampedRadius));
}

function toIndex(x, y, diameter) {
  return (y * diameter) + x;
}

function mixSeed(seed, value) {
  let mixed = (seed ^ ((value + 0x9e3779b9 + (seed << 6) + (seed >>> 2)) >>> 0)) >>> 0;
  mixed ^= mixed >>> 16;
  mixed = Math.imul(mixed, 0x7feb352d) >>> 0;
  mixed ^= mixed >>> 15;
  mixed = Math.imul(mixed, 0x846ca68b) >>> 0;
  mixed ^= mixed >>> 16;
  return mixed >>> 0;
}

function createSeededGenerator(seed) {
  let state = (seed >>> 0) || 1;

  function nextUint32() {
    state ^= state << 13;
    state >>>= 0;
    state ^= state >>> 17;
    state >>>= 0;
    state ^= state << 5;
    state >>>= 0;
    return state >>> 0;
  }

  return {
    nextUint32,
    nextFloat() {
      return (nextUint32() + 0.5) / 4294967296;
    },
    nextRangeInt(min, max) {
      if (max <= min) {
        return min;
      }
      return min + (nextUint32() % ((max - min) + 1));
    },
    chooseIndex(length) {
      if (length <= 1) {
        return 0;
      }
      return nextUint32() % length;
    }
  };
}

function shuffleArray(items, generator) {
  const result = items.slice();
  for (let index = result.length - 1; index > 0; index -= 1) {
    const swapIndex = generator.chooseIndex(index + 1);
    const current = result[index];
    result[index] = result[swapIndex];
    result[swapIndex] = current;
  }
  return result;
}

function hashValue(seed, x, y) {
  const hashed = mixSeed(
    seed >>> 0,
    (Math.imul(x | 0, 374761393) ^ Math.imul(y | 0, 668265263)) >>> 0
  );
  return (hashed & 0x00ffffff) / 0x00ffffff;
}

function latticeValueNoise(seed, x, y) {
  const x0 = Math.floor(x);
  const y0 = Math.floor(y);
  const x1 = x0 + 1;
  const y1 = y0 + 1;
  const tx = smoothstep01(x - x0);
  const ty = smoothstep01(y - y0);

  const top = lerp(hashValue(seed, x0, y0), hashValue(seed, x1, y0), tx);
  const bottom = lerp(hashValue(seed, x0, y1), hashValue(seed, x1, y1), tx);
  return lerp(top, bottom, ty);
}

function fractalNoise(seed, x, y, octaves) {
  let value = 0;
  let amplitude = 0.5;
  let frequency = 1;
  let amplitudeSum = 0;

  for (let octave = 0; octave < octaves; octave += 1) {
    value += latticeValueNoise(mixSeed(seed, octave + 1), x * frequency, y * frequency) * amplitude;
    amplitudeSum += amplitude;
    amplitude *= 0.5;
    frequency *= 2;
  }

  return amplitudeSum > 0 ? value / amplitudeSum : 0;
}

function sampleGammaIntegerShape(generator, shape) {
  let product = 1;
  for (let index = 0; index < shape; index += 1) {
    product *= Math.max(generator.nextFloat(), EPSILON);
  }
  return -Math.log(product);
}

function sampleBeta72(generator) {
  const first = sampleGammaIntegerShape(generator, 7);
  const second = sampleGammaIntegerShape(generator, 2);
  return first / Math.max(first + second, EPSILON);
}

function terrainBrightnessFor(worldSeed, x, y, terrainType) {
  if (terrainType !== TERRAIN_GRASS) {
    return 255;
  }

  let seed = (worldSeed >>> 0) || 1;
  seed = mixSeed(seed, 0x6d2b79f5);
  const positionHash = (Math.imul((x + 1) | 0, 83492791) ^ Math.imul((y + 1) | 0, 2971215073)) >>> 0;
  seed = mixSeed(seed, positionHash);

  const generator = createSeededGenerator(seed);
  const betaSample = sampleBeta72(generator);
  let brightness = 1;
  if (betaSample < 0.9) {
    const normalized = clamp(betaSample / 0.9, 0, 1);
    const contrasted = Math.pow(normalized, 1.8);
    brightness = 0.68 + ((1 - 0.68) * contrasted);
  }

  return clampInt(Math.round(clamp(brightness, 0, 1) * 255), 0, 255);
}

function terrainFlipMaskFor(worldSeed, x, y, terrainType) {
  if (terrainType === TERRAIN_VOID) {
    return 0;
  }

  let seed = (worldSeed >>> 0) || 1;
  seed = mixSeed(seed, (terrainType + 1) >>> 0);
  const positionHash = (Math.imul((x + 1) | 0, 73856093) ^ Math.imul((y + 1) | 0, 19349663)) >>> 0;
  seed = mixSeed(seed, positionHash);
  return seed & (FLIP_HORIZONTAL_MASK | FLIP_VERTICAL_MASK);
}

function createTerrainCell(type, isInCircle) {
  return {
    t: type,
    c: isInCircle ? 1 : 0,
    f: 0,
    b: 255
  };
}

function createGameLikeBoardGrid(radius = BOARD_RADIUS) {
  const diameter = radius * 2;
  const centerX = radius;
  const centerY = radius;

  return Array.from({ length: diameter }, function (_, y) {
    return Array.from({ length: diameter }, function (_, x) {
      const dx = x - centerX + 0.5;
      const dy = y - centerY + 0.5;
      const distance = Math.sqrt((dx * dx) + (dy * dy));
      const isInCircle = distance <= radius;
      return createTerrainCell(isInCircle ? TERRAIN_GRASS : TERRAIN_VOID, isInCircle);
    });
  });
}

function cloneGrid(grid) {
  return grid.map(function (row) {
    return row.map(function (cell) {
      return { ...cell };
    });
  });
}

function getAllValidCells(grid) {
  const cells = [];
  for (let y = 0; y < grid.length; y += 1) {
    const row = grid[y];
    for (let x = 0; x < row.length; x += 1) {
      if (row[x] && row[x].c) {
        cells.push({ x, y });
      }
    }
  }
  return cells;
}

function isInBounds(grid, x, y) {
  return y >= 0 && y < grid.length && x >= 0 && x < grid[y].length;
}

function computeRadialDistance(x, y, radius) {
  const dx = (x - radius) / Math.max(1, radius);
  const dy = (y - radius) / Math.max(1, radius);
  return Math.sqrt((dx * dx) + (dy * dy));
}

function applyTerrainBrightness(grid, worldSeed) {
  for (let y = 0; y < grid.length; y += 1) {
    for (let x = 0; x < grid[y].length; x += 1) {
      const cell = grid[y][x];
      if (!cell.c || cell.t === TERRAIN_VOID) {
        cell.b = 255;
        continue;
      }
      cell.b = terrainBrightnessFor(worldSeed, x, y, cell.t);
    }
  }
}

function applyTerrainFlipMasks(grid, worldSeed) {
  for (let y = 0; y < grid.length; y += 1) {
    for (let x = 0; x < grid[y].length; x += 1) {
      const cell = grid[y][x];
      if (!cell.c || cell.t === TERRAIN_VOID) {
        cell.f = 0;
        continue;
      }
      cell.f = terrainFlipMaskFor(worldSeed, x, y, cell.t);
    }
  }
}

function applyTerrainVisuals(grid, worldSeed) {
  applyTerrainFlipMasks(grid, worldSeed);
  applyTerrainBrightness(grid, worldSeed);
}

function isProtectedTerrainColumn(x, spawnLeftMax, spawnRightMin, clearance) {
  return x <= (spawnLeftMax + clearance) || x >= (spawnRightMin - clearance);
}

function selectThreshold(scores, targetCells, minimumThreshold) {
  if (!scores.length || targetCells <= 0) {
    return Number.POSITIVE_INFINITY;
  }

  const clampedTarget = Math.min(targetCells, scores.length);
  const ordered = scores.slice().sort(function (left, right) {
    return right - left;
  });
  return Math.max(minimumThreshold, ordered[clampedTarget - 1]);
}

function pruneSparseMask(mask, grid, minNeighbours, passes) {
  const diameter = grid.length;
  const neighbourOffsets = [
    { x: 1, y: 0 },
    { x: 1, y: 1 },
    { x: 0, y: 1 },
    { x: -1, y: 1 },
    { x: -1, y: 0 },
    { x: -1, y: -1 },
    { x: 0, y: -1 },
    { x: 1, y: -1 }
  ];

  for (let pass = 0; pass < passes; pass += 1) {
    const next = mask.slice();
    for (let y = 0; y < diameter; y += 1) {
      for (let x = 0; x < diameter; x += 1) {
        const index = toIndex(x, y, diameter);
        if (!mask[index]) {
          continue;
        }

        let neighbours = 0;
        for (const offset of neighbourOffsets) {
          const nx = x + offset.x;
          const ny = y + offset.y;
          if (!isInBounds(grid, nx, ny)) {
            continue;
          }
          if (mask[toIndex(nx, ny, diameter)]) {
            neighbours += 1;
          }
        }

        if (neighbours < minNeighbours) {
          next[index] = false;
        }
      }
    }
    for (let index = 0; index < mask.length; index += 1) {
      mask[index] = next[index];
    }
  }
}

function extractComponents(grid, mask, scores) {
  const diameter = grid.length;
  const visited = Array.from({ length: mask.length }, function () {
    return false;
  });
  const components = [];
  const neighbourOffsets = [
    { x: 1, y: 0 },
    { x: 1, y: 1 },
    { x: 0, y: 1 },
    { x: -1, y: 1 },
    { x: -1, y: 0 },
    { x: -1, y: -1 },
    { x: 0, y: -1 },
    { x: 1, y: -1 }
  ];

  for (let y = 0; y < diameter; y += 1) {
    for (let x = 0; x < diameter; x += 1) {
      const startIndex = toIndex(x, y, diameter);
      if (!mask[startIndex] || visited[startIndex]) {
        continue;
      }

      const queue = [{ x, y }];
      visited[startIndex] = true;
      const component = {
        cells: [],
        averageScore: 0
      };
      let scoreSum = 0;

      while (queue.length) {
        const current = queue.shift();
        const currentIndex = toIndex(current.x, current.y, diameter);
        component.cells.push(current);
        scoreSum += scores[currentIndex];

        for (const offset of neighbourOffsets) {
          const nx = current.x + offset.x;
          const ny = current.y + offset.y;
          if (!isInBounds(grid, nx, ny)) {
            continue;
          }
          const nextIndex = toIndex(nx, ny, diameter);
          if (!mask[nextIndex] || visited[nextIndex]) {
            continue;
          }
          visited[nextIndex] = true;
          queue.push({ x: nx, y: ny });
        }
      }

      component.averageScore = scoreSum / Math.max(component.cells.length, 1);
      components.push(component);
    }
  }

  return components;
}

function limitCellsByScore(cells, scores, diameter, maxCells) {
  if (cells.length <= maxCells) {
    return cells.slice();
  }

  return cells.slice().sort(function (left, right) {
    return scores[toIndex(right.x, right.y, diameter)] - scores[toIndex(left.x, left.y, diameter)];
  }).slice(0, maxCells);
}

function selectComponents(components, scores, diameter, maxRegions, minCells, maxCells, targetCells) {
  if (!components.length || maxRegions <= 0 || targetCells <= 0) {
    return [];
  }

  const ordered = components.slice().sort(function (left, right) {
    if (left.averageScore !== right.averageScore) {
      return right.averageScore - left.averageScore;
    }
    return right.cells.length - left.cells.length;
  });

  const selected = [];
  let selectedCells = 0;
  for (const component of ordered) {
    if (selected.length >= maxRegions) {
      break;
    }

    const clippedCells = limitCellsByScore(component.cells, scores, diameter, maxCells);
    if (clippedCells.length < minCells) {
      continue;
    }

    let scoreSum = 0;
    for (const cell of clippedCells) {
      scoreSum += scores[toIndex(cell.x, cell.y, diameter)];
    }

    selected.push({
      cells: clippedCells,
      averageScore: scoreSum / Math.max(clippedCells.length, 1)
    });
    selectedCells += clippedCells.length;
    if (selectedCells >= targetCells) {
      break;
    }
  }

  if (selected.length) {
    return selected;
  }

  const fallbackCells = limitCellsByScore(ordered[0].cells, scores, diameter, maxCells);
  if (fallbackCells.length < minCells) {
    return [];
  }

  let fallbackScore = 0;
  for (const cell of fallbackCells) {
    fallbackScore += scores[toIndex(cell.x, cell.y, diameter)];
  }

  return [{
    cells: fallbackCells,
    averageScore: fallbackScore / Math.max(fallbackCells.length, 1)
  }];
}

function applyComponents(grid, components, terrainType) {
  for (const component of components) {
    for (const cell of component.cells) {
      grid[cell.y][cell.x].t = terrainType;
    }
  }
}

function setRegionType(grid, cells, terrainType) {
  const modified = [];
  for (const cell of cells) {
    modified.push({ cell, previousType: grid[cell.y][cell.x].t });
    grid[cell.y][cell.x].t = terrainType;
  }
  return modified;
}

function restoreRegionType(grid, modified) {
  for (const entry of modified) {
    grid[entry.cell.y][entry.cell.x].t = entry.previousType;
  }
}

function findNearestLandCell(grid, target) {
  if (isInBounds(grid, target.x, target.y)) {
    const targetCell = grid[target.y][target.x];
    if (targetCell.c && targetCell.t !== TERRAIN_WATER) {
      return { x: target.x, y: target.y };
    }
  }

  const diameter = grid.length;
  for (let searchRadius = 1; searchRadius < diameter; searchRadius += 1) {
    for (let dy = -searchRadius; dy <= searchRadius; dy += 1) {
      for (let dx = -searchRadius; dx <= searchRadius; dx += 1) {
        const nx = target.x + dx;
        const ny = target.y + dy;
        if (!isInBounds(grid, nx, ny)) {
          continue;
        }
        const cell = grid[ny][nx];
        if (cell.c && cell.t !== TERRAIN_WATER) {
          return { x: nx, y: ny };
        }
      }
    }
  }

  return { x: BOARD_RADIUS, y: BOARD_RADIUS };
}

function isIllustrationBorderCell(grid, position) {
  if (!isInBounds(grid, position.x, position.y)) {
    return false;
  }

  const cell = grid[position.y][position.x];
  if (!cell.c || cell.t === TERRAIN_WATER) {
    return false;
  }

  const offsets = [
    { x: 1, y: 0 },
    { x: -1, y: 0 },
    { x: 0, y: 1 },
    { x: 0, y: -1 }
  ];

  return offsets.some(function (offset) {
    const nx = position.x + offset.x;
    const ny = position.y + offset.y;
    return !isInBounds(grid, nx, ny) || !grid[ny][nx].c;
  });
}

function findNearestBorderLandCell(grid, target) {
  if (isIllustrationBorderCell(grid, target)) {
    return { x: target.x, y: target.y };
  }

  const diameter = grid.length;
  for (let searchRadius = 1; searchRadius < diameter; searchRadius += 1) {
    for (let dy = -searchRadius; dy <= searchRadius; dy += 1) {
      for (let dx = -searchRadius; dx <= searchRadius; dx += 1) {
        const nx = target.x + dx;
        const ny = target.y + dy;
        if (isIllustrationBorderCell(grid, { x: nx, y: ny })) {
          return { x: nx, y: ny };
        }
      }
    }
  }

  return findNearestLandCell(grid, target);
}

function collectIllustrationBorderLandCells(grid) {
  const cells = [];

  for (let y = 0; y < grid.length; y += 1) {
    for (let x = 0; x < grid[y].length; x += 1) {
      if (isIllustrationBorderCell(grid, { x, y })) {
        cells.push({ x, y });
      }
    }
  }

  return cells;
}

const ILLUSTRATION_ROOK_DIRECTIONS = [
  { x: 1, y: 0 },
  { x: -1, y: 0 },
  { x: 0, y: 1 },
  { x: 0, y: -1 }
];

const ILLUSTRATION_QUEEN_DIRECTIONS = [
  { x: 1, y: 0 },
  { x: -1, y: 0 },
  { x: 0, y: 1 },
  { x: 0, y: -1 },
  { x: 1, y: 1 },
  { x: 1, y: -1 },
  { x: -1, y: 1 },
  { x: -1, y: -1 }
];

function enumerateIllustrationSlidingMoves(grid, start, directions) {
  const moves = [];

  directions.forEach(function (direction) {
    let x = start.x + direction.x;
    let y = start.y + direction.y;

    while (isInBounds(grid, x, y)) {
      const cell = grid[y][x];
      if (!cell.c || cell.t === TERRAIN_WATER) {
        break;
      }
      moves.push({ x, y });
      x += direction.x;
      y += direction.y;
    }
  });

  return moves;
}

function enumerateIllustrationRookMoves(grid, start) {
  return enumerateIllustrationSlidingMoves(grid, start, ILLUSTRATION_ROOK_DIRECTIONS);
}

function buildIllustrationShortestPath(grid, start, goal, enumerateMoves) {
  const diameter = grid.length;
  const distances = Array.from({ length: diameter * diameter }, function () {
    return -1;
  });
  const previous = Array.from({ length: diameter * diameter }, function () {
    return -1;
  });
  const frontier = [start];
  distances[toIndex(start.x, start.y, diameter)] = 0;

  if (start.x === goal.x && start.y === goal.y) {
    return [];
  }

  while (frontier.length) {
    const current = frontier.shift();
    const currentDistance = distances[toIndex(current.x, current.y, diameter)];

    for (const move of enumerateMoves(grid, current)) {
      const index = toIndex(move.x, move.y, diameter);
      if (distances[index] >= 0) {
        continue;
      }

      distances[index] = currentDistance + 1;
      previous[index] = toIndex(current.x, current.y, diameter);
      if (move.x === goal.x && move.y === goal.y) {
        const path = [];
        let currentIndex = index;

        while (currentIndex >= 0) {
          const x = currentIndex % diameter;
          const y = Math.floor(currentIndex / diameter);
          path.push({ x, y });
          currentIndex = previous[currentIndex];
        }

        path.reverse();
        return path.slice(1);
      }

      frontier.push(move);
    }
  }

  return [];
}

function collectIllustrationTargetLandCells(grid) {
  const center = { x: BOARD_RADIUS, y: BOARD_RADIUS };

  return getAllValidCells(grid).filter(function (cell) {
    if (grid[cell.y][cell.x].t === TERRAIN_WATER || isIllustrationBorderCell(grid, cell)) {
      return false;
    }

    return euclideanDistance(cell, center) <= BOARD_RADIUS * 0.78;
  });
}

function sampleIllustrationTargetPositions(grid, sampleCount, seed) {
  const generator = createSeededGenerator(seed >>> 0);
  const shuffled = shuffleArray(collectIllustrationTargetLandCells(grid), generator);
  const selections = [];

  for (const candidate of shuffled) {
    if (selections.every(function (selected) {
      return euclideanDistance(selected, candidate) >= 8;
    })) {
      selections.push(candidate);
    }

    if (selections.length >= sampleCount) {
      break;
    }
  }

  if (selections.length < sampleCount) {
    for (const candidate of shuffled) {
      if (selections.some(function (selected) {
        return selected.x === candidate.x && selected.y === candidate.y;
      })) {
        continue;
      }

      selections.push(candidate);
      if (selections.length >= sampleCount) {
        break;
      }
    }
  }

  return selections.slice(0, sampleCount).map(function (candidate) {
    return { x: candidate.x, y: candidate.y };
  });
}

function chooseInfernalRookSpawnOption(grid, targetPosition, generator) {
  const spawnOptions = collectIllustrationBorderLandCells(grid)
    .map(function (cell) {
      const path = buildIllustrationShortestPath(grid, cell, targetPosition, enumerateIllustrationRookMoves);
      return {
        cell,
        path,
        weight: path.length ? Math.max(1, (grid.length * 2) - path.length + 1) : 0
      };
    })
    .filter(function (option) {
      return option.path.length > 0;
    });

  if (!spawnOptions.length) {
    return null;
  }

  const chosenIndex = sampleWeightedIndex(
    spawnOptions.map(function (option) {
      return option.weight;
    }),
    generator
  );

  return spawnOptions[Math.max(0, Math.min(chosenIndex, spawnOptions.length - 1))];
}

function buildInfernalRookHuntFrames(grid, targetPosition, spawnOption, sessionId) {
  const queenId = 980 + sessionId;
  const infernalId = 1180 + sessionId;
  const targetQueen = createPiece({
    id: queenId,
    type: PIECE_QUEEN,
    kingdom: 1,
    x: targetPosition.x,
    y: targetPosition.y
  });

  const frames = [
    {
      grid,
      blackPieces: [targetQueen],
      autonomousUnits: [
        createInfernalUnit({
          id: infernalId,
          pieceType: PIECE_ROOK,
          x: spawnOption.cell.x,
          y: spawnOption.cell.y,
          targetKingdom: 1,
          phase: INFERNAL_PHASE_HUNTING
        })
      ]
    }
  ];

  spawnOption.path.forEach(function (step, stepIndex) {
    const captured = stepIndex === spawnOption.path.length - 1;
    frames.push({
      grid,
      blackPieces: captured ? [] : [targetQueen],
      autonomousUnits: [
        createInfernalUnit({
          id: infernalId,
          pieceType: PIECE_ROOK,
          x: step.x,
          y: step.y,
          targetKingdom: 1,
          phase: INFERNAL_PHASE_HUNTING
        })
      ]
    });
  });

  return frames;
}

function isConnected(grid, from, to) {
  if (from.x === to.x && from.y === to.y) {
    return true;
  }

  const visited = Array.from({ length: grid.length }, function () {
    return Array.from({ length: grid.length }, function () {
      return false;
    });
  });
  const queue = [from];
  visited[from.y][from.x] = true;
  const offsets = [
    { x: 0, y: 1 },
    { x: 0, y: -1 },
    { x: 1, y: 0 },
    { x: -1, y: 0 }
  ];

  while (queue.length) {
    const current = queue.shift();
    if (current.x === to.x && current.y === to.y) {
      return true;
    }

    for (const offset of offsets) {
      const nx = current.x + offset.x;
      const ny = current.y + offset.y;
      if (!isInBounds(grid, nx, ny) || visited[ny][nx]) {
        continue;
      }

      const cell = grid[ny][nx];
      if (!cell.c || cell.t === TERRAIN_WATER) {
        continue;
      }

      visited[ny][nx] = true;
      queue.push({ x: nx, y: ny });
    }
  }

  return false;
}

function generateTerrainGrid({
  terrainWorldSeed = 1,
  dirtNoiseSeed = null,
  waterNoiseSeed = null,
  terrainVisualSeed = terrainWorldSeed,
  dirtCoveragePercent = GAME_CONFIG.map.dirtCoveragePercent,
  waterCoveragePercent = GAME_CONFIG.map.waterCoveragePercent,
  numDirtBlobs = GAME_CONFIG.map.numDirtBlobs,
  numLakes = GAME_CONFIG.map.numLakes,
  preserveSpawnCorridor = true
} = {}) {
  const grid = createGameLikeBoardGrid(BOARD_RADIUS);
  const diameter = grid.length;
  const radius = BOARD_RADIUS;
  const worldGenerator = createSeededGenerator((terrainWorldSeed >>> 0) || 1);
  const resolvedDirtNoiseSeed = dirtNoiseSeed == null ? worldGenerator.nextUint32() : dirtNoiseSeed >>> 0;
  const resolvedWaterNoiseSeed = waterNoiseSeed == null ? worldGenerator.nextUint32() : waterNoiseSeed >>> 0;

  const playerZoneWidth = Math.max(2, Math.floor(diameter * clampInt(GAME_CONFIG.map.playerSpawnZonePercent, 10, 45) / 100));
  const aiZoneWidth = Math.max(2, Math.floor(diameter * clampInt(GAME_CONFIG.map.aiSpawnZonePercent, 10, 45) / 100));
  const spawnLeftMax = clampInt(playerZoneWidth, 1, diameter - 3);
  const spawnRightMin = clampInt(diameter - aiZoneWidth - 1, spawnLeftMax + 3, diameter - 2);
  const terrainClearance = Math.max(2, Math.floor(radius / 10));

  const dirtScores = Array.from({ length: diameter * diameter }, function () {
    return -1;
  });
  const waterScores = Array.from({ length: diameter * diameter }, function () {
    return -1;
  });
  const dirtCandidateScores = [];
  const waterCandidateScores = [];
  let traversableCells = 0;

  for (let y = 0; y < diameter; y += 1) {
    for (let x = 0; x < diameter; x += 1) {
      const cell = grid[y][x];
      if (!cell.c) {
        continue;
      }

      traversableCells += 1;
      const index = toIndex(x, y, diameter);
      const localX = (x - radius) / GAME_CONFIG.map.terrainNoiseScale;
      const localY = (y - radius) / GAME_CONFIG.map.terrainNoiseScale;
      const radialDistance = computeRadialDistance(x, y, radius);
      const edgePenalty = Math.max(0, radialDistance - 0.82) * 0.35;
      const protectedColumn = preserveSpawnCorridor && isProtectedTerrainColumn(x, spawnLeftMax, spawnRightMin, terrainClearance);

      const dirtMacro = fractalNoise(resolvedDirtNoiseSeed, localX + 11.7, localY - 4.9, GAME_CONFIG.map.terrainOctaves);
      const dirtDetail = fractalNoise(
        (resolvedDirtNoiseSeed ^ 0x9e3779b9) >>> 0,
        (localX * 2.15) - 6.2,
        (localY * 2.15) + 9.1,
        Math.max(1, GAME_CONFIG.map.terrainOctaves - 1)
      );
      dirtScores[index] = (dirtMacro * 0.72) + (dirtDetail * 0.28) - edgePenalty;

      const waterMacro = fractalNoise(resolvedWaterNoiseSeed, (localX * 1.12) + 3.4, (localY * 1.12) - 7.8, GAME_CONFIG.map.terrainOctaves);
      const waterDetail = fractalNoise(
        (resolvedWaterNoiseSeed ^ 0x85ebca6b) >>> 0,
        (localX * 2.55) + 5.7,
        (localY * 2.55) - 1.9,
        Math.max(1, GAME_CONFIG.map.terrainOctaves - 1)
      );
      const waterRingBias = Math.max(0, 1 - Math.abs(radialDistance - 0.58));
      waterScores[index] = (waterMacro * 0.67) + (waterDetail * 0.33) + (waterRingBias * 0.08);

      if (!protectedColumn) {
        dirtCandidateScores.push(dirtScores[index]);
        waterCandidateScores.push(waterScores[index]);
      }
    }
  }

  const targetDirtCells = Math.floor(traversableCells * clampInt(dirtCoveragePercent, 0, 40) / 100);
  const targetWaterCells = Math.floor(traversableCells * clampInt(waterCoveragePercent, 0, 12) / 100);
  const dirtThreshold = selectThreshold(dirtCandidateScores, targetDirtCells, 0.57);
  const waterThreshold = selectThreshold(waterCandidateScores, targetWaterCells, 0.69);
  const dirtMask = Array.from({ length: diameter * diameter }, function () {
    return false;
  });
  const waterMask = Array.from({ length: diameter * diameter }, function () {
    return false;
  });

  for (let y = 0; y < diameter; y += 1) {
    for (let x = 0; x < diameter; x += 1) {
      const cell = grid[y][x];
      if (!cell.c || (preserveSpawnCorridor && isProtectedTerrainColumn(x, spawnLeftMax, spawnRightMin, terrainClearance))) {
        continue;
      }
      const index = toIndex(x, y, diameter);
      dirtMask[index] = dirtScores[index] >= dirtThreshold;
      waterMask[index] = waterScores[index] >= waterThreshold;
    }
  }

  pruneSparseMask(dirtMask, grid, 2, 2);
  pruneSparseMask(waterMask, grid, 3, 2);

  const dirtComponents = extractComponents(grid, dirtMask, dirtScores);
  const waterComponents = extractComponents(grid, waterMask, waterScores);
  const desiredDirtPerRegion = Math.max(1, Math.floor(targetDirtCells / Math.max(1, numDirtBlobs)));
  const desiredWaterPerRegion = Math.max(1, Math.floor(targetWaterCells / Math.max(1, numLakes)));
  const minDirtCells = Math.max(4, Math.min(Math.floor(regionCellCountFromRadius(GAME_CONFIG.map.dirtBlobMinRadius) / 2), desiredDirtPerRegion));
  const maxDirtCells = Math.max(regionCellCountFromRadius(GAME_CONFIG.map.dirtBlobMaxRadius), desiredDirtPerRegion + Math.floor(desiredDirtPerRegion / 2));
  const minWaterCells = Math.max(3, Math.min(Math.floor(regionCellCountFromRadius(GAME_CONFIG.map.lakeMinRadius) / 2), desiredWaterPerRegion));
  const maxWaterCells = Math.max(regionCellCountFromRadius(GAME_CONFIG.map.lakeMaxRadius), desiredWaterPerRegion + Math.floor(desiredWaterPerRegion / 2));

  const selectedDirt = selectComponents(
    dirtComponents,
    dirtScores,
    diameter,
    Math.max(1, numDirtBlobs),
    minDirtCells,
    maxDirtCells,
    targetDirtCells
  );
  const selectedWater = selectComponents(
    waterComponents,
    waterScores,
    diameter,
    Math.max(1, numLakes),
    minWaterCells,
    maxWaterCells,
    targetWaterCells
  );

  applyComponents(grid, selectedDirt, TERRAIN_DIRT);
  for (const waterRegion of selectedWater) {
    const modified = setRegionType(grid, waterRegion.cells, TERRAIN_WATER);
    const leftTarget = findNearestLandCell(grid, {
      x: Math.max(1, Math.floor(spawnLeftMax / 2)),
      y: radius
    });
    const rightTarget = findNearestLandCell(grid, {
      x: Math.min(diameter - 2, Math.floor(spawnRightMin + ((diameter - 1 - spawnRightMin) / 2))),
      y: radius
    });
    if (!isConnected(grid, leftTarget, rightTarget)) {
      restoreRegionType(grid, modified);
    }
  }

  applyTerrainVisuals(grid, terrainVisualSeed >>> 0);
  return {
    grid,
    metadata: {
      spawnLeftMax,
      spawnRightMin,
      dirtNoiseSeed: resolvedDirtNoiseSeed,
      waterNoiseSeed: resolvedWaterNoiseSeed
    }
  };
}

function createGridWithBrightnessSeed(baseGrid, brightnessSeed) {
  const grid = cloneGrid(baseGrid);
  applyTerrainBrightness(grid, brightnessSeed >>> 0);
  return grid;
}

function createGridWithFlipSeed(baseGrid, flipSeed) {
  const grid = cloneGrid(baseGrid);
  applyTerrainFlipMasks(grid, flipSeed >>> 0);
  return grid;
}

function normalizeRotationQuarterTurns(rotationQuarterTurns) {
  if (!Number.isFinite(rotationQuarterTurns) || rotationQuarterTurns < 0) {
    return 0;
  }

  return Math.trunc(rotationQuarterTurns) % 4;
}

function normalizeFlipMask(flipMask) {
  if (!Number.isFinite(flipMask) || flipMask < 0) {
    return 0;
  }

  return Math.trunc(flipMask) & (FLIP_HORIZONTAL_MASK | FLIP_VERTICAL_MASK);
}

function getFootprintWidthFor(baseWidth, baseHeight, rotationQuarterTurns) {
  return normalizeRotationQuarterTurns(rotationQuarterTurns) % 2 === 0 ? baseWidth : baseHeight;
}

function getFootprintHeightFor(baseWidth, baseHeight, rotationQuarterTurns) {
  return normalizeRotationQuarterTurns(rotationQuarterTurns) % 2 === 0 ? baseHeight : baseWidth;
}

function mapFootprintToSourceLocalFor(localX, localY, baseWidth, baseHeight, rotationQuarterTurns, flipMask) {
  const normalizedRotation = normalizeRotationQuarterTurns(rotationQuarterTurns);
  const footprintWidth = getFootprintWidthFor(baseWidth, baseHeight, normalizedRotation);
  const footprintHeight = getFootprintHeightFor(baseWidth, baseHeight, normalizedRotation);
  if (localX < 0 || localY < 0 || localX >= footprintWidth || localY >= footprintHeight) {
    return { x: -1, y: -1 };
  }

  let sourceX = 0;
  let sourceY = 0;
  switch (normalizedRotation) {
    case 0:
      sourceX = localX;
      sourceY = localY;
      break;
    case 1:
      sourceX = localY;
      sourceY = baseHeight - 1 - localX;
      break;
    case 2:
      sourceX = baseWidth - 1 - localX;
      sourceY = baseHeight - 1 - localY;
      break;
    case 3:
      sourceX = baseWidth - 1 - localY;
      sourceY = localX;
      break;
    default:
      break;
  }

  const normalizedMask = normalizeFlipMask(flipMask);
  if ((normalizedMask & FLIP_HORIZONTAL_MASK) !== 0) {
    sourceX = baseWidth - 1 - sourceX;
  }
  if ((normalizedMask & FLIP_VERTICAL_MASK) !== 0) {
    sourceY = baseHeight - 1 - sourceY;
  }

  return { x: sourceX, y: sourceY };
}

function mapSourceLocalToFootprintLocal(sourceLocal, baseWidth, baseHeight, rotationQuarterTurns, flipMask) {
  const footprintWidth = getFootprintWidthFor(baseWidth, baseHeight, rotationQuarterTurns);
  const footprintHeight = getFootprintHeightFor(baseWidth, baseHeight, rotationQuarterTurns);
  for (let localY = 0; localY < footprintHeight; localY += 1) {
    for (let localX = 0; localX < footprintWidth; localX += 1) {
      const candidate = mapFootprintToSourceLocalFor(
        localX,
        localY,
        baseWidth,
        baseHeight,
        rotationQuarterTurns,
        flipMask
      );
      if (candidate.x === sourceLocal.x && candidate.y === sourceLocal.y) {
        return { x: localX, y: localY };
      }
    }
  }

  return { x: 0, y: 0 };
}

function createPublicBuilding({ id, type, ox, oy, w, h, rot = 0, fm = 0 }) {
  const cellCount = w * h;
  return {
    id,
    type,
    owner: 0,
    isNeutral: true,
    ox,
    oy,
    w,
    h,
    rot,
    fm,
    state: 0,
    destroyedCellsRequired: 1,
    isProducing: false,
    producingType: 0,
    turnsRemaining: 0,
    hp: Array.from({ length: cellCount }, function () {
      return 999;
    }),
    breach: Array.from({ length: cellCount }, function () {
      return 0;
    })
  };
}

function createAnchoredPublicBuilding({ id, kind, anchorCell, rotation = 0, flipMask = 0 }) {
  const template = publicStructureTemplates[kind];
  const anchorFootprintLocal = mapSourceLocalToFootprintLocal(
    template.anchorSourceLocal,
    template.width,
    template.height,
    rotation,
    flipMask
  );
  return createPublicBuilding({
    id,
    type: template.type,
    ox: anchorCell.x - anchorFootprintLocal.x,
    oy: anchorCell.y - anchorFootprintLocal.y,
    w: template.width,
    h: template.height,
    rot: rotation,
    fm: flipMask
  });
}

function createCenteredChurchBuilding(id = 0) {
  const width = GAME_CONFIG.buildings.churchWidth;
  const height = GAME_CONFIG.buildings.churchHeight;
  return createPublicBuilding({
    id,
    type: 0,
    ox: BOARD_RADIUS - Math.floor(width / 2),
    oy: BOARD_RADIUS - Math.floor(height / 2),
    w: width,
    h: height
  });
}

function createPiece({ id, type, kingdom, x, y }) {
  return {
    id,
    type,
    kingdom,
    x,
    y,
    xp: 0,
    formationId: -1,
    wallBreachEntryDx: 0,
    wallBreachEntryDy: 0,
    hasWallBreachEntry: false,
    wallBreachCellX: -1,
    wallBreachCellY: -1
  };
}

function createKing({ id, kingdom, x, y }) {
  return createPiece({ id, type: PIECE_KING, kingdom, x, y });
}

function createInfernalUnit({ id, pieceType, x, y, targetKingdom = 0, phase = INFERNAL_PHASE_HUNTING }) {
  return {
    id,
    x,
    y,
    pieceType,
    targetKingdom,
    phase,
    spawnTurn: 0
  };
}

function createChest({ id, x, y }) {
  return {
    id,
    type: 0,
    x,
    y
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

function resolveBuildingFootprint(building) {
  return {
    width: getFootprintWidthFor(building.w, building.h, building.rot || 0),
    height: getFootprintHeightFor(building.w, building.h, building.rot || 0)
  };
}

function buildingOccupiesCell(building, x, y) {
  const footprint = resolveBuildingFootprint(building);
  return x >= building.ox
    && x < building.ox + footprint.width
    && y >= building.oy
    && y < building.oy + footprint.height;
}

function canPlaceBuildingOnGrid(grid, existingBuildings, origin, width, height) {
  for (let dy = 0; dy < height; dy += 1) {
    for (let dx = 0; dx < width; dx += 1) {
      const x = origin.x + dx;
      const y = origin.y + dy;
      if (!isInBounds(grid, x, y)) {
        return false;
      }
      const cell = grid[y][x];
      if (!cell.c || cell.t === TERRAIN_WATER) {
        return false;
      }
      if (existingBuildings.some(function (building) {
        return buildingOccupiesCell(building, x, y);
      })) {
        return false;
      }
    }
  }
  return true;
}

function footprintCenter(origin, width, height) {
  return {
    x: origin.x + (width * 0.5),
    y: origin.y + (height * 0.5)
  };
}

function centerDistance(origin, width, height, building) {
  const candidateCenter = footprintCenter(origin, width, height);
  const footprint = resolveBuildingFootprint(building);
  const buildingCenter = footprintCenter({ x: building.ox, y: building.oy }, footprint.width, footprint.height);
  const dx = candidateCenter.x - buildingCenter.x;
  const dy = candidateCenter.y - buildingCenter.y;
  return Math.sqrt((dx * dx) + (dy * dy));
}

function scorePublicBuildingCandidate(existing, buildingType, origin, width, height) {
  if (!existing.length) {
    return 0;
  }

  let nearestAnyDistance = Number.POSITIVE_INFINITY;
  let nearestSameTypeDistance = Number.POSITIVE_INFINITY;
  let totalDistance = 0;

  for (const building of existing) {
    const distance = centerDistance(origin, width, height, building);
    nearestAnyDistance = Math.min(nearestAnyDistance, distance);
    totalDistance += distance;
    if (building.type === buildingType) {
      nearestSameTypeDistance = Math.min(nearestSameTypeDistance, distance);
    }
  }

  if (!Number.isFinite(nearestSameTypeDistance)) {
    nearestSameTypeDistance = nearestAnyDistance;
  }

  const averageDistance = totalDistance / existing.length;
  return (nearestAnyDistance * 3.5) + (nearestSameTypeDistance * 2.0) + (averageDistance * 0.35);
}

function selectDispersedCandidate(candidates, existing, width, height, buildingType, generator) {
  if (!candidates.length) {
    return { x: -1, y: -1 };
  }

  if (!existing.length) {
    return candidates[generator.chooseIndex(candidates.length)];
  }

  const scoredCandidates = candidates.map(function (candidate) {
    return {
      pos: candidate,
      score: scorePublicBuildingCandidate(existing, buildingType, candidate, width, height)
    };
  }).sort(function (left, right) {
    return right.score - left.score;
  });

  const topCount = Math.min(scoredCandidates.length, Math.max(3, Math.floor((scoredCandidates.length + 5) / 6)));
  return scoredCandidates[generator.chooseIndex(topCount)].pos;
}

function findValidBuildingPos(grid, existing, width, height, minDistance, avoidLeftX, avoidRightX, buildingType, generator) {
  const diameter = grid.length;
  const fallback = {
    x: BOARD_RADIUS - Math.floor(width / 2),
    y: BOARD_RADIUS - Math.floor(height / 2)
  };

  function collectCandidates(enforceMinDistance) {
    const candidates = [];
    for (let y = 1; y + height < diameter; y += 1) {
      for (let x = Math.max(1, avoidLeftX); x + width <= Math.min(avoidRightX, diameter - 1); x += 1) {
        const origin = { x, y };
        if (!canPlaceBuildingOnGrid(grid, existing, origin, width, height)) {
          continue;
        }

        if (enforceMinDistance) {
          let tooClose = false;
          for (const building of existing) {
            if (centerDistance(origin, width, height, building) < minDistance) {
              tooClose = true;
              break;
            }
          }
          if (tooClose) {
            continue;
          }
        }

        candidates.push(origin);
      }
    }
    return candidates;
  }

  const spacedCandidates = collectCandidates(true);
  if (spacedCandidates.length) {
    return selectDispersedCandidate(spacedCandidates, existing, width, height, buildingType, generator);
  }

  const relaxedCandidates = collectCandidates(false);
  if (relaxedCandidates.length) {
    return selectDispersedCandidate(relaxedCandidates, existing, width, height, buildingType, generator);
  }

  return fallback;
}

function buildPublicResourcePlacementRequests(seed) {
  const generator = createSeededGenerator(seed >>> 0);
  const requests = [];

  function addPlacements(kind, count) {
    const template = publicStructureTemplates[kind];
    for (let index = 0; index < count; index += 1) {
      const rotationQuarterTurns = generator.nextRangeInt(0, 3);
      const flipMask = generator.nextRangeInt(0, 3);
      requests.push({
        kind,
        type: template.type,
        width: template.width,
        height: template.height,
        rotationQuarterTurns,
        flipMask,
        footprintWidth: getFootprintWidthFor(template.width, template.height, rotationQuarterTurns),
        footprintHeight: getFootprintHeightFor(template.width, template.height, rotationQuarterTurns),
        selectionSeed: generator.nextUint32()
      });
    }
  }

  addPlacements("mine", GAME_CONFIG.map.numMines);
  addPlacements("farm", GAME_CONFIG.map.numFarms);
  return requests;
}

function buildPlacementVariantFrame(terrainWorldSeed, placementRequests, order) {
  const generatedTerrain = generateTerrainGrid({ terrainWorldSeed });
  const grid = generatedTerrain.grid;
  const existingBuildings = [createCenteredChurchBuilding(0)];

  for (const requestIndex of order) {
    const request = placementRequests[requestIndex];
    const generator = createSeededGenerator(request.selectionSeed);
    const origin = findValidBuildingPos(
      grid,
      existingBuildings,
      request.footprintWidth,
      request.footprintHeight,
      GAME_CONFIG.map.minPublicBuildingDistance,
      generatedTerrain.metadata.spawnLeftMax,
      generatedTerrain.metadata.spawnRightMin,
      request.type,
      generator
    );
    existingBuildings.push(createPublicBuilding({
      id: existingBuildings.length,
      type: request.type,
      ox: origin.x,
      oy: origin.y,
      w: request.width,
      h: request.height,
      rot: request.rotationQuarterTurns,
      fm: request.flipMask
    }));
  }

  return {
    grid,
    publicBuildings: existingBuildings
  };
}

function buildPlacementBuildUpFrames(terrainWorldSeed, placementRequests, order) {
  const generatedTerrain = generateTerrainGrid({ terrainWorldSeed });
  const grid = generatedTerrain.grid;
  const existingBuildings = [createCenteredChurchBuilding(0)];
  const frames = [];

  for (const requestIndex of order) {
    const request = placementRequests[requestIndex];
    const generator = createSeededGenerator(request.selectionSeed);
    const origin = findValidBuildingPos(
      grid,
      existingBuildings,
      request.footprintWidth,
      request.footprintHeight,
      GAME_CONFIG.map.minPublicBuildingDistance,
      generatedTerrain.metadata.spawnLeftMax,
      generatedTerrain.metadata.spawnRightMin,
      request.type,
      generator
    );

    existingBuildings.push(createPublicBuilding({
      id: existingBuildings.length,
      type: request.type,
      ox: origin.x,
      oy: origin.y,
      w: request.width,
      h: request.height,
      rot: request.rotationQuarterTurns,
      fm: request.flipMask
    }));

    frames.push({
      grid,
      publicBuildings: deepClone(existingBuildings)
    });
  }

  return frames;
}

function euclideanDistance(left, right) {
  const dx = left.x - right.x;
  const dy = left.y - right.y;
  return Math.sqrt((dx * dx) + (dy * dy));
}

function cellOccupiedByBuildings(buildings, x, y) {
  return buildings.some(function (building) {
    return buildingOccupiesCell(building, x, y);
  });
}

function buildChestSpawnCandidates(grid, publicBuildings, whiteKing, blackKing) {
  const center = getIllustrationBoardCenter(grid);
  const candidates = [];

  for (let y = 0; y < grid.length; y += 1) {
    for (let x = 0; x < grid[y].length; x += 1) {
      const cell = grid[y][x];
      if (!cell.c || cell.t === TERRAIN_WATER) {
        continue;
      }
      if (cellOccupiedByBuildings(publicBuildings, x, y)) {
        continue;
      }
      if ((x === whiteKing.x && y === whiteKing.y) || (x === blackKing.x && y === blackKing.y)) {
        continue;
      }

      const current = { x, y };
      const whiteDistance = euclideanDistance(current, whiteKing);
      const blackDistance = euclideanDistance(current, blackKing);
      if (Math.min(whiteDistance, blackDistance) < 6) {
        continue;
      }

      const radialDistance = euclideanDistance(current, center);
      const centrality = Math.max(0, Math.round((BOARD_RADIUS - radialDistance) * 1.6));
      const contestation = Math.max(0, Math.round((BOARD_RADIUS * 0.7) - Math.abs(whiteDistance - blackDistance)));
      const weight = Math.max(1, 1 + centrality + contestation);
      candidates.push({ x, y, weight });
    }
  }

  return candidates;
}

function sampleWeightedUniqueCells(candidates, sampleCount, seed) {
  const generator = createSeededGenerator(seed >>> 0);
  const pool = candidates.slice();
  const selections = [];

  while (pool.length && selections.length < sampleCount) {
    const totalWeight = pool.reduce(function (sum, candidate) {
      return sum + candidate.weight;
    }, 0);
    let threshold = generator.nextFloat() * Math.max(totalWeight, EPSILON);
    let pickedIndex = 0;

    for (let index = 0; index < pool.length; index += 1) {
      threshold -= pool[index].weight;
      if (threshold <= 0) {
        pickedIndex = index;
        break;
      }
    }

    const [picked] = pool.splice(pickedIndex, 1);
    selections.push({ x: picked.x, y: picked.y });
  }

  return selections;
}

function sampleWeightedIndex(weights, generator) {
  const safeWeights = Array.isArray(weights)
    ? weights.map(function (weight) {
      return Math.max(0, weight);
    })
    : [1];
  const totalWeight = safeWeights.reduce(function (sum, weight) {
    return sum + weight;
  }, 0);

  if (totalWeight <= EPSILON) {
    return 0;
  }

  let threshold = generator.nextFloat() * totalWeight;
  for (let index = 0; index < safeWeights.length; index += 1) {
    threshold -= safeWeights[index];
    if (threshold <= 0) {
      return index;
    }
  }

  return Math.max(0, safeWeights.length - 1);
}

function sampleTruncatedNormalAmount(generator, { mean, sigma, clampMultiplier = 2, minimum = 1 }) {
  const safeSigma = Math.max(0.01, sigma);
  const delta = safeSigma * clampMultiplier;
  const raw = mean + (safeSigma * sampleStandardNormal(generator));
  const clipped = clamp(raw, mean - delta, mean + delta);
  return Math.max(minimum, Math.round(clipped));
}

function sampleChestRewardPickup(generator, lateGame) {
  const rewardIndex = sampleWeightedIndex(
    lateGame ? CHEST_LATE_REWARD_WEIGHTS : CHEST_EARLY_REWARD_WEIGHTS,
    generator
  );

  switch (rewardIndex) {
    case CHEST_REWARD_MOVEMENT_MAX_BONUS:
      return { rewardTypeKey: "movement_points_max_bonus", amount: 1 };
    case CHEST_REWARD_BUILD_MAX_BONUS:
      return { rewardTypeKey: "build_points_max_bonus", amount: 1 };
    default:
      return {
        rewardTypeKey: "gold",
        amount: sampleTruncatedNormalAmount(generator, {
          mean: 35,
          sigma: 35 * 0.18,
          clampMultiplier: 2,
          minimum: 1
        })
      };
  }
}

function hashUnitFloat(seed, x, y, salt = 0) {
  const ux = (x + 0x4000) >>> 0;
  const uy = (y + 0x4000) >>> 0;
  let mixed = mixSeed(seed >>> 0, Math.imul(ux, 73856093) >>> 0);
  mixed = mixSeed(mixed, Math.imul(uy, 19349663) >>> 0);
  mixed = mixSeed(mixed, (salt + 1) >>> 0);
  return (mixed & 0xffff) / 65535;
}

function weatherValueNoise(seed, x, y, cellSpan) {
  const safeSpan = Math.max(1, cellSpan);
  const scaledX = x / safeSpan;
  const scaledY = y / safeSpan;
  const x0 = Math.floor(scaledX);
  const y0 = Math.floor(scaledY);
  const x1 = x0 + 1;
  const y1 = y0 + 1;
  const tx = smoothstep01(scaledX - x0);
  const ty = smoothstep01(scaledY - y0);

  const top = lerp(hashUnitFloat(seed, x0, y0), hashUnitFloat(seed, x1, y0), tx);
  const bottom = lerp(hashUnitFloat(seed, x0, y1), hashUnitFloat(seed, x1, y1), tx);
  return lerp(top, bottom, ty);
}

function sampleStandardNormal(generator) {
  const first = Math.max(generator.nextFloat(), EPSILON);
  const second = Math.max(generator.nextFloat(), EPSILON);
  return Math.sqrt(-2 * Math.log(first)) * Math.cos(2 * K_PI * second);
}

function sampleLogNormalCell(seed, cellX, cellY, overrides = {}) {
  const mu = (overrides.densityMuTimes100 ?? GAME_CONFIG.weather.densityMuTimes100) / 100;
  const sigma = Math.max(0.01, (overrides.densitySigmaTimes100 ?? GAME_CONFIG.weather.densitySigmaTimes100) / 100);
  let combinedSeed = mixSeed(seed >>> 0, Math.imul((cellX + 0x5000) | 0, 2246822519) >>> 0);
  combinedSeed = mixSeed(combinedSeed, Math.imul((cellY + 0x5000) | 0, 3266489917) >>> 0);
  const generator = createSeededGenerator(combinedSeed);
  return Math.exp(mu + (sigma * sampleStandardNormal(generator)));
}

function fromFixed(value) {
  return value / 1000;
}

function toFixed(value) {
  return Math.round(value * 1000);
}

function directionStep(directionId) {
  return WEATHER_DIRECTION_STEPS[directionId] || WEATHER_DIRECTION_STEPS[WEATHER_DIRECTION_EAST];
}

function normalizeDirectionVector(step) {
  const length = Math.sqrt((step.x * step.x) + (step.y * step.y));
  if (length <= 0) {
    return { x: 1, y: 0 };
  }
  return {
    x: step.x / length,
    y: step.y / length
  };
}

function boundaryCenterForEdge(edge, edgePosition, diameter) {
  const maxCoord = diameter - 0.5;
  switch (edge) {
    case ENTRY_EDGE_TOP:
      return { x: edgePosition, y: -0.5 };
    case ENTRY_EDGE_BOTTOM:
      return { x: edgePosition, y: maxCoord };
    case ENTRY_EDGE_LEFT:
      return { x: -0.5, y: edgePosition };
    case ENTRY_EDGE_RIGHT:
      return { x: maxCoord, y: edgePosition };
    default:
      return { x: edgePosition, y: -0.5 };
  }
}

function exitDistanceForRay(start, direction, diameter) {
  const minBound = -0.5;
  const maxBound = diameter - 0.5;
  let distance = Number.POSITIVE_INFINITY;

  if (direction.x > 0) {
    distance = Math.min(distance, (maxBound - start.x) / direction.x);
  } else if (direction.x < 0) {
    distance = Math.min(distance, (minBound - start.x) / direction.x);
  }

  if (direction.y > 0) {
    distance = Math.min(distance, (maxBound - start.y) / direction.y);
  } else if (direction.y < 0) {
    distance = Math.min(distance, (minBound - start.y) / direction.y);
  }

  if (!Number.isFinite(distance) || distance <= 0) {
    return diameter;
  }

  return distance;
}

function weatherDistancePerStep() {
  return Math.max(0.001, (GAME_CONFIG.weather.speedBlocksPer100Turns / 100) / 2);
}

function frontCenterX(front) {
  return fromFixed(front.centerStartXTimes1000) + (fromFixed(front.stepXTimes1000) * front.currentTurnStep);
}

function frontCenterY(front) {
  return fromFixed(front.centerStartYTimes1000) + (fromFixed(front.stepYTimes1000) * front.currentTurnStep);
}

function concealmentAlpha(front, grid, cellX, cellY, overrides = {}) {
  if (!isInBounds(grid, cellX, cellY)) {
    return 0;
  }

  const cell = grid[cellY][cellX];
  if (!cell.c) {
    return 0;
  }

  const forward = normalizeDirectionVector(directionStep(front.direction));
  const sideways = { x: -forward.y, y: forward.x };
  const center = { x: frontCenterX(front), y: frontCenterY(front) };
  const samplePoint = { x: cellX, y: cellY };
  const relative = {
    x: samplePoint.x - center.x,
    y: samplePoint.y - center.y
  };
  const along = (relative.x * forward.x) + (relative.y * forward.y);
  const across = (relative.x * sideways.x) + (relative.y * sideways.y);
  const radiusAlong = Math.max(0.1, fromFixed(front.radiusAlongTimes1000));
  const radiusAcross = Math.max(0.1, fromFixed(front.radiusAcrossTimes1000));
  const normalizedDistance = Math.sqrt(
    ((along * along) / Math.max(radiusAlong * radiusAlong, EPSILON))
    + ((across * across) / Math.max(radiusAcross * radiusAcross, EPSILON))
  );

  const shapeNoiseCellSpan = overrides.shapeNoiseCellSpan ?? GAME_CONFIG.weather.shapeNoiseCellSpan;
  const boundaryNoise = weatherValueNoise(front.shapeSeed, cellX, cellY, shapeNoiseCellSpan);
  const noiseAmplitude = (overrides.shapeNoiseAmplitudePercent ?? GAME_CONFIG.weather.shapeNoiseAmplitudePercent) / 100;
  const effectiveBoundary = 1 + ((boundaryNoise - 0.5) * noiseAmplitude);
  const edgeDistance = effectiveBoundary - normalizedDistance;
  if (edgeDistance <= 0) {
    return 0;
  }

  const edgeSoftness = Math.max(0.05, (overrides.edgeSoftnessPercent ?? GAME_CONFIG.weather.edgeSoftnessPercent) / 100);
  const edgeFade = clamp(edgeDistance / edgeSoftness, 0, 1);
  if (edgeFade <= 0) {
    return 0;
  }

  const alphaBase = (overrides.alphaBasePercent ?? GAME_CONFIG.weather.alphaBasePercent) / 100;
  const alphaMin = (overrides.alphaMinPercent ?? GAME_CONFIG.weather.alphaMinPercent) / 100;
  const alphaMax = (overrides.alphaMaxPercent ?? GAME_CONFIG.weather.alphaMaxPercent) / 100;
  const densityMultiplier = sampleLogNormalCell(front.densitySeed, cellX, cellY, overrides);
  const localAlpha = clamp(alphaBase * densityMultiplier, alphaMin, alphaMax);
  return clamp(localAlpha * edgeFade, 0, 1);
}

function encodedAlpha(alpha) {
  return clampInt(Math.round(alpha * 255), 0, 255);
}

function concealmentShade(alpha, seed, cellX, cellY) {
  const toneNoise = hashUnitFloat(seed, cellX, cellY, 97);
  const baseShade = 210 + ((toneNoise - 0.5) * 24);
  const densityDarkening = alpha * 0.16;
  return clampInt(Math.round(baseShade - densityDarkening), 160, 225);
}

function makeProbeFront(directionId, center, radiusAlong, radiusAcross, shapeSeed, densitySeed) {
  return {
    direction: directionId,
    currentTurnStep: 0,
    totalTurnSteps: 1,
    centerStartXTimes1000: toFixed(center.x),
    centerStartYTimes1000: toFixed(center.y),
    stepXTimes1000: 0,
    stepYTimes1000: 0,
    radiusAlongTimes1000: toFixed(radiusAlong),
    radiusAcrossTimes1000: toFixed(radiusAcross),
    shapeSeed,
    densitySeed
  };
}

function frontHasAnyRenderedCells(grid, validCells, front, overrides = {}) {
  for (const cell of validCells) {
    if (encodedAlpha(concealmentAlpha(front, grid, cell.x, cell.y, overrides)) > 0) {
      return true;
    }
  }
  return false;
}

function maxSearchDistance(grid, radiusAlong, radiusAcross, startDistance) {
  const maxRadius = Math.max(radiusAlong, radiusAcross);
  return startDistance + grid.length + ((maxRadius + 4) * 6);
}

function findNearestHiddenDistance(grid, validCells, boundaryCenter, normalizedDirection, directionId, radiusAlong, radiusAcross, shapeSeed, densitySeed, startDistance, directionSign, overrides = {}) {
  function hasRenderedCellsAtDistance(distanceAlongRay) {
    const center = {
      x: boundaryCenter.x + (normalizedDirection.x * distanceAlongRay * directionSign),
      y: boundaryCenter.y + (normalizedDirection.y * distanceAlongRay * directionSign)
    };
    const probeFront = makeProbeFront(directionId, center, radiusAlong, radiusAcross, shapeSeed, densitySeed);
    return frontHasAnyRenderedCells(grid, validCells, probeFront, overrides);
  }

  let low = Math.max(0, startDistance);
  if (!hasRenderedCellsAtDistance(low)) {
    return low;
  }

  let step = Math.max(0.5, weatherDistancePerStep());
  let high = low + step;
  const searchLimit = maxSearchDistance(grid, radiusAlong, radiusAcross, startDistance);

  while (high < searchLimit && hasRenderedCellsAtDistance(high)) {
    low = high;
    step *= 2;
    high = Math.min(searchLimit, low + step);
  }

  if (hasRenderedCellsAtDistance(high)) {
    return high;
  }

  for (let iteration = 0; iteration < 18; iteration += 1) {
    const mid = (low + high) * 0.5;
    if (hasRenderedCellsAtDistance(mid)) {
      low = mid;
    } else {
      high = mid;
    }
  }

  return high;
}

function buildSpawnedFrontDescriptor({
  grid,
  directionId,
  entryEdge,
  edgePosition,
  coveragePercent,
  aspectRatio,
  shapeSeed,
  densitySeed,
  currentTurnStep = 0,
  weatherOverrides = {}
}) {
  const validCells = getAllValidCells(grid);
  const boundaryCenter = boundaryCenterForEdge(entryEdge, edgePosition, grid.length);
  const normalizedDirection = normalizeDirectionVector(directionStep(directionId));
  const targetArea = Math.max(1, (validCells.length * coveragePercent) / 100);
  const safeAspectRatio = Math.max(1, aspectRatio);
  const radiusAlong = Math.sqrt(targetArea / (K_PI * safeAspectRatio));
  const radiusAcross = Math.sqrt((targetArea * safeAspectRatio) / K_PI);
  const entryHiddenDistance = findNearestHiddenDistance(
    grid,
    validCells,
    boundaryCenter,
    normalizedDirection,
    directionId,
    radiusAlong,
    radiusAcross,
    shapeSeed,
    densitySeed,
    0,
    -1,
    weatherOverrides
  );
  const boardCrossDistance = exitDistanceForRay(boundaryCenter, normalizedDirection, grid.length);
  const exitHiddenDistance = findNearestHiddenDistance(
    grid,
    validCells,
    boundaryCenter,
    normalizedDirection,
    directionId,
    radiusAlong,
    radiusAcross,
    shapeSeed,
    densitySeed,
    boardCrossDistance,
    1,
    weatherOverrides
  );
  const totalTravelDistance = entryHiddenDistance + exitHiddenDistance;
  const distancePerStep = weatherDistancePerStep();
  const totalTurnSteps = Math.max(2, Math.ceil(totalTravelDistance / distancePerStep));
  const clampedTurnStep = clampInt(currentTurnStep, 0, Math.max(totalTurnSteps - 1, 0));

  return {
    direction: directionId,
    currentTurnStep: clampedTurnStep,
    totalTurnSteps,
    centerStartXTimes1000: toFixed(boundaryCenter.x - (normalizedDirection.x * entryHiddenDistance)),
    centerStartYTimes1000: toFixed(boundaryCenter.y - (normalizedDirection.y * entryHiddenDistance)),
    stepXTimes1000: toFixed(normalizedDirection.x * distancePerStep),
    stepYTimes1000: toFixed(normalizedDirection.y * distancePerStep),
    radiusAlongTimes1000: toFixed(radiusAlong),
    radiusAcrossTimes1000: toFixed(radiusAcross),
    shapeSeed,
    densitySeed
  };
}

function createWeatherMaskFromFronts(grid, activeFronts, weatherOverrides = {}) {
  const diameter = grid.length;
  const alphaByCell = Array.from({ length: diameter * diameter }, function () {
    return 0;
  });
  const shadeByCell = Array.from({ length: diameter * diameter }, function () {
    return 0;
  });

  for (let y = 0; y < diameter; y += 1) {
    for (let x = 0; x < diameter; x += 1) {
      const cell = grid[y][x];
      if (!cell.c) {
        continue;
      }

      let bestAlpha = 0;
      let bestShade = 0;
      for (const front of activeFronts) {
        const alpha = encodedAlpha(concealmentAlpha(front, grid, x, y, weatherOverrides));
        if (alpha <= bestAlpha) {
          continue;
        }

        bestAlpha = alpha;
        bestShade = concealmentShade(alpha, front.shapeSeed, x, y);
      }

      const index = toIndex(x, y, diameter);
      alphaByCell[index] = bestAlpha;
      shadeByCell[index] = bestShade;
    }
  }

  return {
    diameter,
    alphaByCell,
    shadeByCell
  };
}

function createEmptyWeatherMask() {
  return {
    diameter: BOARD_HEIGHT,
    alphaByCell: Array.from({ length: BOARD_WIDTH * BOARD_HEIGHT }, function () {
      return 0;
    }),
    shadeByCell: Array.from({ length: BOARD_WIDTH * BOARD_HEIGHT }, function () {
      return 0;
    })
  };
}

function createWeatherState(grid, activeFronts, weatherOverrides = {}) {
  const fronts = Array.isArray(activeFronts) ? activeFronts : [];
  return {
    nextSpawnTurnStep: 0,
    hasActiveFront: fronts.length ? 1 : 0,
    rngCounter: 0,
    revision: 1,
    activeFront: fronts.length ? deepClone(fronts[0]) : null,
    activeFronts: deepClone(fronts),
    mask: fronts.length ? createWeatherMaskFromFronts(grid, fronts, weatherOverrides) : createEmptyWeatherMask()
  };
}

function createSnapshot({
  activeKingdom = 0,
  grid,
  whitePieces = EMPTY_PIECES,
  blackPieces = EMPTY_PIECES,
  publicBuildings = EMPTY_BUILDINGS,
  mapObjects = [],
  autonomousUnits = EMPTY_AUTONOMOUS_UNITS,
  weatherState = null
} = {}) {
  const snapshot = {
    activeKingdom,
    grid: cloneGrid(grid),
    whiteKingdom: {
      pieces: deepClone(whitePieces),
      buildings: []
    },
    blackKingdom: {
      pieces: deepClone(blackPieces),
      buildings: []
    },
    publicBuildings: deepClone(publicBuildings),
    mapObjects: deepClone(mapObjects),
    autonomousUnits: deepClone(autonomousUnits)
  };

  if (weatherState) {
    snapshot.weatherState = deepClone(weatherState);
  }

  return snapshot;
}

function createReplayData({ saveName, frames }) {
  const snapshots = frames.map(function (frame) {
    return createSnapshot(frame);
  });

  return {
    schemaVersion: 5,
    saveName,
    historyContinuityComplete: true,
    createdAtUnix: 0,
    referenceData: deepClone(illustrationReferenceData),
    initialSnapshot: snapshots[0],
    turnHistory: snapshots.slice(1).map(function (snapshot, index) {
      const frame = frames[index + 1] || {};
      return {
        committedTurnNumber: Number.isFinite(frame.committedTurnNumber)
          ? Math.max(1, Math.trunc(frame.committedTurnNumber))
          : index + 1,
        committedActiveKingdom: Number.isFinite(frame.committedActiveKingdom)
          ? Math.max(0, Math.trunc(frame.committedActiveKingdom))
          : (Number.isFinite(snapshot.activeKingdom) ? Math.max(0, Math.trunc(snapshot.activeKingdom)) : (index % 2)),
        capturedAtUnix: index + 1,
        snapshot,
        newEvents: Array.isArray(frame.newEvents) ? deepClone(frame.newEvents) : [],
        notifications: Array.isArray(frame.notifications) ? deepClone(frame.notifications) : []
      };
    })
  };
}

function getIllustrationBoardCenter(grid) {
  const center = (grid.length - 1) / 2;
  return { x: center, y: center };
}

function recenterFrontDescriptor(descriptor, targetCenter) {
  const currentCenterX = frontCenterX(descriptor);
  const currentCenterY = frontCenterY(descriptor);

  return {
    ...descriptor,
    centerStartXTimes1000: toFixed(fromFixed(descriptor.centerStartXTimes1000) + (targetCenter.x - currentCenterX)),
    centerStartYTimes1000: toFixed(fromFixed(descriptor.centerStartYTimes1000) + (targetCenter.y - currentCenterY))
  };
}

function createWeatherFrame(grid, options) {
  const descriptor = buildSpawnedFrontDescriptor({
    grid,
    directionId: options.directionId,
    entryEdge: options.entryEdge,
    edgePosition: options.edgePosition,
    coveragePercent: options.coveragePercent,
    aspectRatio: options.aspectRatio,
    shapeSeed: options.shapeSeed,
    densitySeed: options.densitySeed,
    currentTurnStep: options.currentTurnStep,
    weatherOverrides: options.weatherOverrides
  });
  const normalizedDescriptor = options.centerAt
    ? recenterFrontDescriptor(descriptor, options.centerAt)
    : descriptor;

  return {
    grid,
    weatherState: createWeatherState(grid, [normalizedDescriptor], options.weatherOverrides),
    whitePieces: options.whitePieces || EMPTY_PIECES,
    blackPieces: options.blackPieces || EMPTY_PIECES,
    publicBuildings: options.publicBuildings || EMPTY_BUILDINGS,
    autonomousUnits: options.autonomousUnits || EMPTY_AUTONOMOUS_UNITS
  };
}

function buildIllustrationConfig(replayData, overrides = {}) {
  return {
    replayData,
    autoplayIntervalMs: DEFAULT_AUTOPLAY_INTERVAL_MS,
    autoplayOnMount: true,
    loopPlayback: true,
    ...overrides
  };
}

function swapOrder(order, leftIndex, rightIndex) {
  const swapped = order.slice();
  const temp = swapped[leftIndex];
  swapped[leftIndex] = swapped[rightIndex];
  swapped[rightIndex] = temp;
  return swapped;
}

function createLandPosition(grid, target) {
  return findNearestLandCell(grid, target);
}

const CONTEXT_WORLD_SEED = 0x2c1f3d5b;
const PLACEMENT_REQUEST_SEED = 0x5f1c92ad;
const WEATHER_WORLD_SEED = 0x71a53d29;
const BASE_VISUAL_SEED = 0x19d4ae67;

const contextualTerrain = generateTerrainGrid({
  terrainWorldSeed: CONTEXT_WORLD_SEED,
  terrainVisualSeed: BASE_VISUAL_SEED
});
const contextualTerrainGrid = contextualTerrain.grid;

const rotationMineAnchor = createLandPosition(contextualTerrainGrid, { x: BOARD_RADIUS - 7, y: BOARD_RADIUS + 4 });
const rotationFarmAnchor = createLandPosition(contextualTerrainGrid, { x: BOARD_RADIUS + 7, y: BOARD_RADIUS - 3 });

const rotationReplayData = createReplayData({
  saveName: "illustration-rotation-public-buildings",
  frames: [0, 1, 2, 3].map(function (rotation) {
    return {
      grid: contextualTerrainGrid,
      publicBuildings: [
        createAnchoredPublicBuilding({ id: 100, kind: "mine", anchorCell: rotationMineAnchor, rotation }),
        createAnchoredPublicBuilding({ id: 101, kind: "farm", anchorCell: rotationFarmAnchor, rotation: (rotation + 1) % 4 })
      ]
    };
  })
});

const flipReplayData = createReplayData({
  saveName: "illustration-flip-public-buildings",
  frames: [0, 1, 2, 3].map(function (flipMask) {
    return {
      grid: contextualTerrainGrid,
      publicBuildings: [
        createAnchoredPublicBuilding({ id: 100, kind: "mine", anchorCell: rotationMineAnchor, rotation: 1, flipMask }),
        createAnchoredPublicBuilding({ id: 101, kind: "farm", anchorCell: rotationFarmAnchor, rotation: 2, flipMask: (flipMask + 1) % 4 })
      ]
    };
  })
});

const placementRequests = buildPublicResourcePlacementRequests(PLACEMENT_REQUEST_SEED);
const canonicalPlacementOrder = shuffleArray(
  placementRequests.map(function (_, index) {
    return index;
  }),
  createSeededGenerator(PLACEMENT_REQUEST_SEED ^ 0x9e3779b9)
);

const placementVariantOrders = [
  canonicalPlacementOrder,
  swapOrder(canonicalPlacementOrder, 1, 2),
  swapOrder(canonicalPlacementOrder, 2, 3),
  swapOrder(canonicalPlacementOrder, 0, 4)
];

const placementVariantFrames = placementVariantOrders.map(function (order) {
  return buildPlacementVariantFrame(CONTEXT_WORLD_SEED, placementRequests, order);
});

const staticContextBuildings = placementVariantFrames[0].publicBuildings;

const whiteSpawnPositions = [
  createLandPosition(contextualTerrainGrid, { x: 7, y: BOARD_RADIUS }),
  createLandPosition(contextualTerrainGrid, { x: 9, y: BOARD_RADIUS - 4 }),
  createLandPosition(contextualTerrainGrid, { x: 10, y: BOARD_RADIUS + 4 }),
  createLandPosition(contextualTerrainGrid, { x: 8, y: BOARD_RADIUS + 1 })
];

const blackSpawnPositions = [
  createLandPosition(contextualTerrainGrid, { x: BOARD_WIDTH - 8, y: BOARD_RADIUS }),
  createLandPosition(contextualTerrainGrid, { x: BOARD_WIDTH - 10, y: BOARD_RADIUS - 4 }),
  createLandPosition(contextualTerrainGrid, { x: BOARD_WIDTH - 9, y: BOARD_RADIUS + 4 }),
  createLandPosition(contextualTerrainGrid, { x: BOARD_WIDTH - 7, y: BOARD_RADIUS + 1 })
];

const combinedSpawnReplayData = createReplayData({
  saveName: "illustration-kingdom-spawn-zones",
  frames: whiteSpawnPositions.map(function (whitePosition, index) {
    const blackPosition = blackSpawnPositions[index];
    return {
      grid: contextualTerrainGrid,
      whitePieces: [createKing({ id: 0, kingdom: 0, x: whitePosition.x, y: whitePosition.y })],
      blackPieces: [createKing({ id: 1, kingdom: 1, x: blackPosition.x, y: blackPosition.y })],
      publicBuildings: staticContextBuildings
    };
  })
});

const placementOrderReplayData = createReplayData({
  saveName: "illustration-public-building-order",
  frames: placementVariantFrames
});

const placementBuildUpReplayData = createReplayData({
  saveName: "illustration-public-building-position",
  frames: buildPlacementBuildUpFrames(CONTEXT_WORLD_SEED, placementRequests, canonicalPlacementOrder)
});

const chestIllustrationWhiteKing = createKing({
  id: 601,
  kingdom: 0,
  x: whiteSpawnPositions[0].x,
  y: whiteSpawnPositions[0].y
});

const chestIllustrationBlackKing = createKing({
  id: 602,
  kingdom: 1,
  x: blackSpawnPositions[0].x,
  y: blackSpawnPositions[0].y
});

const chestSpawnIllustrationFrames = sampleWeightedUniqueCells(
  buildChestSpawnCandidates(
    contextualTerrainGrid,
    staticContextBuildings,
    chestIllustrationWhiteKing,
    chestIllustrationBlackKing
  ),
  10,
  0x3a5f91c7
).map(function (position, index) {
  return {
    grid: contextualTerrainGrid,
    whitePieces: [chestIllustrationWhiteKing],
    blackPieces: [chestIllustrationBlackKing],
    publicBuildings: staticContextBuildings,
    mapObjects: [createChest({ id: 700 + index, x: position.x, y: position.y })]
  };
});

const chestSpawnCellReplayData = createReplayData({
  saveName: "illustration-chest-spawn-cell",
  frames: chestSpawnIllustrationFrames
});

const chestRewardIllustrationGrid = createGameLikeBoardGrid();
const chestRewardChestPosition = createLandPosition(chestRewardIllustrationGrid, {
  x: BOARD_RADIUS,
  y: BOARD_RADIUS
});
const chestRewardPawnStartPosition = createLandPosition(chestRewardIllustrationGrid, {
  x: chestRewardChestPosition.x - 3,
  y: chestRewardChestPosition.y
});
const chestRewardWhiteKingPosition = createLandPosition(chestRewardIllustrationGrid, {
  x: BOARD_RADIUS - 7,
  y: BOARD_RADIUS + 7
});

const chestRewardIllustrationKing = createKing({
  id: 811,
  kingdom: 0,
  x: chestRewardWhiteKingPosition.x,
  y: chestRewardWhiteKingPosition.y
});

const chestRewardIllustrationGenerator = createSeededGenerator(0x4b9d31c7);
const chestRewardIllustrationPickups = Array.from({ length: 10 }, function (_, index) {
  const lateGame = index >= 5;
  return {
    phaseLabel: lateGame ? "Fin de partie" : "Début de partie",
    ...sampleChestRewardPickup(chestRewardIllustrationGenerator, lateGame)
  };
});

const chestRewardTypeIllustrationFrames = chestRewardIllustrationPickups.flatMap(function (pickup, index) {
  const pawnId = 812 + index;
  const chestId = 860 + index;
  const pathPositions = [0, 1, 2].map(function (offset) {
    return findNearestLandCell(chestRewardIllustrationGrid, {
      x: chestRewardPawnStartPosition.x + offset,
      y: chestRewardPawnStartPosition.y
    });
  }).concat([{ x: chestRewardChestPosition.x, y: chestRewardChestPosition.y }]);

  return pathPositions.map(function (position, stepIndex) {
    const pickupFrame = stepIndex === pathPositions.length - 1;

    return {
      grid: chestRewardIllustrationGrid,
      whitePieces: [
        chestRewardIllustrationKing,
        createPiece({
          id: pawnId,
          type: PIECE_PAWN,
          kingdom: 0,
          x: position.x,
          y: position.y
        })
      ],
      mapObjects: pickupFrame
        ? []
        : [createChest({ id: chestId, x: chestRewardChestPosition.x, y: chestRewardChestPosition.y })],
      ...(pickupFrame
        ? {
            committedTurnNumber: index + 1,
            committedActiveKingdom: 0,
            notifications: [
              createChestRewardNotification({
                kingdom: 0,
                rewardTypeKey: pickup.rewardTypeKey,
                amount: pickup.amount
              })
            ]
          }
        : {})
    };
  });
});

const chestRewardTypeReplayData = createReplayData({
  saveName: "illustration-chest-reward-type",
  frames: chestRewardTypeIllustrationFrames
});

const chestRewardTypeStatusValues = chestRewardIllustrationPickups.flatMap(function (pickup) {
  return [pickup.phaseLabel, pickup.phaseLabel, pickup.phaseLabel, pickup.phaseLabel];
});

function buildTargetedSpawnScenario({
  grid,
  targetPosition,
  generator,
  sessionId
}) {
  const spawnOption = chooseInfernalRookSpawnOption(grid, targetPosition, generator);
  return spawnOption
    ? buildInfernalRookHuntFrames(grid, targetPosition, spawnOption, sessionId)
    : [];
}

const targetedSpawnTerrainGrid = generateTerrainGrid({
  terrainWorldSeed: CONTEXT_WORLD_SEED,
  terrainVisualSeed: BASE_VISUAL_SEED
}).grid;

const targetedSpawnTargetPositions = sampleIllustrationTargetPositions(
  targetedSpawnTerrainGrid,
  4,
  0x42f18e6d
);

const targetedSpawnGenerator = createSeededGenerator(0x781f2c93);

const targetedSpawnOptionFrames = targetedSpawnTargetPositions.flatMap(function (targetPosition, targetIndex) {
  return Array.from({ length: 5 }, function (_, sessionOffset) {
    return buildTargetedSpawnScenario({
      grid: targetedSpawnTerrainGrid,
      targetPosition,
      generator: targetedSpawnGenerator,
      sessionId: (targetIndex * 5) + sessionOffset
    });
  }).flat();
});

const targetedSpawnOptionReplayData = createReplayData({
  saveName: "illustration-infernal-targeted-spawn-option",
  frames: targetedSpawnOptionFrames
});

const weatherTerrain = generateTerrainGrid({
  terrainWorldSeed: WEATHER_WORLD_SEED,
  terrainVisualSeed: WEATHER_WORLD_SEED
});
const weatherBaseGrid = weatherTerrain.grid;
const weatherIllustrationCenter = getIllustrationBoardCenter(weatherBaseGrid);
const weatherMidEdge = BOARD_RADIUS;
const weatherQuarterEdge = Math.floor(BOARD_WIDTH * 0.32);
const weatherUpperEdge = Math.floor(BOARD_WIDTH * 0.68);
const weatherContextBuildings = [createCenteredChurchBuilding(0)];
const weatherWhiteKingPosition = createLandPosition(weatherBaseGrid, { x: 6, y: BOARD_RADIUS + 6 });
const weatherBlackKingPosition = createLandPosition(weatherBaseGrid, { x: BOARD_WIDTH - 7, y: BOARD_RADIUS - 6 });
const weatherWhiteKing = createKing({
  id: 301,
  kingdom: 0,
  x: weatherWhiteKingPosition.x,
  y: weatherWhiteKingPosition.y
});
const weatherBlackKing = createKing({
  id: 302,
  kingdom: 1,
  x: weatherBlackKingPosition.x,
  y: weatherBlackKingPosition.y
});

const frontDirectionReplayData = createReplayData({
  saveName: "illustration-weather-front-direction",
  frames: [
    {
      directionId: WEATHER_DIRECTION_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherMidEdge,
      coveragePercent: 14,
      aspectRatio: 2.2,
      shapeSeed: 11,
      densitySeed: 17,
      currentTurnStep: 8
    },
    {
      directionId: WEATHER_DIRECTION_NORTH_EAST,
      entryEdge: ENTRY_EDGE_BOTTOM,
      edgePosition: weatherMidEdge,
      coveragePercent: 14,
      aspectRatio: 2.2,
      shapeSeed: 11,
      densitySeed: 17,
      currentTurnStep: 8
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherMidEdge,
      coveragePercent: 14,
      aspectRatio: 2.2,
      shapeSeed: 11,
      densitySeed: 17,
      currentTurnStep: 8
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_WEST,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherMidEdge,
      coveragePercent: 14,
      aspectRatio: 2.2,
      shapeSeed: 11,
      densitySeed: 17,
      currentTurnStep: 8
    }
  ].map(function (options) {
    return createWeatherFrame(weatherBaseGrid, options);
  })
});

const diagonalEntryReplayData = createReplayData({
  saveName: "illustration-weather-front-diagonal-entry",
  frames: [
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 2,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 8,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 14,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 2,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 8,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    },
    {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherUpperEdge,
      coveragePercent: 19,
      aspectRatio: 2.35,
      shapeSeed: 41,
      densitySeed: 17,
      currentTurnStep: 14,
      whitePieces: [weatherWhiteKing],
      blackPieces: [weatherBlackKing],
      publicBuildings: weatherContextBuildings
    }
  ].map(function (options) {
    return createWeatherFrame(weatherBaseGrid, options);
  })
});

const searchingCloudFrame = createWeatherFrame(weatherBaseGrid, {
  directionId: WEATHER_DIRECTION_EAST,
  entryEdge: ENTRY_EDGE_LEFT,
  edgePosition: weatherMidEdge,
  coveragePercent: 18,
  aspectRatio: 1.9,
  shapeSeed: 83,
  densitySeed: 61,
  currentTurnStep: 9,
  centerAt: weatherIllustrationCenter,
  publicBuildings: weatherContextBuildings,
  weatherOverrides: {
    alphaBasePercent: 52,
    alphaMinPercent: 24,
    alphaMaxPercent: 86
  }
});

const searchingBlackKingPosition = createLandPosition(weatherBaseGrid, { x: BOARD_RADIUS + 1, y: BOARD_RADIUS - 1 });
const searchingBlackPawnOnePosition = createLandPosition(weatherBaseGrid, { x: BOARD_RADIUS - 1, y: BOARD_RADIUS + 1 });
const searchingBlackPawnTwoPosition = createLandPosition(weatherBaseGrid, { x: BOARD_RADIUS + 3, y: BOARD_RADIUS + 2 });
const searchingWhiteKingPosition = createLandPosition(weatherBaseGrid, { x: 5, y: BOARD_RADIUS + 7 });

const searchingBlackPieces = [
  createPiece({ id: 401, type: PIECE_KING, kingdom: 1, x: searchingBlackKingPosition.x, y: searchingBlackKingPosition.y }),
  createPiece({ id: 402, type: PIECE_PAWN, kingdom: 1, x: searchingBlackPawnOnePosition.x, y: searchingBlackPawnOnePosition.y }),
  createPiece({ id: 403, type: PIECE_PAWN, kingdom: 1, x: searchingBlackPawnTwoPosition.x, y: searchingBlackPawnTwoPosition.y })
];

const searchingWhitePieces = [
  createPiece({ id: 404, type: PIECE_KING, kingdom: 0, x: searchingWhiteKingPosition.x, y: searchingWhiteKingPosition.y })
];

const searchingInfernalWaypoints = [
  createLandPosition(weatherBaseGrid, { x: 4, y: BOARD_RADIUS - 5 }),
  createLandPosition(weatherBaseGrid, { x: 4, y: BOARD_RADIUS - 1 }),
  createLandPosition(weatherBaseGrid, { x: 4, y: BOARD_RADIUS + 3 }),
  createLandPosition(weatherBaseGrid, { x: 7, y: BOARD_RADIUS + 3 }),
  createLandPosition(weatherBaseGrid, { x: 7, y: BOARD_RADIUS - 1 }),
  createLandPosition(weatherBaseGrid, { x: 5, y: BOARD_RADIUS - 1 })
];

const infernalSearchingReplayData = createReplayData({
  saveName: "illustration-infernal-searching-random-move",
  frames: searchingInfernalWaypoints.map(function (position) {
    return {
      grid: weatherBaseGrid,
      weatherState: searchingCloudFrame.weatherState,
      whitePieces: searchingWhitePieces,
      blackPieces: searchingBlackPieces,
      publicBuildings: weatherContextBuildings,
      autonomousUnits: [
        createInfernalUnit({
          id: 501,
          pieceType: PIECE_ROOK,
          x: position.x,
          y: position.y,
          targetKingdom: 1,
          phase: INFERNAL_PHASE_SEARCHING
        })
      ]
    };
  })
});

const frontAspectRatioReplayData = createReplayData({
  saveName: "illustration-weather-front-aspect-ratio",
  frames: [1.8, 2.2, 2.6].map(function (aspectRatio) {
    return createWeatherFrame(weatherBaseGrid, {
      directionId: WEATHER_DIRECTION_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherMidEdge,
      coveragePercent: 22,
      aspectRatio,
      shapeSeed: 51,
      densitySeed: 19,
      currentTurnStep: 8,
      centerAt: weatherIllustrationCenter
    });
  })
});

const frontShapeReplayData = createReplayData({
  saveName: "illustration-weather-front-shape-seed",
  frames: [7, 29, 71, 113].map(function (shapeSeed) {
    return createWeatherFrame(weatherBaseGrid, {
      directionId: WEATHER_DIRECTION_SOUTH_EAST,
      entryEdge: ENTRY_EDGE_TOP,
      edgePosition: weatherMidEdge,
      coveragePercent: 22,
      aspectRatio: 2.25,
      shapeSeed,
      densitySeed: 37,
      currentTurnStep: 9,
      centerAt: weatherIllustrationCenter
    });
  })
});

const frontDensityReplayData = createReplayData({
  saveName: "illustration-weather-front-density-seed",
  frames: [5, 41, 83, 127].map(function (densitySeed) {
    return createWeatherFrame(weatherBaseGrid, {
      directionId: WEATHER_DIRECTION_EAST,
      entryEdge: ENTRY_EDGE_LEFT,
      edgePosition: weatherMidEdge,
      coveragePercent: 22,
      aspectRatio: 2.25,
      shapeSeed: 59,
      densitySeed,
      currentTurnStep: 9,
      centerAt: weatherIllustrationCenter
    });
  })
});

const frontContourComparisonSeeds = [
  19,
  41,
  67,
  89,
  113,
  137,
  163,
  191,
  223,
  251
];

const frontContourReplayData = createReplayData({
  saveName: "illustration-weather-front-contour-noise",
  frames: frontContourComparisonSeeds.flatMap(function (shapeSeed) {
    return [
      createWeatherFrame(weatherBaseGrid, {
        directionId: WEATHER_DIRECTION_NORTH_EAST,
        entryEdge: ENTRY_EDGE_BOTTOM,
        edgePosition: weatherMidEdge,
        coveragePercent: 20,
        aspectRatio: 1.95,
        shapeSeed,
        densitySeed: 53,
        currentTurnStep: 10,
        weatherOverrides: { shapeNoiseAmplitudePercent: 0 },
        centerAt: weatherIllustrationCenter
      }),
      createWeatherFrame(weatherBaseGrid, {
        directionId: WEATHER_DIRECTION_NORTH_EAST,
        entryEdge: ENTRY_EDGE_BOTTOM,
        edgePosition: weatherMidEdge,
        coveragePercent: 20,
        aspectRatio: 1.95,
        shapeSeed,
        densitySeed: 53,
        currentTurnStep: 10,
        weatherOverrides: {},
        centerAt: weatherIllustrationCenter
      })
    ];
  })
});

const frontContourStatusValues = frontContourComparisonSeeds.flatMap(function () {
  return ["Sans bruit", "Avec bruit"];
});

const dirtFieldReplayData = createReplayData({
  saveName: "illustration-dirt-field",
  frames: [31, 71, 111, 151].map(function (dirtSeed) {
    return {
      grid: generateTerrainGrid({
        terrainWorldSeed: WEATHER_WORLD_SEED,
        dirtNoiseSeed: dirtSeed,
        waterNoiseSeed: weatherTerrain.metadata.waterNoiseSeed,
        terrainVisualSeed: BASE_VISUAL_SEED
      }).grid
    };
  })
});

const waterFieldReplayData = createReplayData({
  saveName: "illustration-water-field",
  frames: [17, 53, 89, 125].map(function (waterSeed) {
    return {
      grid: generateTerrainGrid({
        terrainWorldSeed: WEATHER_WORLD_SEED,
        dirtNoiseSeed: weatherTerrain.metadata.dirtNoiseSeed,
        waterNoiseSeed: waterSeed,
        terrainVisualSeed: BASE_VISUAL_SEED
      }).grid
    };
  })
});

const grassBaseTerrain = generateTerrainGrid({
  terrainWorldSeed: 0x31415926,
  terrainVisualSeed: BASE_VISUAL_SEED
});

const textureShowcaseTerrainGrid = (function () {
  const grid = generateTerrainGrid({
    terrainWorldSeed: 0x57c3a18d,
    terrainVisualSeed: BASE_VISUAL_SEED,
    dirtCoveragePercent: 22,
    waterCoveragePercent: 8,
    numDirtBlobs: 9,
    numLakes: 5,
    preserveSpawnCorridor: false
  }).grid;
  const showcasePatches = [
    { type: TERRAIN_DIRT, x: BOARD_RADIUS - 2, y: BOARD_RADIUS + 2, radius: 4 },
    { type: TERRAIN_WATER, x: BOARD_RADIUS + 5, y: BOARD_RADIUS - 3, radius: 3 },
    { type: TERRAIN_DIRT, x: BOARD_RADIUS - 6, y: BOARD_RADIUS - 4, radius: 2 }
  ];

  showcasePatches.forEach(function (patch) {
    for (let y = Math.max(0, patch.y - patch.radius); y <= Math.min(grid.length - 1, patch.y + patch.radius); y += 1) {
      for (let x = Math.max(0, patch.x - patch.radius); x <= Math.min(grid[y].length - 1, patch.x + patch.radius); x += 1) {
        const dx = x - patch.x;
        const dy = y - patch.y;
        if ((dx * dx) + (dy * dy) > patch.radius * patch.radius) {
          continue;
        }
        if (!grid[y][x].c) {
          continue;
        }
        grid[y][x].t = patch.type;
      }
    }
  });

  return grid;
})();

const grassBrightnessReplayData = createReplayData({
  saveName: "illustration-grass-brightness-beta",
  frames: [11, 37, 73, 109].map(function (brightnessSeed) {
    return {
      grid: createGridWithBrightnessSeed(grassBaseTerrain.grid, brightnessSeed)
    };
  })
});

const terrainFlipReplayData = createReplayData({
  saveName: "illustration-terrain-flip-mask",
  frames: [3, 17, 43, 79].map(function (flipSeed) {
    return {
      grid: createGridWithFlipSeed(textureShowcaseTerrainGrid, flipSeed)
    };
  })
});

export const processIllustrationsByKey = {
  "chest-spawn-cell": buildIllustrationConfig(chestSpawnCellReplayData, {
    autoplayIntervalMs: 900,
    description:
      "Sur la même carte générée, chaque tick montre une case d'apparition possible pour un coffre. Les positions restent sur des cellules admissibles, loin des deux rois et plutôt dans des zones centrales ou contestées."
  }),
  "chest-reward-type": buildIllustrationConfig(chestRewardTypeReplayData, {
    autoplayIntervalMs: 720,
    toastCooldownMs: 1450,
    showToasts: true,
    showStatusOverlay: true,
    statusOverlay: {
      label: "Régime",
      values: chestRewardTypeStatusValues,
      showShield: false
    },
    initialZoom: 2.05,
    description:
      "Chaque paire de ticks montre le coffre central avant puis après ramassage. Les cinq premières ouvertures restent en début de partie avec les poids 8/3/3, les cinq dernières passent en fin de partie avec les poids 4/6/6, et les toasts reprennent le format du replay."
  }),
  "public-building-rotation": buildIllustrationConfig(rotationReplayData),
  "public-building-flip": buildIllustrationConfig(flipReplayData),
  "public-building-position": buildIllustrationConfig(placementBuildUpReplayData, {
    autoplayIntervalMs: 950,
    description:
      "À chaque tick, un bâtiment public supplémentaire est ajouté sur la même carte. Le placement privilégie les positions admissibles les mieux dispersées par rapport à l'église centrale et aux bâtiments déjà posés."
  }),
  "kingdom-spawn-zones": buildIllustrationConfig(combinedSpawnReplayData),
  "weather-front-diagonal-entry": buildIllustrationConfig(diagonalEntryReplayData, {
    autoplayIntervalMs: 900,
    description:
      "Ticks 1 à 3 : la diagonale entre par le haut. Ticks 4 à 6 : la même diagonale entre par la gauche. On voit ainsi plus nettement que la direction reste la même, mais que le bord d'entrée change."
  }),
  "weather-front-aspect-ratio": buildIllustrationConfig(frontAspectRatioReplayData),
  "weather-front-density-seed": buildIllustrationConfig(frontDensityReplayData),
  "public-building-order": buildIllustrationConfig(placementOrderReplayData),
  "weather-front-direction": buildIllustrationConfig(frontDirectionReplayData),
  "infernal-targeted-spawn-option": buildIllustrationConfig(targetedSpawnOptionReplayData, {
    autoplayIntervalMs: 760,
    initialZoom: 1.35,
    description:
      "Le terrain reste fixe pendant toute l'illustration. Une dame noire reste cinq fois au même endroit pendant que la dame infernale apparaît sur un bord choisi avec les vrais poids de proximité de chemin, puis la poursuit jusqu'à capture. Après cinq captures, la dame noire change de position, et l'expérience recommence quatre fois au total."
  }),
  "infernal-searching-random-move": buildIllustrationConfig(infernalSearchingReplayData, {
    autoplayIntervalMs: 1000,
    initialZoom: 1.25,
    description:
      "Ici, la pièce du diable vise le royaume noir. Les pièces noires restent cachées sous un brouillard fixe : elle ne retrouve donc aucune cible visible et erre aléatoirement autour du nuage jusqu'à revoir une pièce noire."
  }),
  "grass-brightness-beta": buildIllustrationConfig(grassBrightnessReplayData, { initialZoom: 1.7 }),
  "dirt-field": buildIllustrationConfig(dirtFieldReplayData),
  "water-field": buildIllustrationConfig(waterFieldReplayData),
  "terrain-flip-mask": buildIllustrationConfig(terrainFlipReplayData, { initialZoom: 1.7 }),
  "weather-front-contour-noise": buildIllustrationConfig(frontContourReplayData, {
    autoplayIntervalMs: 780,
    showStatusOverlay: true,
    statusOverlay: {
      label: "Contour",
      values: frontContourStatusValues,
      showShield: false
    },
    description:
      "L'illustration alterne sur 20 ticks une version sans bruit puis une version bruitée. Chaque tick bruité régénère un contour différent, tandis que le tick juste avant montre la même base sans perturbation."
  })
};
