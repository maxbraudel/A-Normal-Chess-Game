/**
 * Centralized store for replay and master config data.
 * Ensures all components fetch once and share the same data instances.
 */

let loadingPromises = {
  replay: null,
  masterConfig: null
};

const cachedData = {
  replay: null,
  masterConfig: null
};

const DEFAULT_MASTER_CONFIG = Object.freeze({
  dimensions: {
    boardWidth: 16,
    boardHeight: 16,
    cellSizePx: 48
  },
  display: {
    gridColor: "rgba(0, 0, 0, 0.1)",
    gridLineWidth: 1
  }
});

/**
 * Resolves a URL relative to the current page.
 */
function resolveUrl(url) {
  if (!url) return url;
  if (/^(https?:|\/\/)/.test(url)) return url;
  return new URL(url, window.location.href).toString();
}

/**
 * Load master config once and cache it.
 */
export async function loadMasterConfigData(url) {
  if (!url) {
    return DEFAULT_MASTER_CONFIG;
  }

  if (cachedData.masterConfig) {
    return cachedData.masterConfig;
  }

  if (loadingPromises.masterConfig) {
    return loadingPromises.masterConfig;
  }

  loadingPromises.masterConfig = (async () => {
    try {
      const response = await fetch(resolveUrl(url), {
        cache: "no-store"
      });
      if (!response.ok) {
        console.warn(`Master config HTTP ${response.status}, using default.`);
        return DEFAULT_MASTER_CONFIG;
      }
      const data = await response.json();
      cachedData.masterConfig = data;
      return data;
    } catch (error) {
      console.warn("Failed to load master config, using default.", error);
      return DEFAULT_MASTER_CONFIG;
    } finally {
      loadingPromises.masterConfig = null;
    }
  })();

  return loadingPromises.masterConfig;
}

/**
 * Load replay data once and cache it.
 */
export async function loadReplayData(url) {
  if (!url) {
    throw new Error("No replay URL provided.");
  }

  if (cachedData.replay) {
    return cachedData.replay;
  }

  if (loadingPromises.replay) {
    return loadingPromises.replay;
  }

  loadingPromises.replay = (async () => {
    try {
      const response = await fetch(resolveUrl(url), {
        cache: "no-store"
      });
      if (!response.ok) {
        throw new Error(`Impossible de charger le replay: HTTP ${response.status} ${response.statusText}`);
      }
      const data = await response.json();
      validateReplay(data);
      cachedData.replay = data;
      return data;
    } finally {
      loadingPromises.replay = null;
    }
  })();

  return loadingPromises.replay;
}

/**
 * Get cached replay data if available, without triggering a new load.
 */
export function getCachedReplayData() {
  return cachedData.replay;
}

/**
 * Get cached master config if available.
 */
export function getCachedMasterConfig() {
  return cachedData.masterConfig || DEFAULT_MASTER_CONFIG;
}

/**
 * Validate replay structure.
 */
function validateReplay(data) {
  if (!data || typeof data !== "object" || Array.isArray(data)) {
    throw new Error("Le companion charge n'est pas un objet JSON valide.");
  }

  const TARGET_SCHEMA_VERSION = 5;
  if (Number(data.schemaVersion) !== TARGET_SCHEMA_VERSION) {
    throw new Error("Ce viewer attend un companion Data schema v5.");
  }

  if (!data.initialSnapshot && !Array.isArray(data.turnHistory) && !data.currentStateSummary) {
    throw new Error("Le companion n'expose aucun snapshot exploitable.");
  }
}

/**
 * Clear cached data (for testing or cleanup).
 */
export function clearReplayDataCache() {
  cachedData.replay = null;
  cachedData.masterConfig = null;
  loadingPromises.replay = null;
  loadingPromises.masterConfig = null;
}
