// Lusol — minimal circular gauge, drawn with plain SVG (no canvas libs).

export function gaugeSvg({ percent = 0, size = 84, stroke = 9, color = "var(--color-primary)", label = "" }) {
  const p = Math.max(0, Math.min(100, Number(percent) || 0));
  const r = (size - stroke) / 2;
  const c = size / 2;
  const circumference = 2 * Math.PI * r;
  const dash = (p / 100) * circumference;
  return `
    <svg class="gauge-wrap" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">
      <circle class="gauge-track" cx="${c}" cy="${c}" r="${r}" stroke-width="${stroke}"></circle>
      <circle class="gauge-value" cx="${c}" cy="${c}" r="${r}" stroke-width="${stroke}"
        stroke="${color}"
        stroke-dasharray="${dash.toFixed(1)} ${circumference.toFixed(1)}"
        transform="rotate(-90 ${c} ${c})"></circle>
      <text x="${c}" y="${c + 1}" text-anchor="middle" dominant-baseline="middle" class="gauge-center-label">${label}</text>
    </svg>`;
}
