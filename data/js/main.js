import { Api } from "./api.js";
import { I18n } from "./i18n.js";
import { createRouter } from "./router.js";
import { Icon } from "./components/icons.js";
import { fmtUptime, sourceStatusLabel } from "./components/dom.js";
import { toast } from "./components/toast.js";

import mountDashboard from "./views/dashboard.js";
import mountNetwork from "./views/network.js";
import mountMqtt from "./views/mqtt.js";
import mountSurplus from "./views/surplus.js";
import mountLoad from "./views/load.js";
import mountTemperature from "./views/temperature.js";
import mountSystem from "./views/system.js";
import mountLog from "./views/log.js";
import mountMaintenance from "./views/maintenance.js";

const NAV = [
  { path: "dashboard", label: "nav.dashboard", icon: "home" },
  {
    path: "settings",
    label: "nav.settings",
    icon: "sliders",
    children: [
      { path: "settings/network", label: "nav.network" },
      { path: "settings/mqtt", label: "nav.mqtt" },
      { path: "settings/surplus", label: "nav.surplus" },
      { path: "settings/load", label: "nav.load" },
      { path: "settings/temperature", label: "nav.temperature" },
      { path: "settings/system", label: "nav.general" },
    ],
  },
  { path: "log", label: "nav.log", icon: "terminal" },
  { path: "maintenance", label: "nav.system", icon: "tool" },
];

const VIEWS = {
  "dashboard": mountDashboard,
  "settings/network": mountNetwork,
  "settings/mqtt": mountMqtt,
  "settings/surplus": mountSurplus,
  "settings/load": mountLoad,
  "settings/temperature": mountTemperature,
  "settings/system": mountSystem,
  "log": mountLog,
  "maintenance": mountMaintenance,
};

