import { section, row, textInput, numberInput, switchInput, selectInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";

function addrToHex(addr) {
  if (!addr || !addr.length) return "";
  if (addr.every((b) => !b)) return "";
  return addr.map((b) => Number(b).toString(16).padStart(2, "0")).join("").toUpperCase();
}

function hexToAddr(hex) {
  if (!hex) return [0, 0, 0, 0, 0, 0, 0, 0];
  const bytes = [];
  for (let i = 0; i < 16; i += 2) bytes.push(parseInt(hex.substr(i, 2), 16) || 0);
  while (bytes.length < 8) bytes.push(0);
  return bytes;
}

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("temperature.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="temperature.title"></h2></div>
    <div class="card"><div id="temp-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  let cfg;
  let sensors = [];
  try {
    const fullCfg = await api.getConfig();
    cfg = fullCfg.temperature || {};
  } catch {
    container.querySelector("#temp-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }
  try {
    const res = await api.getTemperatureSensors();
    sensors = (res && res.sensors) || [];
  } catch {
    sensors = [];
  }
  sensors = sensors.map((s) => (typeof s === "string" ? s.toUpperCase() : addrToHex(s)));

  function sensorSelectHtml(id, currentAddr) {
    const currentHex = addrToHex(currentAddr);
    const known = new Set(sensors);
    if (currentHex && !known.has(currentHex)) known.add(currentHex);
    const options = [["", i18n.t("temperature.no_sensor")]].concat(
      Array.from(known).map((hex) => [hex, hex])
    );
    return selectInput({ id, options, value: currentHex });
  }

  function draw() {
    const form = container.querySelector("#temp-form");
    const modeOptions = [0, 1, 2, 3].map((m) => [m, i18n.t(`temperature.mode_${m}`)]);
    form.innerHTML = `
      ${section("temperature.title", i18n, `
        ${row({ i18n, labelKey: "temperature.enabled", controlHtml: switchInput({ id: "enabled", checked: cfg.enabled, i18n }) })}
        ${row({ i18n, labelKey: "temperature.turn_on", controlHtml: numberInput({ id: "turnOnC", value: cfg.turnOnC ?? 0, min: 0, max: 99 }), hintKey: null })}
        ${row({ i18n, labelKey: "temperature.turn_off", controlHtml: numberInput({ id: "turnOffC", value: cfg.turnOffC ?? 0, min: 0, max: 99 }) })}
        ${row({ i18n, labelKey: "temperature.mode", controlHtml: selectInput({ id: "mode", options: modeOptions, value: cfg.mode ?? 0 }) })}
      `)}
      ${section("temperature.thermo_sensor", i18n, `
        ${row({ i18n, labelKey: "temperature.thermo_sensor", controlHtml: sensorSelectHtml("thermoAddr", cfg.thermoSensorAddr) })}
        ${row({ i18n, labelKey: "temperature.triac_sensor", controlHtml: sensorSelectHtml("triacAddr", cfg.triacSensorAddr) })}
        ${row({ i18n, labelKey: "temperature.custom_sensor_name", controlHtml: textInput({ id: "customName", value: cfg.customSensorName, maxlength: 30 }) })}
        ${row({ i18n, labelKey: "temperature.custom_sensor", controlHtml: sensorSelectHtml("customAddr", cfg.customSensorAddr) })}
        <div class="btn-row" style="margin-top:0;">
          <button type="button" class="btn btn-secondary" id="scan-btn">${Icon.refresh()} <span data-i18n="temperature.scan_sensors"></span></button>
        </div>
        <p class="field-hint" data-i18n="temperature.sensors_note" style="margin-top:8px;"></p>
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);

    form.querySelector("#scan-btn").addEventListener("click", async (e) => {
      const btn = e.currentTarget;
      btn.disabled = true;
      const original = btn.innerHTML;
      btn.innerHTML = i18n.t("temperature.scanning");
      try {
        const res = await api.getTemperatureSensors();
        sensors = ((res && res.sensors) || []).map((s) => (typeof s === "string" ? s.toUpperCase() : addrToHex(s)));
        draw();
      } finally {
        btn.disabled = false;
        btn.innerHTML = original;
      }
    });

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const body = {
        enabled: readVal(form, "enabled"),
        turnOnC: readVal(form, "turnOnC"),
        turnOffC: readVal(form, "turnOffC"),
        mode: Number(readVal(form, "mode")),
        thermoSensorAddr: hexToAddr(readVal(form, "thermoAddr")),
        triacSensorAddr: hexToAddr(readVal(form, "triacAddr")),
        customSensorAddr: hexToAddr(readVal(form, "customAddr")),
        customSensorName: readVal(form, "customName"),
      };
      await handleSave(() => api.postTemperatureConfig(body), i18n);
    });
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
