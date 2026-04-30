const DOCUMENT_HIDDEN_REASON = "document-hidden";
const VIEWPORT_REASON = "viewport";
const VIEWPORT_MARGIN_PX = 0;

const viewerRegistry = new Map();

let viewportObserver = null;
let documentVisibilityListenerActive = false;

function hasViewerSuspensionApi(viewerInstance) {
  return Boolean(viewerInstance && typeof viewerInstance.setSuspended === "function");
}

function computeViewportVisibility(element) {
  if (!(element instanceof HTMLElement)) {
    return true;
  }

  const rect = element.getBoundingClientRect();
  if (rect.width <= 0 || rect.height <= 0) {
    return false;
  }

  const viewportWidth = window.innerWidth || document.documentElement.clientWidth || 0;
  const viewportHeight = window.innerHeight || document.documentElement.clientHeight || 0;

  return (
    rect.bottom >= -VIEWPORT_MARGIN_PX
    && rect.right >= 0
    && rect.top <= viewportHeight + VIEWPORT_MARGIN_PX
    && rect.left <= viewportWidth
  );
}

function applySuspension(record) {
  if (!record || !hasViewerSuspensionApi(record.viewerInstance)) {
    return;
  }

  record.viewerInstance.setSuspended(DOCUMENT_HIDDEN_REASON, record.documentHidden);
  record.viewerInstance.setSuspended(VIEWPORT_REASON, !record.inViewport);
}

function updateViewportState(element, inViewport) {
  const record = viewerRegistry.get(element);
  if (!record || record.inViewport === inViewport) {
    return;
  }

  record.inViewport = inViewport;
  applySuspension(record);
}

function handleViewportEntries(entries) {
  for (const entry of entries) {
    updateViewportState(
      entry.target,
      entry.isIntersecting && entry.intersectionRatio > 0
    );
  }
}

function ensureViewportObserver() {
  if (viewportObserver || typeof IntersectionObserver === "undefined") {
    return viewportObserver;
  }

  viewportObserver = new IntersectionObserver(handleViewportEntries, {
    root: null,
    rootMargin: `${VIEWPORT_MARGIN_PX}px 0px ${VIEWPORT_MARGIN_PX}px 0px`,
    threshold: [0, 0.01, 0.1]
  });

  return viewportObserver;
}

function handleDocumentVisibilityChange() {
  const documentHidden = Boolean(document.hidden);
  for (const record of viewerRegistry.values()) {
    if (record.documentHidden === documentHidden) {
      continue;
    }

    record.documentHidden = documentHidden;
    applySuspension(record);
  }
}

function ensureDocumentVisibilityListener() {
  if (documentVisibilityListenerActive || typeof document === "undefined") {
    return;
  }

  document.addEventListener("visibilitychange", handleDocumentVisibilityChange);
  documentVisibilityListenerActive = true;
}

function teardownSharedObserversIfIdle() {
  if (viewerRegistry.size > 0) {
    return;
  }

  if (viewportObserver) {
    viewportObserver.disconnect();
    viewportObserver = null;
  }

  if (documentVisibilityListenerActive && typeof document !== "undefined") {
    document.removeEventListener("visibilitychange", handleDocumentVisibilityChange);
    documentVisibilityListenerActive = false;
  }
}

export function registerViewerVisibility(rootElement, viewerInstance) {
  if (!(rootElement instanceof HTMLElement) || !hasViewerSuspensionApi(viewerInstance)) {
    return;
  }

  const record = {
    rootElement,
    viewerInstance,
    inViewport: computeViewportVisibility(rootElement),
    documentHidden: typeof document !== "undefined" ? Boolean(document.hidden) : false
  };

  viewerRegistry.set(rootElement, record);

  ensureViewportObserver()?.observe(rootElement);
  ensureDocumentVisibilityListener();
  applySuspension(record);
}

export function unregisterViewerVisibility(rootElement) {
  if (!(rootElement instanceof HTMLElement)) {
    return;
  }

  viewerRegistry.delete(rootElement);
  viewportObserver?.unobserve(rootElement);
  teardownSharedObserversIfIdle();
}