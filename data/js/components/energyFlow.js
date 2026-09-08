// Lusol — mosaico "casa" com o fluxo de energia em tempo real: painel solar,
// controlador e termoacumulador dentro de uma casa, ligação à rede (a
// importar ou a exportar) e a potência que está a ser desviada para a
// resistência. Tudo desenhado em SVG simples (mesmo estilo dos ícones da
// app), sem imagens externas, e recalculado a cada atualização de estado.
//
// Convenção de sinal (igual à do firmware, ver EnergyTracker.cpp): gridWatts
// POSITIVO = excedente injetado na rede; NEGATIVO = consumo vindo da rede.
//
// O desenho reserva "corredores" livres para os números (entre o painel e o
// depósito, entre o controlador e o depósito, e à esquerda da casa), para que
// nenhum valor fique por cima de um objeto. Como rede de segurança, os
// próprios rótulos levam um contorno da cor do fundo (ver .ef-flow-label no
// styles.css), o que os mantém legíveis mesmo em ecrãs muito estreitos.

import { fmtWatts, fmtNumber } from "./dom.js";

export function energyFlowHtml({ solarWatts, gridWatts, loadWatts, thermoC, hasTemp, i18n }) {
  const grid = Number(gridWatts) || 0;
  const load = Number(loadWatts) || 0;
  const solar = Number(solarWatts) || 0;

  const exporting = grid > 1;
  const importing = grid < -1;
  const heating = load > 1;
  const gridState = exporting ? "ef-export" : importing ? "ef-import" : "";

  const tempLabel = hasTemp && thermoC !== undefined && thermoC !== null && thermoC > -100
    ? `${fmtNumber(thermoC, 1)}°C`
    : "—";

  // Seta sobre o cabo da rede: aponta para dentro de casa quando se importa,
  // para fora quando se exporta. Fica fora da parede, em espaço livre.
  const arrow = importing
    ? "M84,199 94,205 84,211"
    : exporting
      ? "M70,199 60,205 70,211"
      : "";

  const gridLabel = exporting
    ? i18n.t("dashboard.grid_export")
    : importing
      ? i18n.t("dashboard.grid_import")
      : i18n.t("dashboard.grid");

  return `
    <svg class="energy-flow-svg" viewBox="0 0 520 300" role="img"
         aria-label="${i18n.t("dashboard.energy_flow")}">
      <!-- sol, por cima do painel -->
      <g class="ef-sun">
        <circle cx="150" cy="40" r="14"/>
        <path d="M150 12v-9M150 68v9M122 40h-9M178 40h9M130 20l-6-6M170 60l6 6M170 20l6-6M130 60l-6 6"/>
      </g>

      <!-- casa -->
      <polygon class="ef-roof" points="85,155 250,72 415,155"/>
      <rect class="ef-wall" x="110" y="155" width="280" height="105"/>
      <line class="ef-ground" x1="26" y1="261" x2="494" y2="261"/>

      <!-- painel solar sobre a água esquerda do telhado -->
      <g class="ef-panel">
        <polygon points="118,138 204,95 212,111 126,154"/>
        <line x1="147" y1="124" x2="155" y2="140"/>
        <line x1="175" y1="110" x2="183" y2="126"/>
        <line x1="122" y1="146" x2="208" y2="103"/>
      </g>

      <!-- cabo painel -> controlador -->
      <path class="ef-wire ef-wire-solar" d="M172,131 V196"/>
      <text class="ef-flow-label ef-solar-label" x="243" y="152" text-anchor="middle">${fmtWatts(solar)} W</text>

      <!-- controlador -->
      <g class="ef-controller">
        <rect x="140" y="196" width="64" height="46" rx="7"/>
        <circle cx="172" cy="212" r="8"/>
        <path d="M167.5 212h9M172 207.5v9"/>
        <text x="172" y="234" text-anchor="middle" class="ef-controller-label">LUSOL</text>
      </g>

      <!-- cabo controlador -> resistência do depósito -->
      <path class="ef-wire ef-wire-heat ${heating ? "ef-active" : ""}" d="M204,219 H290"/>
      <text class="ef-flow-label ef-heat-label ${heating ? "ef-active" : ""}"
            x="247" y="210" text-anchor="middle">${fmtWatts(load)} W</text>

      <!-- termoacumulador -->
      <g class="ef-tank">
        <rect x="290" y="162" width="72" height="98" rx="13"/>
        <ellipse cx="326" cy="162" rx="36" ry="9"/>
        <circle class="ef-gauge-ring" cx="326" cy="186" r="11"/>
        <path class="ef-gauge-needle" d="M326 180v6l4 4"/>
        <path class="ef-droplet" d="M326 206c7.5 8.5 7.5 13.5 0 19.5-7.5-6-7.5-11 0-19.5z"/>
        <path class="ef-wave ${heating ? "ef-active" : ""}" d="M310 236q8-5 16 0t16 0"/>
        <text x="326" y="254" text-anchor="middle" class="ef-temp-label">${tempLabel}</text>
      </g>

      <!-- poste e ligação à rede -->
      <g class="ef-pole">
        <line x1="48" y1="104" x2="48" y2="261"/>
        <line x1="32" y1="118" x2="64" y2="118"/>
        <line x1="37" y1="118" x2="37" y2="105"/>
        <line x1="59" y1="118" x2="59" y2="105"/>
      </g>
      <path class="ef-wire ef-grid-wire ${gridState}" d="M48,205 H110"/>
      ${arrow ? `<path class="ef-grid-arrow ${gridState}" d="${arrow}"/>` : ""}
      <text class="ef-flow-label ef-grid-label ${gridState}" x="79" y="192" text-anchor="middle">${fmtWatts(Math.abs(grid))} W</text>
      <text class="ef-node-caption" x="62" y="228" text-anchor="middle">${gridLabel}</text>
    </svg>

    <div class="ef-legend">
      <span class="ef-legend-item"><i class="ef-dot ef-dot-solar"></i>${i18n.t("dashboard.solar_power")}</span>
      <span class="ef-legend-item"><i class="ef-dot ef-dot-heat"></i>${i18n.t("dashboard.legend_diverted")}</span>
      <span class="ef-legend-item"><i class="ef-dot ef-dot-grid"></i>${i18n.t("dashboard.grid_power")}</span>
    </div>`;
}