async function boot() {
  const sidebarNav = document.getElementById("sidebar-nav");
  const sidebar = document.getElementById("sidebar");
  const overlay = document.getElementById("sidebar-overlay");
  const hamburger = document.getElementById("hamburger");
  const pageTitle = document.getElementById("page-title");
  const wifiPill = document.getElementById("wifi-pill");
  const mqttPill = document.getElementById("mqtt-pill");
  const sourcePill = document.getElementById("source-pill");
  const uptimeFooter = document.getElementById("uptime-footer");
  const langSwitch = document.getElementById("lang-switch");

  // ---- live data plumbing (SSE) ----
  let lastStatus = null;
  const statusListeners = new Set();
  const weblogListeners = new Set();
  const connectionListeners = new Set();
  let eventStreamOpen = false;

  function setConnectionState(open) {
    eventStreamOpen = open;
    connectionListeners.forEach((cb) => cb(open));
  }

  function connectEvents() {
    let es;
    try {
      es = new EventSource(Api.eventsUrl());
    } catch {
      return;
    }
    es.addEventListener("open", () => setConnectionState(true));
    es.addEventListener("error", () => setConnectionState(false));
    es.addEventListener("status", (e) => {
      try {
        lastStatus = JSON.parse(e.data);
      } catch {
        return;
      }
      setConnectionState(true);
      statusListeners.forEach((cb) => cb(lastStatus));
      updateGlobalChrome(lastStatus);
    });
    es.addEventListener("weblog", (e) => {
      weblogListeners.forEach((cb) => cb(e.data));
    });
  }

  function updateGlobalChrome(status) {
    const wifi = status.wifi || {};
    wifiPill.classList.toggle("ok", !!wifi.connected);
    wifiPill.classList.toggle("bad", !wifi.connected);
    wifiPill.querySelector("[data-role='text']").textContent = wifi.connected
      ? `${wifi.ssid || "Wi-Fi"} · ${wifi.rssi ?? "—"} dBm`
      : I18n.t("dashboard.wifi_disconnected");

    const sourceOk = status.sourceConnected && !status.dataFault;
    sourcePill.classList.toggle("ok", !!sourceOk);
    sourcePill.classList.toggle("bad", !sourceOk);
    sourcePill.querySelector("[data-role='text']").textContent = sourceStatusLabel(status, I18n);

    const mqtt = status.mqtt || {};
    mqttPill.classList.toggle("ok", !!mqtt.connected);
    mqttPill.classList.toggle("bad", !!mqtt.enabled && !mqtt.connected);
    mqttPill.querySelector("[data-role='text']").textContent = !mqtt.enabled
      ? I18n.t("dashboard.mqtt_disabled")
      : mqtt.connected
        ? I18n.t("dashboard.mqtt_connected")
        : I18n.t("dashboard.mqtt_fault");

    uptimeFooter.textContent = `${I18n.t("dashboard.uptime")}: ${fmtUptime(status.uptimeSeconds)}`;
  }

  // ---- sidebar ----
  const collapsedGroups = new Set();

  function isActive(path, current) {
    return current === path;
  }
  function isGroupActive(groupPath, current) {
    return current.startsWith(`${groupPath}/`);
  }

  function renderNav(current) {
    sidebarNav.innerHTML = NAV.map((item) => {
      if (item.children) {
        const groupActive = isGroupActive(item.path, current);
        const open = groupActive && !collapsedGroups.has(item.path);
        return `
          <div class="nav-group">
            <div class="nav-item ${groupActive ? "active expanded" : ""}" data-path="${item.path}" data-group="1">
              ${Icon[item.icon]()}<span>${I18n.t(item.label)}</span>
              <span class="chevron">${Icon.chevronRight()}</span>
            </div>
            <div class="nav-subitems ${open ? "open" : ""}">
              ${item.children.map((c) => `<div class="nav-subitem ${isActive(c.path, current) ? "active" : ""}" data-path="${c.path}">${I18n.t(c.label)}</div>`).join("")}
            </div>
          </div>`;
      }
      return `
        <div class="nav-group">
          <div class="nav-item ${isActive(item.path, current) ? "active" : ""}" data-path="${item.path}">
            ${Icon[item.icon]()}<span>${I18n.t(item.label)}</span>
          </div>
        </div>`;
    }).join("");

    sidebarNav.querySelectorAll("[data-path]").forEach((node) => {
      node.addEventListener("click", () => {
        const path = node.dataset.path;
        if (node.dataset.group && isGroupActive(path, current)) {
          if (collapsedGroups.has(path)) collapsedGroups.delete(path);
          else collapsedGroups.add(path);
          renderNav(current);
          return;
        }
        router.navigate(path);
        closeMobileSidebar();
      });
    });
  }

  function closeMobileSidebar() {
    sidebar.classList.remove("open");
    overlay.classList.remove("open");
  }

  hamburger.addEventListener("click", () => {
    sidebar.classList.toggle("open");
    overlay.classList.toggle("open");
  });
  overlay.addEventListener("click", closeMobileSidebar);

  // ---- language switch ----
  function renderLangSwitch() {
    langSwitch.querySelectorAll("button").forEach((btn) => {
      btn.classList.toggle("active", btn.dataset.lang === I18n.lang);
    });
  }
  langSwitch.querySelectorAll("button").forEach((btn) => {
    btn.addEventListener("click", () => I18n.setLang(btn.dataset.lang));
  });

  // ---- title ----
  let currentTitleKey = "dashboard.title";
  function setTitle(key) {
    currentTitleKey = key;
    pageTitle.textContent = I18n.t(key);
  }

  // ---- router + context passed to views ----
  const ctx = {
    api: Api,
    i18n: I18n,
    toast,
    navigate: (path) => router.navigate(path),
    setTitle,
    getStatus: () => lastStatus,
    onStatus: (cb) => { statusListeners.add(cb); return () => statusListeners.delete(cb); },
    onWeblog: (cb) => { weblogListeners.add(cb); return () => weblogListeners.delete(cb); },
    onConnectionChange: (cb) => { connectionListeners.add(cb); return () => connectionListeners.delete(cb); },
    isEventStreamOpen: () => eventStreamOpen,
  };

  // Cada vista espera (container, ctx); o router só passa o container, por
  // isso o ctx é fechado aqui via closure em vez de ser esquecido.
  const ROUTES = Object.fromEntries(
    Object.entries(VIEWS).map(([path, mount]) => [path, (container) => mount(container, ctx)])
  );

  const router = createRouter(ROUTES, {
    defaultPath: "dashboard",
    onChange: (path) => {
      renderNav(path);
      window.scrollTo(0, 0);
    },
  });

  I18n.onChange(() => {
    renderLangSwitch();
    pageTitle.textContent = I18n.t(currentTitleKey);
    renderNav((location.hash || "").replace(/^#\/?/, "") || "dashboard");
    if (lastStatus) updateGlobalChrome(lastStatus);
  });

  // ---- boot sequence ----
  let initialLang = "pt";
  try {
    const cfg = await Api.getConfig();
    initialLang = (cfg.system && cfg.system.language) || "pt";
  } catch {
    toast(I18n.t("errors.fetch_config"), "error");
  }
  await I18n.init(initialLang);
  renderLangSwitch();

  try {
    lastStatus = await Api.getStatus();
    updateGlobalChrome(lastStatus);
  } catch {
    // dashboard will show placeholders until the SSE stream delivers data
  }

  router.start();
  connectEvents();
}

boot();
