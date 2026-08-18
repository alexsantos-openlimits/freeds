import { Icon } from "../components/icons.js";

const MAX_LINES = 800;

export default async function mount(container, ctx) {
  const { i18n } = ctx;
  ctx.setTitle("log.title");

  container.innerHTML = `
    <div class="view-header">
      <h2 data-i18n="log.title"></h2>
      <p data-i18n="log.subtitle"></p>
    </div>
    <div class="card">
      <div class="log-toolbar">
        <button type="button" class="btn btn-secondary" id="clear-btn">${Icon.close()} <span data-i18n="log.clear"></span></button>
        <label class="switch">
          <input type="checkbox" id="autoscroll" checked>
          <span class="track"></span>
          <span class="switch-label" data-i18n="log.autoscroll"></span>
        </label>
        <span class="topbar-spacer"></span>
        <span class="status-pill" id="log-status"><span class="dot"></span><span data-role="text"></span></span>
      </div>
      <div class="log-console" id="log-console"></div>
    </div>
  `;
  i18n.applyTo(container);

  const consoleEl = container.querySelector("#log-console");
  const autoscroll = container.querySelector("#autoscroll");
  const statusPill = container.querySelector("#log-status");

  function setConnected(ok) {
    statusPill.classList.toggle("ok", ok);
    statusPill.classList.toggle("bad", !ok);
    statusPill.querySelector("[data-role='text']").textContent = ok ? i18n.t("log.connected") : i18n.t("log.disconnected");
  }
  setConnected(ctx.isEventStreamOpen());

  function appendLine(text) {
    const p = document.createElement("p");
    p.className = "line";
    p.textContent = text;
    consoleEl.appendChild(p);
    while (consoleEl.children.length > MAX_LINES) consoleEl.removeChild(consoleEl.firstChild);
    if (autoscroll.checked) consoleEl.scrollTop = consoleEl.scrollHeight;
  }

  container.querySelector("#clear-btn").addEventListener("click", () => {
    consoleEl.innerHTML = "";
  });

  const unsubLog = ctx.onWeblog(appendLine);
  const unsubConn = ctx.onConnectionChange(setConnected);

  return () => {
    unsubLog();
    unsubConn();
  };
}
