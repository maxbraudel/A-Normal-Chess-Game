export function normalizeReportAnchorFragment(value) {
  return (value || "")
    .normalize("NFD")
    .replace(/\p{Diacritic}/gu, "")
    .trim()
    .toLowerCase()
    .replace(/["'’]/g, "")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .replace(/-{2,}/g, "-");
}

export function buildReportProcessAnchor(sectionId, processTitle) {
  const normalizedSectionId = normalizeReportAnchorFragment(sectionId);
  const normalizedProcessTitle = normalizeReportAnchorFragment(processTitle);

  return [normalizedSectionId, normalizedProcessTitle].filter(Boolean).join("-");
}