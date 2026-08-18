import { section, row, textInput, switchInput, readVal, handleSave } from "../components/form.js";
import { Icon } from "../components/icons.js";
import { toast } from "../components/toast.js";

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("network.title");

  container.innerHTML = `
    <div class="view-header"><h2 data-i18n="network.title"></h2></div>
    <div class="card"><div id="network-form"><div class="empty-note" data-i18n="common.loading"></div></div></div>
  `;
  i18n.applyTo(container);

  let cfg;
  try {
    cfg = (await api.getConfig()).network || {};
  } catch {
    container.querySelector("#network-form").innerHTML = `<div class="empty-note" data-i18n="errors.fetch_config"></div>`;
    i18n.applyTo(container);
    return;
  }

  let lastSsidFocus = "ssid1";

  function draw() {
    const form = container.querySelector("#network-form");
    form.innerHTML = `
      ${section("network.wifi_settings", i18n, `
        ${row({ i18n, labelKey: "network.ssid1", controlHtml: textInput({ id: "ssid1", value: cfg.ssid1, maxlength: 32 }) })}
        ${row({ i18n, labelKey: "network.pass1", controlHtml: textInput({ id: "pass1", value: cfg.pass1, type: "password", maxlength: 64 }) })}
        ${row({ i18n, labelKey: "network.ssid2", controlHtml: textInput({ id: "ssid2", value: cfg.ssid2, maxlength: 32 }) })}
        ${row({ i18n, labelKey: "network.pass2", controlHtml: textInput({ id: "pass2", value: cfg.pass2, type: "password", maxlength: 64 }) })}
        <div class="form-row">
          <label class="field-label"></label>
          <div>
            <button type="button" class="btn btn-secondary" id="scan-btn">${Icon.wifi()} <span data-i18n="network.scan"></span></button>
            <div class="wifi-list hidden" id="wifi-list" style="margin-top:10px;"></div>
          </div>
        </div>
      `)}
      ${section("network.lan_settings", i18n, `
        ${row({ i18n, labelKey: "network.hostname", controlHtml: textInput({ id: "hostname", value: cfg.hostname, maxlength: 32 }) })}
        ${row({ i18n, labelKey: "network.dhcp", controlHtml: switchInput({ id: "dhcp", checked: cfg.dhcp, i18n }) })}
        <div id="static-ip-fields" class="${cfg.dhcp ? "hidden" : ""}">
          ${row({ i18n, labelKey: "network.ip_address", controlHtml: textInput({ id: "ip", value: cfg.ip, maxlength: 15 }) })}
          ${row({ i18n, labelKey: "network.gateway", controlHtml: textInput({ id: "gateway", value: cfg.gateway, maxlength: 15 }) })}
          ${row({ i18n, labelKey: "network.subnet", controlHtml: textInput({ id: "subnet", value: cfg.subnet, maxlength: 15 }) })}
          ${row({ i18n, labelKey: "network.dns1", controlHtml: textInput({ id: "dns1", value: cfg.dns1, maxlength: 15 }) })}
          ${row({ i18n, labelKey: "network.dns2", controlHtml: textInput({ id: "dns2", value: cfg.dns2, maxlength: 15 }) })}
        </div>
      `)}
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="save-btn">${Icon.check()} <span data-i18n="common.save"></span></button>
      </div>
    `;
    i18n.applyTo(form);

    form.querySelector("#dhcp").addEventListener("change", (e) => {
      form.querySelector("#static-ip-fields").classList.toggle("hidden", e.target.checked);
    });

    ["ssid1", "ssid2"].forEach((id) => {
      form.querySelector(`#${id}`).addEventListener("focus", () => { lastSsidFocus = id; });
    });

    form.querySelector("#scan-btn").addEventListener("click", async () => {
      const list = form.querySelector("#wifi-list");
      list.classList.remove("hidden");
      list.innerHTML = `<div class="wifi-item">${i18n.t("network.scanning")}</div>`;
      try {
        const res = await api.scanWifi();
        const nets = (res && res.networks) || [];
        if (!nets.length) {
          list.innerHTML = `<div class="wifi-item">${i18n.t("network.no_networks")}</div>`;
          return;
        }
        nets.sort((a, b) => (b.rssi || -999) - (a.rssi || -999));
        list.innerHTML = nets.map((n) => `
          <div class="wifi-item" data-ssid="${n.ssid}">
            <span>${Icon.wifi()} ${n.ssid}</span>
            <span class="rssi">${n.rssi ?? ""} dBm</span>
          </div>`).join("");
        list.querySelectorAll(".wifi-item[data-ssid]").forEach((item) => {
          item.addEventListener("click", () => {
            form.querySelector(`#${lastSsidFocus}`).value = item.dataset.ssid;
          });
        });
      } catch {
        list.innerHTML = `<div class="wifi-item">${i18n.t("common.error")}</div>`;
      }
    });

    form.querySelector("#save-btn").addEventListener("click", async () => {
      const body = {
        ssid1: readVal(form, "ssid1"),
        pass1: readVal(form, "pass1"),
        ssid2: readVal(form, "ssid2"),
        pass2: readVal(form, "pass2"),
        hostname: readVal(form, "hostname"),
        dhcp: readVal(form, "dhcp"),
        ip: readVal(form, "ip"),
        gateway: readVal(form, "gateway"),
        subnet: readVal(form, "subnet"),
        dns1: readVal(form, "dns1"),
        dns2: readVal(form, "dns2"),
      };
      await handleSave(() => api.postNetworkConfig(body), i18n);
    });
  }

  draw();
  const off = i18n.onChange(() => draw());
  return () => off();
}
