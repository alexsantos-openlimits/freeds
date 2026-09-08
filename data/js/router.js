// Lusol — tiny hash router. No dependencies, no history API needed
// since the device serves a single static page.

export function createRouter(routes, { defaultPath = "dashboard", onChange } = {}) {
  let cleanup = null;

  function currentPath() {
    const raw = (location.hash || "").replace(/^#\/?/, "");
    return raw || defaultPath;
  }

  function resolve(path) {
    if (routes[path]) return { path, route: routes[path] };
    // fall back to the first matching parent section, e.g. "settings" -> "settings/network"
    const parent = Object.keys(routes).find((p) => p.startsWith(`${path}/`));
    if (parent) return { path: parent, route: routes[parent] };
    return { path: defaultPath, route: routes[defaultPath] };
  }

  async function render() {
    const requested = currentPath();
    const { path, route } = resolve(requested);
    if (path !== requested) {
      location.hash = `#/${path}`;
      return; // hashchange will re-trigger render()
    }
    if (typeof cleanup === "function") {
      try { cleanup(); } catch (e) { /* ignore */ }
    }
    cleanup = null;
    const container = document.getElementById("view-root");
    container.innerHTML = "";
    if (onChange) onChange(path);
    const result = await route(container);
    if (typeof result === "function") cleanup = result;
  }

  window.addEventListener("hashchange", render);

  return {
    start() { render(); },
    navigate(path) {
      if (currentPath() === path) { render(); return; }
      location.hash = `#/${path}`;
    },
    refresh: render,
  };
}
