// FreeDS — tiny inline-SVG icon set (no external icon font/library).
// Each function returns a ready-to-insert <svg> markup string.

function svg(inner, extra = "") {
  return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" ${extra}>${inner}</svg>`;
}

export const Icon = {
  sun: () => svg(`<circle cx="12" cy="12" r="4"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/>`),
  zap: () => svg(`<path d="M12 2 4 14h6l-1 8 9-12h-6l1-8z"/>`),
  battery: () => svg(`<rect x="2" y="7" width="17" height="10" rx="2.2"/><path d="M21 10.5v3"/><path d="M6 10.5v3"/>`),
  thermometer: () => svg(`<path d="M12 14.5V4a2 2 0 1 0-4 0v10.5a4 4 0 1 0 4 0z"/><path d="M11 8h2"/>`),
  wifi: () => svg(`<path d="M3 8.5a13 13 0 0 1 18 0"/><path d="M6.2 12a9 9 0 0 1 11.6 0"/><path d="M9.3 15.4a5 5 0 0 1 5.4 0"/><circle cx="12" cy="19" r="1" fill="currentColor" stroke="none"/>`),
  home: () => svg(`<path d="M3.5 11 12 4l8.5 7"/><path d="M5.5 9.7V20h13V9.7"/>`),
  sliders: () => svg(`<path d="M4 6h11M18 6h2M4 12h6M13 12h7M4 18h13M20 18h0"/><circle cx="17" cy="6" r="2"/><circle cx="10" cy="12" r="2"/><circle cx="16" cy="18" r="2"/>`),
  terminal: () => svg(`<rect x="3" y="4" width="18" height="16" rx="2.2"/><path d="M7 9.5 10.5 12 7 14.5M12.5 15h4.5"/>`),
  tool: () => svg(`<path d="M14.5 6.5 18 3l3 3-3.5 3.5M14.5 6.5 4 17v3h3L17.5 9.5M14.5 6.5l3 3"/>`),
  router: () => svg(`<rect x="3" y="12" width="18" height="7" rx="1.6"/><path d="M7 12V9a5 5 0 0 1 10 0v3"/><circle cx="8" cy="15.5" r="0.7" fill="currentColor" stroke="none"/><circle cx="12" cy="15.5" r="0.7" fill="currentColor" stroke="none"/>`),
  message: () => svg(`<path d="M4 5h16v10H9l-4 4v-4H4z"/>`),
  cpu: () => svg(`<rect x="7" y="7" width="10" height="10" rx="1.6"/><path d="M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3"/>`),
  globe: () => svg(`<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.7 2.6 4 5.7 4 9s-1.3 6.4-4 9c-2.7-2.6-4-5.7-4-9s1.3-6.4 4-9z"/>`),
  chevronRight: () => svg(`<path d="M9 6l6 6-6 6"/>`),
  chevronDown: () => svg(`<path d="M6 9l6 6 6-6"/>`),
  menu: () => svg(`<path d="M4 7h16M4 12h16M4 17h16"/>`),
  close: () => svg(`<path d="M6 6l12 12M18 6 6 18"/>`),
  check: () => svg(`<path d="M5 13l4 4L19 7"/>`),
  alertTriangle: () => svg(`<path d="M12 3.5 2 20h20L12 3.5z"/><path d="M12 10v4"/><circle cx="12" cy="17" r="0.8" fill="currentColor" stroke="none"/>`),
  download: () => svg(`<path d="M12 4v11m0 0-4-4m4 4 4-4"/><path d="M5 19h14"/>`),
  upload: () => svg(`<path d="M12 20V9m0 0-4 4m4-4 4 4"/><path d="M5 5h14"/>`),
  refresh: () => svg(`<path d="M3.5 12a8.5 8.5 0 0 1 14.7-5.9M20.5 12a8.5 8.5 0 0 1-14.7 5.9"/><path d="M18 3.5v3.6h-3.6M6 20.5v-3.6h3.6"/>`),
  power: () => svg(`<path d="M12 4v7"/><path d="M6.5 6.5a7.5 7.5 0 1 0 11 0"/>`),
  rotateCcw: () => svg(`<path d="M3 12a9 9 0 1 0 2.6-6.4"/><path d="M3 4v5h5"/>`),
  plug: () => svg(`<path d="M9 3v6M15 3v6"/><path d="M6 9h12v3a6 6 0 0 1-12 0z"/><path d="M12 18v3"/>`),
  gauge: () => svg(`<path d="M4 15a8 8 0 1 1 16 0"/><path d="M12 15l4-4"/><circle cx="12" cy="15" r="1.2" fill="currentColor" stroke="none"/>`),
  droplet: () => svg(`<path d="M12 3s6 6.5 6 11a6 6 0 1 1-12 0c0-4.5 6-11 6-11z"/>`),
  lock: () => svg(`<rect x="5" y="10.5" width="14" height="9" rx="1.8"/><path d="M8 10.5V7a4 4 0 1 1 8 0v3.5"/>`),
  clock: () => svg(`<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3.5 2"/>`),
  arrowsUpDown: () => svg(`<path d="M8 4v14m0 0-3-3m3 3 3-3"/><path d="M16 20V6m0 0-3 3m3-3 3 3"/>`),
};
