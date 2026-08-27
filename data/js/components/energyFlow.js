// Lusol — mosaico "casa" com o fluxo de energia em tempo real: painel solar,
// controlador e termoacumulador dentro de uma casa, ligação à rede (a
// importar ou a exportar) e a potência que está a ser desviada para a
// resistência. Tudo desenhado em SVG simples (mesmo estilo dos ícones da
// app), sem imagens externas, e recalculado a cada atualização de estado.

import { fmtWatts, fmtNumber } from "./dom.js";

export function energyFlowSvg({ solarWatts, gridWatts, loadWatts, thermoC, hasTemp, i18n }) {
  const importing = (gridWatts || 0) > 0;
  const exporting = (gridWatts || 0) < 0;
  const gridAbs = Math.abs(gridWatts || 0);
  const heating = (loadWatts || 0) > 1;
  const tempLabel = hasTemp && thermoC !== undefined && thermoC !== null && thermoC > -100
    ? `${fmtNumber(thermoC, 1)}°C`
    : "—";

  const gridLabel = importing
    ? `${i18n.t("dashboard.grid_import")} ${fmtWatts(gridAbs)} W`
    : exporting
      ? `${i18n.t("dashboard.grid_export")} ${fmtWatts(gridAbs)} W`
      : `${i18n.t("dashboard.grid_power")} 0 W`;

  return `
    <svg class="energy-flow-svg" viewBox="0 0 480 240" role="img" aria-label="${i18n.t("dashboard.energy_flow")}">
      <!-- sol -->
      <g class="ef-sun">
        <circle cx="222" cy="30" r="13"/>
        <path d="M222 8v-8M222 60v8M200 30h-8M244 30h8M206 14l-6-6M238 46l6 6M238 14l6-6M206 46l-6 6"/>
      </g>

      <!-- painel solar (encosta esquerda do telhado) -->
      <g class="ef-panel">
        <polygon points="103,92 188,54 202,76 116,113"/>
        <line x1="122" y1="103" x2="181" y2="65"/>
        <line x1="112" y1="98" x2="192" y2="60"/>
        <line x1="132" y1="108" x2="192" y2="80"/>
      </g>

      <!-- casa -->
      <polygon class="ef-roof" points="78,112 222,42 366,112"/>
      <rect class="ef-wall" x="96" y="112" width="252" height="100" rx="2"/>

      <!-- fio painel -> controlador -->
      <path class="ef-wire ef-wire-solar" d="M150,100 V132 Q150,140 158,140 H176"/>
      <text class="ef-flow-label" x="150" y="128" text-anchor="middle">${fmtWatts(solarWatts)} W</text>

      <!-- controlador -->
      <g class="ef-controller">
        <rect x="176" y="140" width="58" height="48" rx="6"/>
        <circle cx="205" cy="158" r="9"/>
        <path d="M200 158h10M205 153v10"/>
        <text x="205" y="180" text-anchor="middle" class="ef-controller-label">LUSOL</text>
      </g>

      <!-- fio controlador -> termoacumulador -->
      <path class="ef-wire ef-wire-heat ${heating ? "ef-active" : ""}" d="M234,164 H262"/>
      <text class="ef-flow-label ef-heat-label ${heating ? "ef-active" : ""}" x="248" y="156" text-anchor="middle">${fmtWatts(loadWatts)} W</text>

      <!-- termoacumulador -->
      <g class="ef-tank">
        <rect x="262" y="120" width="66" height="88" rx="12"/>
        <ellipse cx="295" cy="120" rx="33" ry="9"/>
        <circle cx="295" cy="140" r="10" class="ef-gauge-ring"/>
        <path d="M295 134v6l3.5 3.5" class="ef-gauge-needle"/>
        <path class="ef-droplet" d="M295 162c7 8 7 12.5 0 18.5-7-5.5-7-10.5 0-18.5z"/>
        <path class="ef-wave ${heating ? "ef-active" : ""}" d="M281 190q7-4.5 14 0t14 0" />
        <text x="295" y="202" text-anchor="middle" class="ef-temp-label">${tempLabel}</text>
      </g>

      <!-- poste + ligação à rede -->
      <g class="ef-pole">
        <line x1="34" y1="70" x2="34" y2="212"/>
        <line x1="20" y1="82" x2="48" y2="82"/>
        <line x1="24" y1="82" x2="24" y2="70"/>
        <line x1="44" y1="82" x2="44" y2="70"/>
      </g>
      <path class="ef-wire ef-grid-wire ${importing ? "ef-import" : exporting ? "ef-export" : ""}" d="M34,150 H96"/>
      <path class="ef-grid-arrow ${importing ? "ef-import" : exporting ? "ef-export" : ""}"
        d="${importing ? "M74,144 84,150 74,156" : "M56,144 46,150 56,156"}" />
      <text class="ef-flow-label ef-grid-label ${importing ? "ef-import" : exporting ? "ef-export" : ""}" x="65" y="132" text-anchor="middle">${gridLabel}</text>
    </svg>`;
}
