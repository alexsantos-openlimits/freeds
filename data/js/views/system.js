import { section, row, textInput, numberInput, switchInput, selectInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";
import { toast } from "../components/toast.js";

function b64EncodeUtf8(str) {
  return btoa(unescape(encodeURIComponent(str)));
}

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("system.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="system.title"></h2></div>
    <div class="card"><div id="system-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  let cfg;
  try {
    cfg = (await api.getConfig()).system || {};
  } catch {
    container.querySelector("#system-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }
  const idx = cfg.domoticzIdx || [0, 0, 0];

  function draw() {
    const form = container.querySelector("#system-form");
    form.innerHTML = `
      ${section("system.language_section", i18n, `
        ${row({ i18n, labelKey: "system.language", controlHtml: selectInput({ id: "language", options: i18n.supported.map((l) => [l, l.toUpperCase()]), value: i18n.lang }) })}
        ${row({ i18n, labelKey: "system.timezone", controlHtml: textInput({ id: "timezone", value: cfg.timezone, placeholder: "Europe/Lisbon", maxlength: 40 }) })}
        ${row({ i18n, labelKey: "system.ntp_server", controlHtml: textInput({ id: "ntpServer", value: cfg.ntpServer, placeholder: "pool.ntp.org", maxlength: 60 }) })}
      `)}
      ${section("system.screen_section", i18n, `
        ${row({ i18n, labelKey: "system.oled_power", controlHtml: switchInput({ id: "oledPower", checked: cfg.oledPower, i18n }) })}
        ${row({ i18n, labelKey: "system.oled_auto_off", controlHtml: switchInput({ id: "oledAutoOff", checked: cfg.oledAutoOff, i18n }) })}
        ${row({ i18n, labelKey: "system.oled_auto_off_ms", controlHtml: numberInput({ id: "oledAutoOffMs", value: cfg.oledAutoOffMs ?? 60000, min: 1000, step: 1000 }), hintKey: "common.milliseconds" })}
        ${row({ i18n, labelKey: "system.flip_screen", controlHtml: switchInput({ id: "flipScreen", checked: cfg.flipScreen, i18n }) })}
        ${row({ i18n, labelKey: "system.oled_brightness", controlHtml: `
          <div class="range-row">
            <input type="range" id="oledBrightness" min="0" max="255" step="1" value="${cfg.oledBrightness ?? 200}">
            <span class="range-out" id="oledBrightnessOut">${cfg.oledBrightness ?? 200}</span>
          </div>` })}
      `)}
      ${section("system.integrations_section", i18n, `
        ${row({ i18n, labelKey: "system.alexa_control", controlHtml: switchInput({ id: "alexaControl", checked: cfg.alexaControl, i18n }) })}
        ${row({ i18n, labelKey: "system.domoticz_enabled", controlHtml: switchInput({ id: "domoticzEnabled", checked: cfg.domoticzEnabled, i18n }) })}
        ${row({ i18n, labelKey: "system.domoticz_idx_pwm", controlHtml: numberInput({ id: "domoticzIdx0", value: idx[0] ?? 0, min: 0 }) })}
        ${row({ i18n, labelKey: "system.domoticz_idx_manual", controlHtml: numberInput({ id: "domoticzIdx1", value: idx[1] ?? 0, min: 0 }) })}
        ${row({ i18n, labelKey: "system.domoticz_idx_screen", controlHtml: numberInput({ id: "domoticzIdx2", value: idx[2] ?? 0, min: 0 }) })}
      `)}
      ${section("system.logging_section", i18n, `
        ${row({ i18n, labelKey: "system.weblog_enabled", controlHtml: switchInput({ id: "weblogEnabled", checked: cfg.weblogEnabled, i18n }) })}
        ${row({ i18n, labelKey: "system.serial_log_enabled", controlHtml: switchInput({ id: "serialLogEnabled", checked: cfg.serialLogEnabled, i18n }) })}
        ${row({ i18n, labelKey: "system.debug_mode", controlHtml: switchInput({ id: "debugMode", checked: (cfg.debugFlags ?? 0) & 1, i18n }), hintKey: "system.debug_mode_hint" })}
      `)}
      ${section("system.security_section", i18n, `
        ${row({ i18n, labelKey: "system.admin_password_new", controlHtml: textInput({ id: "newPassword", type: "password", maxlength: 32 }) })}
        ${row({ i18n, labelKey: "system.admin_password_confirm", controlHtml: textInput({ id: "confirmPassword", type: "password", maxlength: 32 }) })}
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);

    const brightness = form.querySelector("#oledBrightness");
    const brightnessOut = form.querySelector("#oledBrightnessOut");
    brightness.addEventListener("input", () => { brightnessOut.textContent = brightness.value; });

    form.querySelector("#language").addEventListener("change", (e) => {
      i18n.setLang(e.target.value);
    });

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const newPass = readVal(form, "newPassword");
      const confirmPass = readVal(form, "confirmPassword");
      if (newPass || confirmPass) {
        if (newPass !== confirmPass) {
          toast(i18n.t("system.password_mismatch"), "error");
          return;
        }
      }
      const body = {
        language: readVal(form, "language"),
        timezone: readVal(form, "timezone"),
        ntpServer: readVal(form, "ntpServer"),
        oledPower: readVal(form, "oledPower"),
        oledAutoOff: readVal(form, "oledAutoOff"),
        oledAutoOffMs: readVal(form, "oledAutoOffMs"),
        flipScreen: readVal(form, "flipScreen"),
        oledBrightness: Number(brightness.value),
        alexaControl: readVal(form, "alexaControl"),
        domoticzEnabled: readVal(form, "domoticzEnabled"),
        domoticzIdx: [readVal(form, "domoticzIdx0"), readVal(form, "domoticzIdx1"), readVal(form, "domoticzIdx2")],
        weblogEnabled: readVal(form, "weblogEnabled"),
        serialLogEnabled: readVal(form, "serialLogEnabled"),
        debugFlags: readVal(form, "debugMode") ? 1 : 0,
      };
      if (newPass) body.adminPasswordB64 = b64EncodeUtf8(newPass);
      await handleSave(() => api.postSystemConfig(body), i18n);
      form.querySelector("#newPassword").value = "";
      form.querySelector("#confirmPassword").value = "";
    });
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
