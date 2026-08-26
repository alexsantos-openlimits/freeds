import { Icon } from "../components/icons.js";
import { gaugeSvg } from "../components/gauge.js";
import { fmtWatts, fmtKwh, fmtNumber, fmtUptime, sourceStatusLabel } from "../components/dom.js";
import { toast } from "../components/toast.js";

export default async function mount(container, ctx) {
  const { i18n } = ctx;
  ctx.setTitle("dashboard.title");

  let skeletonKey = null;

  function skeletonKeyFor(status) {
    const f = status.fields || {};
    return [
      JSON.stringify(f),
      status.temperatures && status.temperatures.enabled ? 1 : 0,
      (status.relays || []).length,
      status.battery ? 1 : 0,
    ].join("|");
  }

  function statCard({ id, accent, iconName, labelKey, extra = "" }) {
    return `
      <div class="stat-card accent-${accent}" id="${id}">
        <div class="stat-head">
          <span class="stat-label"><span class="icon-badge ${accent}">${Icon[iconName]()}</span> ${i18n.t(labelKey)}</span>
        </div>
        <div class="stat-value" data-role="value">—</div>
        ${extra}
      </div>`;
  }

  function buildSkeleton(status) {
    const f = status.fields || {};
    const hasBattery = !!status.battery && f.batteryWatts !== false;
    const showPv1 = f.pv1 !== false && status.pv1;
    const showPv2 = f.pv2 !== false && status.pv2;
    const showInvTemp = f.inverterTemperature !== false && status.inverterTemperature !== undefined;
    const showLoad = f.loadWatts !== false && status.loadWatts !== undefined;
    const tempsEnabled = status.temperatures && status.temperatures.enabled;

    return `
      <div class="view-header">
        <h2 data-i18n="dashboard.title"></h2>
      </div>

      <div class="card" id="quick-control-card">
        <div class="card-title">${Icon.gauge()} <span data-i18n="dashboard.quick_control"></span></div>
        <div class="flex-between" style="margin-bottom:14px;">
          <div>
            <div class="stat-label" style="margin-bottom:6px;" data-i18n="dashboard.working_mode"></div>
            <div class="mode-segmented" id="mode-segmented" role="group">
              <button type="button" data-mode="auto" data-i18n="common.auto"></button>
              <button type="button" data-mode="manual" data-i18n="common.manual"></button>
              <button type="button" data-mode="off" data-i18n="common.off"></button>
            </div>
          </div>
          <div id="pwm-gauge-holder"></div>
        </div>
        <div id="manual-pwm-row" class="hidden" style="max-width:420px;">
          <div class="stat-label" style="margin-bottom:6px;" data-i18n="dashboard.manual_pwm"></div>
          <div class="range-row">
            <input type="range" id="manual-pwm-range" min="0" max="100" step="1" value="0">
            <span class="range-out" id="manual-pwm-out">0%</span>
            <button type="button" class="btn btn-primary" id="manual-pwm-apply" data-i18n="dashboard.apply"></button>
          </div>
        </div>
      </div>

      <div class="grid metrics" style="margin-top:16px;">
        ${statCard({ id: "card-solar", accent: "solar", iconName: "sun", labelKey: "dashboard.solar_power",
          extra: `<div class="stat-sub" data-i18n="dashboard.solar_today"></div><div class="stat-sub"><strong data-role="today">—</strong></div>` })}
        ${statCard({ id: "card-grid", accent: "grid", iconName: "zap", labelKey: "dashboard.grid_power",
          extra: `<div class="stat-sub"><span data-i18n="dashboard.grid_import"></span>: <strong data-role="import">—</strong> · <span data-i18n="dashboard.grid_export"></span>: <strong data-role="export">—</strong></div>` })}
        ${hasBattery ? statCard({ id: "card-battery", accent: "battery", iconName: "battery", labelKey: "dashboard.battery_power",
          extra: `<div class="stat-sub"><span data-i18n="dashboard.battery_soc"></span>: <strong data-role="soc">—</strong></div>` }) : ""}
        ${showLoad ? statCard({ id: "card-load", accent: "primary", iconName: "cpu", labelKey: "dashboard.load_watts" }) : ""}
        ${showPv1 ? statCard({ id: "card-pv1", accent: "solar", iconName: "sun", labelKey: "dashboard.pv1",
          extra: `<div class="stat-sub" data-role="detail"></div>` }) : ""}
        ${showPv2 ? statCard({ id: "card-pv2", accent: "solar", iconName: "sun", labelKey: "dashboard.pv2",
          extra: `<div class="stat-sub" data-role="detail"></div>` }) : ""}
        ${showInvTemp ? statCard({ id: "card-invtemp", accent: "primary", iconName: "thermometer", labelKey: "dashboard.inverter_temperature" }) : ""}
      </div>

      <div class="grid metrics" style="margin-top:16px;">
        <div class="stat-card accent-primary" id="card-energy-today">
          <div class="stat-head"><span class="stat-label"><span class="icon-badge primary">${Icon.sun()}</span> ${i18n.t("dashboard.energy_today")}</span></div>
          <div class="stat-sub"><span data-i18n="dashboard.imported"></span>: <strong data-role="imp">—</strong> ${i18n.t("common.kwh")}</div>
          <div class="stat-sub"><span data-i18n="dashboard.exported"></span>: <strong data-role="exp">—</strong> ${i18n.t("common.kwh")}</div>
        </div>
        <div class="stat-card accent-primary" id="card-energy-total">
          <div class="stat-head"><span class="stat-label"><span class="icon-badge primary">${Icon.sun()}</span> ${i18n.t("dashboard.energy_total")}</span></div>
          <div class="stat-sub"><span data-i18n="dashboard.imported"></span>: <strong data-role="imp">—</strong> ${i18n.t("common.kwh")}</div>
          <div class="stat-sub"><span data-i18n="dashboard.exported"></span>: <strong data-role="exp">—</strong> ${i18n.t("common.kwh")}</div>
        </div>
        <div class="stat-card accent-grid" id="card-wifi">
          <div class="stat-head"><span class="stat-label"><span class="icon-badge grid">${Icon.wifi()}</span> ${i18n.t("dashboard.wifi")}</span></div>
          <div class="stat-value" data-role="value" style="font-size:16px;">—</div>
          <div class="stat-sub" data-role="detail"></div>
        </div>
        <div class="stat-card accent-primary" id="card-source">
          <div class="stat-head"><span class="stat-label"><span class="icon-badge primary">${Icon.router()}</span> ${i18n.t("dashboard.source_status")}</span></div>
          <div class="stat-value" data-role="value" style="font-size:16px;">—</div>
          <div class="stat-sub" data-role="detail"></div>
        </div>
      </div>

      ${tempsEnabled ? `
      <div class="card" style="margin-top:16px;">
        <div class="card-title">${Icon.thermometer()} <span data-i18n="dashboard.temperatures"></span></div>
        <div class="grid metrics" id="temps-grid"></div>
      </div>` : ""}

      <div class="card" style="margin-top:16px;">
        <div class="card-title">${Icon.plug()} <span data-i18n="dashboard.relays"></span></div>
        <div class="relay-list" id="relay-list"></div>
      </div>
    `;
  }

  function relayCardHtml(i, r) {
    return `
      <div class="relay-card" data-relay="${i}">
        <div class="relay-top">
          <div class="relay-icon ${r.on ? "on" : ""}" data-role="icon">${Icon.plug()}</div>
          <div class="relay-body">
            <div class="relay-name">${i18n.t("dashboard.relay")} ${i + 1}</div>
            <div class="relay-state" data-role="state"></div>
          </div>
        </div>
        <div class="relay-toggle-group" data-role="toggle">
          <button type="button" data-mode="auto" data-i18n="dashboard.relay_auto"></button>
          <button type="button" data-mode="on" data-i18n="dashboard.relay_on"></button>
          <button type="button" data-mode="off" data-i18n="dashboard.relay_off"></button>
        </div>
      </div>`;
  }

  function tempCardHtml(labelKey, value) {
    return `
      <div class="stat-card accent-primary">
        <div class="stat-head"><span class="stat-label"><span class="icon-badge primary">${Icon.thermometer()}</span> ${i18n.t(labelKey)}</span></div>
        <div class="stat-value">${value === undefined || value === null ? "—" : fmtNumber(value, 1)}<span class="unit">${i18n.t("common.celsius")}</span></div>
      </div>`;
  }

  let draggingPwm = false;

  function render(status) {
    const key = skeletonKeyFor(status);
    if (key !== skeletonKey) {
      container.innerHTML = buildSkeleton(status);
      i18n.applyTo(container);
      wireControls();
      skeletonKey = key;
    }
    updateValues(status);
  }

  function wireControls() {
    const modeSeg = container.querySelector("#mode-segmented");
    if (modeSeg) {
      modeSeg.addEventListener("click", async (e) => {
        const btn = e.target.closest("button[data-mode]");
        if (!btn) return;
        try {
          await ctx.api.postControl({ workingMode: btn.dataset.mode });
          toast(i18n.t("common.saved"), "success");
        } catch {
          toast(i18n.t("errors.save_failed"), "error");
        }
      });
    }
    const range = container.querySelector("#manual-pwm-range");
    const out = container.querySelector("#manual-pwm-out");
    if (range) {
      range.addEventListener("input", () => {
        draggingPwm = true;
        out.textContent = `${range.value}%`;
      });
      range.addEventListener("pointerup", () => { setTimeout(() => (draggingPwm = false), 400); });
    }
    const applyBtn = container.querySelector("#manual-pwm-apply");
    if (applyBtn) {
      applyBtn.addEventListener("click", async () => {
        try {
          await ctx.api.postControl({ manualPwmPercent: Number(range.value) });
          toast(i18n.t("common.saved"), "success");
        } catch {
          toast(i18n.t("errors.save_failed"), "error");
        }
      });
    }
    const relayList = container.querySelector("#relay-list");
    if (relayList) {
      relayList.addEventListener("click", async (e) => {
        const btn = e.target.closest("button[data-mode]");
        if (!btn) return;
        const card = e.target.closest(".relay-card");
        const index = Number(card.dataset.relay);
        try {
          await ctx.api.postControl({ relay: { index, mode: btn.dataset.mode } });
          toast(i18n.t("common.saved"), "success");
        } catch {
          toast(i18n.t("errors.save_failed"), "error");
        }
      });
    }
  }

  function updateValues(status) {
    const f = status.fields || {};
    const set = (sel, text) => { const n = container.querySelector(sel); if (n) n.textContent = text; };

    // mode segmented
    const modeSeg = container.querySelector("#mode-segmented");
    if (modeSeg) {
      modeSeg.querySelectorAll("button").forEach((b) => b.classList.toggle("active", b.dataset.mode === status.workingMode));
    }
    const manualRow = container.querySelector("#manual-pwm-row");
    if (manualRow) manualRow.classList.toggle("hidden", status.workingMode !== "manual");
    const range = container.querySelector("#manual-pwm-range");
    const out = container.querySelector("#manual-pwm-out");
    if (range && !draggingPwm) {
      range.value = status.pwmPercent ?? 0;
      if (out) out.textContent = `${status.pwmPercent ?? 0}%`;
    }
    const gaugeHolder = container.querySelector("#pwm-gauge-holder");
    if (gaugeHolder) {
      gaugeHolder.innerHTML = gaugeSvg({ percent: status.pwmPercent ?? 0, size: 88, color: "var(--color-primary)", label: `${Math.round(status.pwmPercent ?? 0)}%` });
    }

    if (f.solarWatts !== false) {
      const node = container.querySelector("#card-solar [data-role='value']");
      if (node) node.innerHTML = `${fmtWatts(status.solar && status.solar.watts)}<span class="unit">${i18n.t("common.watts")}</span>`;
    }
    if (f.solarWattsToday !== false) {
      set("#card-solar [data-role='today']", `${fmtKwh(status.solar && status.solar.today)} ${i18n.t("common.kwh")}`);
    }

    const gridCard = container.querySelector("#card-grid");
    if (gridCard) {
      gridCard.querySelector("[data-role='value']").innerHTML = `${fmtWatts(status.grid && status.grid.watts)}<span class="unit">${i18n.t("common.watts")}</span>`;
      const gw = (status.grid && status.grid.watts) || 0;
      gridCard.querySelector("[data-role='import']").textContent = gw > 0 ? `${fmtWatts(gw)} W` : "0 W";
      gridCard.querySelector("[data-role='export']").textContent = gw < 0 ? `${fmtWatts(Math.abs(gw))} W` : "0 W";
    }

    const battCard = container.querySelector("#card-battery");
    if (battCard && status.battery) {
      battCard.querySelector("[data-role='value']").innerHTML = `${fmtWatts(status.battery.watts)}<span class="unit">${i18n.t("common.watts")}</span>`;
      const soc = status.battery.soc;
      battCard.querySelector("[data-role='soc']").textContent = soc === undefined ? "—" : `${fmtNumber(soc)}%`;
    }

    const loadCard = container.querySelector("#card-load");
    if (loadCard) loadCard.querySelector("[data-role='value']").innerHTML = `${fmtWatts(status.loadWatts)}<span class="unit">${i18n.t("common.watts")}</span>`;

    const pv1Card = container.querySelector("#card-pv1");
    if (pv1Card && status.pv1) {
      pv1Card.querySelector("[data-role='value']").innerHTML = `${fmtWatts(status.pv1.watts)}<span class="unit">${i18n.t("common.watts")}</span>`;
      pv1Card.querySelector("[data-role='detail']").textContent = `${fmtNumber(status.pv1.voltage, 1)} V · ${fmtNumber(status.pv1.current, 1)} A`;
    }
    const pv2Card = container.querySelector("#card-pv2");
    if (pv2Card && status.pv2) {
      pv2Card.querySelector("[data-role='value']").innerHTML = `${fmtWatts(status.pv2.watts)}<span class="unit">${i18n.t("common.watts")}</span>`;
      pv2Card.querySelector("[data-role='detail']").textContent = `${fmtNumber(status.pv2.voltage, 1)} V · ${fmtNumber(status.pv2.current, 1)} A`;
    }
    const invCard = container.querySelector("#card-invtemp");
    if (invCard) invCard.querySelector("[data-role='value']").innerHTML = `${fmtNumber(status.inverterTemperature, 1)}<span class="unit">${i18n.t("common.celsius")}</span>`;

    const et = status.energyToday || {};
    const etCard = container.querySelector("#card-energy-today");
    if (etCard) {
      etCard.querySelector("[data-role='imp']").textContent = fmtKwh(et.importedKwh);
      etCard.querySelector("[data-role='exp']").textContent = fmtKwh(et.exportedKwh);
    }
    const eTot = status.energyTotal || {};
    const etotCard = container.querySelector("#card-energy-total");
    if (etotCard) {
      etotCard.querySelector("[data-role='imp']").textContent = fmtKwh(eTot.importedKwh);
      etotCard.querySelector("[data-role='exp']").textContent = fmtKwh(eTot.exportedKwh);
    }

    const wifiCard = container.querySelector("#card-wifi");
    if (wifiCard && status.wifi) {
      const connected = status.wifi.connected;
      wifiCard.querySelector("[data-role='value']").textContent = connected ? i18n.t("dashboard.wifi_connected") : i18n.t("dashboard.wifi_disconnected");
      wifiCard.querySelector("[data-role='detail']").textContent = connected
        ? `${status.wifi.ssid || ""} · ${status.wifi.rssi ?? "—"} dBm · ${status.wifi.ip || ""}`
        : "";
    }

    const sourceCard = container.querySelector("#card-source");
    if (sourceCard) {
      sourceCard.querySelector("[data-role='value']").textContent = sourceStatusLabel(status, i18n);
      sourceCard.querySelector("[data-role='detail']").textContent = `${i18n.t("dashboard.uptime")}: ${fmtUptime(status.uptimeSeconds)}`;
    }

    if (status.temperatures && status.temperatures.enabled) {
      const grid = container.querySelector("#temps-grid");
      if (grid) {
        grid.innerHTML = [
          tempCardHtml("dashboard.temp_thermo", status.temperatures.thermo),
          tempCardHtml("dashboard.temp_triac", status.temperatures.triac),
          tempCardHtml("dashboard.temp_custom", status.temperatures.custom),
        ].join("");
      }
    }

    const relayList = container.querySelector("#relay-list");
    if (relayList && status.relays) {
      const existing = relayList.querySelectorAll(".relay-card");
      if (existing.length !== status.relays.length) {
        relayList.innerHTML = status.relays.map((r, i) => relayCardHtml(i, r)).join("");
        i18n.applyTo(relayList);
      }
      status.relays.forEach((r, i) => {
        const card = relayList.children[i];
        if (!card) return;
        card.querySelector("[data-role='icon']").classList.toggle("on", !!r.on);
        card.querySelector("[data-role='state']").textContent = r.auto
          ? `${i18n.t("common.auto")} · ${r.on ? i18n.t("common.on") : i18n.t("common.off")}`
          : `${i18n.t("common.manual")} · ${r.on ? i18n.t("common.on") : i18n.t("common.off")}`;
        card.querySelectorAll("[data-role='toggle'] button").forEach((b) => {
          const active = (b.dataset.mode === "auto" && r.auto) ||
            (b.dataset.mode === "on" && !r.auto && r.on) ||
            (b.dataset.mode === "off" && !r.auto && !r.on);
          b.classList.toggle("active", active);
          b.classList.toggle("off-state", b.dataset.mode === "off" && active);
        });
      });
    }
  }

  const status = ctx.getStatus();
  container.innerHTML = buildSkeleton(status || { fields: {}, relays: [{}, {}, {}, {}] });
  i18n.applyTo(container);
  wireControls();
  skeletonKey = status ? skeletonKeyFor(status) : null;
  if (status) updateValues(status);

  const unsub = ctx.onStatus(render);
  return () => unsub();
}
