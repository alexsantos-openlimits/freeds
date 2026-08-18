import { section, row, textInput, numberInput, switchInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";

const MODE_GROUPS = [
  { key: "mode_group_meter", modes: [1, 2, 3, 4] },
  { key: "mode_group_inverter", modes: [21, 22, 23, 24, 25, 26, 27, 28] },
  { key: "mode_group_mqtt", modes: [41, 42] },
  { key: "mode_group_modbustcp", modes: [61, 62, 63, 64, 65, 66, 67, 68, 80] },
];

const SERIAL_METER_MODES = [1, 2, 3, 4];
const ESP01_MODES = [21];
const SOURCE_IP_MODES = [22, 23, 24, 25, 26, 27, 28, 61, 62, 63, 64, 65, 66, 67, 68, 80];
const SOLAX_VERSION_MODES = [21, 22, 23];
const METER_ID_MODES = [1, 2, 3, 4, 61, 62, 63, 64, 65, 66, 67, 68, 80];
const SLAVE_MODES = [27];

const BAUD_OPTIONS = [300, 600, 1200, 2400, 4800, 9600, 19200, 38400];

function visibility(mode) {
  return {
    serialMeter: SERIAL_METER_MODES.includes(mode),
    esp01: ESP01_MODES.includes(mode),
    sourceIp: SOURCE_IP_MODES.includes(mode),
    solaxVersion: SOLAX_VERSION_MODES.includes(mode),
    meterId: METER_ID_MODES.includes(mode),
    slave: SLAVE_MODES.includes(mode),
  };
}

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("surplus.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="surplus.title"></h2></div>
    <div class="card"><div id="surplus-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  let cfg;
  try {
    cfg = (await api.getConfig()).surplus || {};
  } catch {
    container.querySelector("#surplus-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }

  function modeOptionsHtml(selected) {
    return MODE_GROUPS.map((g) => `
      <optgroup label="${i18n.t(`surplus.${g.key}`)}">
        ${g.modes.map((m) => `<option value="${m}" ${m === selected ? "selected" : ""}>${m} — ${i18n.t(`surplus.modes.${m}`)}</option>`).join("")}
      </optgroup>`).join("");
  }

  function draw() {
    const mode = Number(cfg.mode) || 1;
    const vis = visibility(mode);
    const form = container.querySelector("#surplus-form");
    form.innerHTML = `
      ${section("surplus.title", i18n, `
        ${row({ i18n, labelKey: "surplus.mode", controlHtml: `<select id="mode">${modeOptionsHtml(mode)}</select>` })}
      `)}
      <div id="mode-dependent-fields"></div>
      ${section("surplus.section_advanced", i18n, `
        ${row({ i18n, labelKey: "surplus.change_grid_sign", controlHtml: switchInput({ id: "changeGridSign", checked: cfg.changeGridSign, i18n }) })}
        ${row({ i18n, labelKey: "surplus.use_external_meter", controlHtml: switchInput({ id: "useExternalMeter", checked: cfg.useExternalMeter, i18n }) })}
        ${row({ i18n, labelKey: "surplus.use_clamp", controlHtml: switchInput({ id: "useClamp", checked: cfg.useClamp, i18n }) })}
        ${row({ i18n, labelKey: "surplus.clamp_calibration", controlHtml: numberInput({ id: "clampCalibration", value: cfg.clampCalibration ?? 0, step: "0.01" }) })}
        ${row({ i18n, labelKey: "surplus.clamp_voltage", controlHtml: numberInput({ id: "clampVoltage", value: cfg.clampVoltage ?? 230, step: "0.1" }) })}
        ${row({ i18n, labelKey: "surplus.attached_load_watts", controlHtml: numberInput({ id: "attachedLoadWatts", value: cfg.attachedLoadWatts ?? 0, min: 0 }) })}
        ${row({ i18n, labelKey: "surplus.use_solar_as_mptt", controlHtml: switchInput({ id: "useSolarAsMptt", checked: cfg.useSolarAsMptt, i18n }) })}
        ${row({ i18n, labelKey: "surplus.use_bmv", controlHtml: switchInput({ id: "useBmv", checked: cfg.useBmv, i18n }) })}
        ${row({ i18n, labelKey: "surplus.grid_phase", controlHtml: numberInput({ id: "gridPhase", value: cfg.gridPhase ?? 0, min: 0, max: 2 }) })}
        ${row({ i18n, labelKey: "surplus.max_error_time", controlHtml: numberInput({ id: "maxErrorTimeMs", value: cfg.maxErrorTimeMs ?? 30000, min: 0, step: 500 }), hintKey: "common.milliseconds" })}
        ${row({ i18n, labelKey: "surplus.poll_interval", controlHtml: numberInput({ id: "pollIntervalMs", value: cfg.pollIntervalMs ?? 2000, min: 100, step: 100 }), hintKey: "common.milliseconds" })}
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);
    drawDependent(vis);

    form.querySelector("#mode").addEventListener("change", (e) => {
      cfg.mode = Number(e.target.value);
      drawDependent(visibility(cfg.mode));
    });

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const body = {
        mode: Number(readVal(form, "mode")),
        changeGridSign: readVal(form, "changeGridSign"),
        useExternalMeter: readVal(form, "useExternalMeter"),
        useClamp: readVal(form, "useClamp"),
        clampCalibration: readVal(form, "clampCalibration"),
        clampVoltage: readVal(form, "clampVoltage"),
        attachedLoadWatts: readVal(form, "attachedLoadWatts"),
        useSolarAsMptt: readVal(form, "useSolarAsMptt"),
        useBmv: readVal(form, "useBmv"),
        gridPhase: readVal(form, "gridPhase"),
        maxErrorTimeMs: readVal(form, "maxErrorTimeMs"),
        pollIntervalMs: readVal(form, "pollIntervalMs"),
      };
      const vis2 = visibility(body.mode);
      if (vis2.sourceIp) body.sourceIp = readVal(form, "sourceIp");
      if (vis2.esp01) { body.esp01Ssid = readVal(form, "esp01Ssid"); body.esp01Password = readVal(form, "esp01Password"); }
      if (vis2.serialMeter) body.meterBaud = Number(readVal(form, "meterBaud"));
      if (vis2.meterId) body.meterId = readVal(form, "meterId");
      if (vis2.solaxVersion) body.solaxVersion = readVal(form, "solaxVersion");
      if (vis2.slave) body.pwmSlaveOnPercent = readVal(form, "pwmSlaveOnPercent");
      await handleSave(() => api.postSurplusConfig(body), i18n);
    });
  }

  function drawDependent(vis) {
    const holder = container.querySelector("#mode-dependent-fields");
    let html = "";
    if (vis.sourceIp) html += row({ i18n, labelKey: "surplus.source_ip", controlHtml: textInput({ id: "sourceIp", value: cfg.sourceIp, maxlength: 15, placeholder: "192.168.1.50" }) });
    if (vis.esp01) {
      html += row({ i18n, labelKey: "surplus.esp01_ssid", controlHtml: textInput({ id: "esp01Ssid", value: cfg.esp01Ssid, maxlength: 32 }) });
      html += row({ i18n, labelKey: "surplus.esp01_password", controlHtml: textInput({ id: "esp01Password", value: cfg.esp01Password, type: "password", maxlength: 64 }) });
    }
    if (vis.serialMeter) html += row({ i18n, labelKey: "surplus.meter_baud", controlHtml: `<select id="meterBaud">${BAUD_OPTIONS.map((b) => `<option value="${b}" ${b === (cfg.meterBaud ?? 9600) ? "selected" : ""}>${b} bps</option>`).join("")}</select>` });
    if (vis.meterId) html += row({ i18n, labelKey: "surplus.meter_id", controlHtml: numberInput({ id: "meterId", value: cfg.meterId ?? 1, min: 1, max: 247 }) });
    if (vis.solaxVersion) html += row({ i18n, labelKey: "surplus.solax_version", controlHtml: numberInput({ id: "solaxVersion", value: cfg.solaxVersion ?? 1, min: 1, max: 3 }) });
    if (vis.slave) html += row({ i18n, labelKey: "surplus.pwm_slave_on_percent", controlHtml: numberInput({ id: "pwmSlaveOnPercent", value: cfg.pwmSlaveOnPercent ?? 0, min: 0, max: 100 }) });
    holder.innerHTML = html ? section("surplus.section_source", i18n, html) : "";
    i18n.applyTo(holder);
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
