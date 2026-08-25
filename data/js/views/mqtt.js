import { section, row, textInput, numberInput, switchInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";
import { toast } from "../components/toast.js";

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("mqtt.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="mqtt.title"></h2></div>
    <div class="card" id="mqtt-status-card" style="margin-bottom:16px;"></div>
    <div class="card"><div id="mqtt-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  async function refreshStatus() {
    const el = container.querySelector("#mqtt-status-card");
    if (!el) return;
    try {
      const status = await api.getStatus();
      const m = status.mqtt || {};
      if (!m.enabled) {
        el.innerHTML = `<span class="status-pill">${i18n.t("mqtt.status_disabled")}</span>`;
      } else if (m.connected) {
        el.innerHTML = `<span class="status-pill ok"><span class="dot"></span> ${i18n.t("mqtt.status_connected")}</span>`;
      } else {
        el.innerHTML = `<span class="status-pill bad"><span class="dot"></span> ${i18n.t("mqtt.status_disconnected")}</span>`;
      }
    } catch {
      /* mantém o último estado conhecido */
    }
  }
  refreshStatus();

  let cfg;
  try {
    cfg = (await api.getConfig()).mqtt || {};
  } catch {
    container.querySelector("#mqtt-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }
  const topics = cfg.relayTopic || ["", "", "", ""];

  function draw() {
    const form = container.querySelector("#mqtt-form");
    form.innerHTML = `
      ${section("mqtt.title", i18n, `
        ${row({ i18n, labelKey: "mqtt.enable", controlHtml: switchInput({ id: "enabled", checked: cfg.enabled, i18n }) })}
        ${row({ i18n, labelKey: "mqtt.broker", controlHtml: textInput({ id: "broker", value: cfg.broker, maxlength: 64 }) })}
        ${row({ i18n, labelKey: "mqtt.port", controlHtml: numberInput({ id: "port", value: cfg.port ?? 1883, min: 1, max: 65535 }) })}
        ${row({ i18n, labelKey: "mqtt.user", controlHtml: textInput({ id: "user", value: cfg.user, maxlength: 32 }) })}
        ${row({ i18n, labelKey: "mqtt.password", controlHtml: textInput({ id: "password", value: cfg.password, type: "password", maxlength: 32 }) })}
        ${row({ i18n, labelKey: "mqtt.publish_interval", controlHtml: numberInput({ id: "publishIntervalMs", value: cfg.publishIntervalMs ?? 5000, min: 500, step: 500 }), hintKey: "common.milliseconds" })}
      `)}
      ${section("mqtt.topics", i18n, `
        ${row({ i18n, labelKey: "mqtt.relay_topic", controlHtml: textInput({ id: "relayTopic0", value: topics[0], maxlength: 80 }) + ` <span class="text-faint">1</span>` })}
        ${row({ i18n, labelKey: "mqtt.relay_topic", controlHtml: textInput({ id: "relayTopic1", value: topics[1], maxlength: 80 }) + ` <span class="text-faint">2</span>` })}
        ${row({ i18n, labelKey: "mqtt.relay_topic", controlHtml: textInput({ id: "relayTopic2", value: topics[2], maxlength: 80 }) + ` <span class="text-faint">3</span>` })}
        ${row({ i18n, labelKey: "mqtt.relay_topic", controlHtml: textInput({ id: "relayTopic3", value: topics[3], maxlength: 80 }) + ` <span class="text-faint">4</span>` })}
        ${row({ i18n, labelKey: "mqtt.solax_topic", controlHtml: textInput({ id: "solaxTopic", value: cfg.solaxTopic, maxlength: 80 }) })}
        ${row({ i18n, labelKey: "mqtt.meter_topic", controlHtml: textInput({ id: "meterTopic", value: cfg.meterTopic, maxlength: 80 }) })}
        ${row({ i18n, labelKey: "mqtt.soc_topic", controlHtml: textInput({ id: "socTopic", value: cfg.socTopic, maxlength: 80 }) })}
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const body = {
        enabled: readVal(form, "enabled"),
        broker: readVal(form, "broker"),
        port: readVal(form, "port"),
        user: readVal(form, "user"),
        password: readVal(form, "password"),
        publishIntervalMs: readVal(form, "publishIntervalMs"),
        relayTopic: [readVal(form, "relayTopic0"), readVal(form, "relayTopic1"), readVal(form, "relayTopic2"), readVal(form, "relayTopic3")],
        solaxTopic: readVal(form, "solaxTopic"),
        meterTopic: readVal(form, "meterTopic"),
        socTopic: readVal(form, "socTopic"),
      };
      await handleSave(() => api.postMqttConfig(body), i18n);

      if (body.enabled) {
        toast(i18n.t("mqtt.testing_connection"), "info");
        setTimeout(async () => {
          await refreshStatus();
          try {
            const status = await api.getStatus();
            const m = status.mqtt || {};
            toast(m.connected ? i18n.t("mqtt.status_connected") : i18n.t("mqtt.status_connect_failed"),
                  m.connected ? "success" : "error");
          } catch { /* ignore */ }
        }, 2500);
      } else {
        refreshStatus();
      }
    });
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
