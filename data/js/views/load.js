import { section, row, textInput, numberInput, switchInput, selectInput, timeInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";
import { hhmmIntToTimeInput, timeInputToHhmmInt } from "../components/dom.js";

const PWM_FREQ_OPTIONS = [
  [100, "10 Hz"], [125, "12.5 Hz"], [250, "25 Hz"], [5000, "500 Hz"],
  [10000, "1.0 KHz"], [15000, "1.5 KHz"], [20000, "2.0 KHz"], [25000, "2.5 KHz"], [30000, "3.0 KHz"],
];

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("load.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="load.title"></h2></div>
    <div class="card"><div id="load-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  let cfg;
  try {
    cfg = (await api.getConfig()).load || {};
  } catch {
    container.querySelector("#load-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }
  const outputs = cfg.outputs && cfg.outputs.length === 4 ? cfg.outputs : [{}, {}, {}, {}];
  const pid = cfg.pid || {};
  const offGrid = cfg.offGrid || {};

  function outputsTableHtml() {
    return `
      <div style="overflow-x:auto;">
      <table style="width:100%; border-collapse:collapse; min-width:560px;">
        <thead>
          <tr style="text-align:left; font-size:12.5px; color:var(--color-text-muted);">
            <th style="padding:6px 8px;"></th>
            <th style="padding:6px 8px;" data-i18n="load.min_percent"></th>
            <th style="padding:6px 8px;" data-i18n="load.on_watts"></th>
            <th style="padding:6px 8px;" data-i18n="load.off_watts"></th>
            <th style="padding:6px 8px;" data-i18n="load.manual"></th>
          </tr>
        </thead>
        <tbody>
          ${outputs.map((o, i) => `
            <tr style="border-top:1px solid var(--color-border);">
              <td style="padding:6px 8px; font-weight:600;">${i18n.t("load.relay")} ${i + 1}</td>
              <td style="padding:6px 8px;">${numberInput({ id: `out${i}min`, value: o.minPercent ?? 0, min: 0, max: 100 })}</td>
              <td style="padding:6px 8px;">${numberInput({ id: `out${i}on`, value: o.onWatts ?? 0, min: 0 })}</td>
              <td style="padding:6px 8px;">${numberInput({ id: `out${i}off`, value: o.offWatts ?? 0, min: 0 })}</td>
              <td style="padding:6px 8px;">${switchInput({ id: `out${i}man`, checked: o.manual, i18n })}</td>
            </tr>`).join("")}
        </tbody>
      </table>
      </div>`;
  }

  function draw() {
    const form = container.querySelector("#load-form");
    form.innerHTML = `
      ${section("load.pwm_section", i18n, `
        ${row({ i18n, labelKey: "load.pwm_enabled", controlHtml: switchInput({ id: "pwmEnabled", checked: cfg.pwmEnabled, i18n }) })}
        ${row({ i18n, labelKey: "load.manual_mode", controlHtml: switchInput({ id: "manualMode", checked: cfg.manualMode, i18n }) })}
        ${row({ i18n, labelKey: "load.pot_target", controlHtml: numberInput({ id: "potTarget", value: cfg.potTarget ?? 0, min: 0 }) })}
        ${row({ i18n, labelKey: "load.manual_control_percent", controlHtml: numberInput({ id: "manualControlPercent", value: cfg.manualControlPercent ?? 0, min: 0, max: 100 }) })}
        ${row({ i18n, labelKey: "load.auto_control_percent", controlHtml: numberInput({ id: "autoControlPercent", value: cfg.autoControlPercent ?? 0, min: 0, max: 100 }) })}
        ${row({ i18n, labelKey: "load.pwm_frequency", controlHtml: selectInput({ id: "pwmFrequencyHz", options: PWM_FREQ_OPTIONS, value: cfg.pwmFrequencyHz ?? 30000 }) })}
        ${row({ i18n, labelKey: "load.dimmer_low_cost", controlHtml: switchInput({ id: "dimmerLowCost", checked: cfg.dimmerLowCost, i18n }) })}
        ${row({ i18n, labelKey: "load.max_pwm_low_cost", controlHtml: numberInput({ id: "maxPwmLowCost", value: cfg.maxPwmLowCost ?? 100, min: 0, max: 100 }) })}
        ${row({ i18n, labelKey: "load.max_watts_tariff", controlHtml: numberInput({ id: "maxWattsTariff", value: cfg.maxWattsTariff ?? 0, min: 0 }) })}
      `)}
      ${section("load.pid_section", i18n, `
        ${row({ i18n, labelKey: "load.pid_kp", controlHtml: numberInput({ id: "kp", value: pid.kp ?? 0, step: "0.01" }) })}
        ${row({ i18n, labelKey: "load.pid_ki", controlHtml: numberInput({ id: "ki", value: pid.ki ?? 0, step: "0.01" }) })}
        ${row({ i18n, labelKey: "load.pid_kd", controlHtml: numberInput({ id: "kd", value: pid.kd ?? 0, step: "0.01" }) })}
      `)}
      <div class="form-section">
        <div class="form-section-title">${i18n.t("load.outputs_section")}</div>
        ${outputsTableHtml()}
      </div>
      ${section("load.off_grid_section", i18n, `
        ${row({ i18n, labelKey: "load.off_grid_enabled", controlHtml: switchInput({ id: "offGridEnabled", checked: offGrid.enabled, i18n }) })}
        ${row({ i18n, labelKey: "load.off_grid_use_voltage", controlHtml: switchInput({ id: "offGridUseVoltage", checked: offGrid.useVoltage, i18n }) })}
        ${row({ i18n, labelKey: "load.off_grid_soc_threshold", controlHtml: numberInput({ id: "offGridSoc", value: offGrid.socThreshold ?? 0, min: 0, max: 100 }) })}
        ${row({ i18n, labelKey: "load.off_grid_batt_watts_threshold", controlHtml: numberInput({ id: "offGridBattWatts", value: offGrid.battWattsThreshold ?? 0 }) })}
        ${row({ i18n, labelKey: "load.off_grid_battery_voltage", controlHtml: numberInput({ id: "offGridBattVoltage", value: offGrid.batteryVoltage ?? 0, step: "0.1" }) })}
        ${row({ i18n, labelKey: "load.off_grid_voltage_offset", controlHtml: numberInput({ id: "offGridVoltageOffset", value: offGrid.voltageOffset ?? 0, step: "0.1" }) })}
      `)}
      ${section("load.pot_man_section", i18n, `
        ${row({ i18n, labelKey: "load.pot_man_pwm_active", controlHtml: switchInput({ id: "potManPwmActive", checked: cfg.potManPwmActive, i18n }) })}
        ${row({ i18n, labelKey: "load.pot_man_pwm_watts", controlHtml: numberInput({ id: "potManPwmWatts", value: cfg.potManPwmWatts ?? 0, min: 0 }) })}
      `)}
      ${section("load.timer_section", i18n, `
        ${row({ i18n, labelKey: "load.timer_enabled", controlHtml: switchInput({ id: "timerEnabled", checked: cfg.timerEnabled, i18n }) })}
        ${row({ i18n, labelKey: "load.timer_start", controlHtml: timeInput({ id: "timerStart", value: hhmmIntToTimeInput(cfg.timerStartHHMM) }) })}
        ${row({ i18n, labelKey: "load.timer_stop", controlHtml: timeInput({ id: "timerStop", value: hhmmIntToTimeInput(cfg.timerStopHHMM) }) })}
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const body = {
        pwmEnabled: readVal(form, "pwmEnabled"),
        manualMode: readVal(form, "manualMode"),
        potTarget: readVal(form, "potTarget"),
        manualControlPercent: readVal(form, "manualControlPercent"),
        autoControlPercent: readVal(form, "autoControlPercent"),
        pwmFrequencyHz: Number(readVal(form, "pwmFrequencyHz")),
        dimmerLowCost: readVal(form, "dimmerLowCost"),
        maxPwmLowCost: readVal(form, "maxPwmLowCost"),
        maxWattsTariff: readVal(form, "maxWattsTariff"),
        pid: { kp: readVal(form, "kp"), ki: readVal(form, "ki"), kd: readVal(form, "kd") },
        outputs: [0, 1, 2, 3].map((i) => ({
          minPercent: readVal(form, `out${i}min`),
          onWatts: readVal(form, `out${i}on`),
          offWatts: readVal(form, `out${i}off`),
          manual: readVal(form, `out${i}man`),
        })),
        offGrid: {
          enabled: readVal(form, "offGridEnabled"),
          useVoltage: readVal(form, "offGridUseVoltage"),
          socThreshold: readVal(form, "offGridSoc"),
          battWattsThreshold: readVal(form, "offGridBattWatts"),
          batteryVoltage: readVal(form, "offGridBattVoltage"),
          voltageOffset: readVal(form, "offGridVoltageOffset"),
        },
        potManPwmActive: readVal(form, "potManPwmActive"),
        potManPwmWatts: readVal(form, "potManPwmWatts"),
        timerEnabled: readVal(form, "timerEnabled"),
        timerStartHHMM: timeInputToHhmmInt(readVal(form, "timerStart")),
        timerStopHHMM: timeInputToHhmmInt(readVal(form, "timerStop")),
      };
      await handleSave(() => api.postLoadConfig(body), i18n);
    });
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
