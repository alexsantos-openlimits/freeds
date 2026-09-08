// Lusol — small DOM / formatting helpers shared across views.

export function el(html) {
  const tpl = document.createElement("template");
  tpl.innerHTML = html.trim();
  return tpl.content.firstElementChild;
}

export function qs(root, sel) {
  return (root || document).querySelector(sel);
}

export function qsa(root, sel) {
  return Array.from((root || document).querySelectorAll(sel));
}

export function debounce(fn, wait = 300) {
  let t;
  return (...args) => {
    clearTimeout(t);
    t = setTimeout(() => fn(...args), wait);
  };
}

export function fmtNumber(n, decimals = 0) {
  if (n === undefined || n === null || Number.isNaN(n)) return "—";
  return Number(n).toLocaleString(undefined, {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

export function fmtWatts(n) {
  if (n === undefined || n === null || Number.isNaN(n)) return "—";
  return fmtNumber(Math.round(n));
}

export function fmtKwh(n, decimals = 2) {
  if (n === undefined || n === null || Number.isNaN(n)) return "—";
  return fmtNumber(n, decimals);
}

export function fmtUptime(seconds) {
  if (!seconds || seconds < 0) return "—";
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const parts = [];
  if (d) parts.push(`${d}d`);
  if (h || d) parts.push(`${h}h`);
  parts.push(`${m}m`);
  return parts.join(" ");
}

// Converts an HHMM/HMM integer (e.g. 500 -> "05:00", 1730 -> "17:30")
// into a value suitable for <input type="time">.
export function hhmmIntToTimeInput(value) {
  const v = Number(value) || 0;
  const hh = Math.floor(v / 100);
  const mm = v % 100;
  return `${String(hh).padStart(2, "0")}:${String(mm).padStart(2, "0")}`;
}

export function timeInputToHhmmInt(value) {
  if (!value) return 0;
  const [hh, mm] = value.split(":").map(Number);
  return (hh || 0) * 100 + (mm || 0);
}

// Rótulo do balão/mosaico de "fonte de dados": usa o nome do gestor de
// excedentes ativo (ex.: "DDSU666", "Solax V2", "MQTT") em vez de um texto
// genérico fixo, para continuar correto sempre que o utilizador troca de
// fonte. Cai no texto antigo (sem nome) só enquanto ainda não há nenhum
// gestor instanciado (arranque).
export function sourceStatusLabel(status, i18n) {
  const ok = status.sourceConnected && !status.dataFault;
  const name = (status.sourceName || "").replace(/^Modbus\s+/, "");
  if (!name) return ok ? i18n.t("dashboard.source_connected") : i18n.t("dashboard.source_fault");
  const state = ok ? i18n.t("dashboard.state_connected") : i18n.t("dashboard.state_disconnected");
  return `${name} · ${state}`;
}

export function escapeHtml(str) {
  return String(str ?? "").replace(/[&<>"']/g, (c) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}
