// FreeDS — thin wrapper around the device REST API.
// No build step, no dependencies: plain fetch() against same-origin endpoints.

async function request(path, options = {}) {
  const opts = Object.assign({ headers: {} }, options);
  if (opts.body && typeof opts.body !== "string" && !(opts.body instanceof FormData)) {
    opts.body = JSON.stringify(opts.body);
    opts.headers["Content-Type"] = "application/json";
  }
  let res;
  try {
    res = await fetch(path, opts);
  } catch (err) {
    throw new ApiError(path, 0, "network");
  }
  if (!res.ok) {
    throw new ApiError(path, res.status, await safeText(res));
  }
  const ct = res.headers.get("content-type") || "";
  if (ct.includes("application/json")) {
    return res.json();
  }
  return res.text();
}

async function safeText(res) {
  try { return await res.text(); } catch { return ""; }
}

export class ApiError extends Error {
  constructor(path, status, detail) {
    super(`Request to ${path} failed (${status})`);
    this.path = path;
    this.status = status;
    this.detail = detail;
  }
}

export const Api = {
  getStatus() {
    return request("/api/status");
  },
  getConfig() {
    return request("/api/config");
  },
  getBackup() {
    return request("/api/backup");
  },
  getTemperatureSensors() {
    // Assumed endpoint — see project notes. Falls back to an empty list
    // if the firmware does not implement it yet, so the UI degrades gracefully.
    return request("/api/temperature/sensors").catch(() => ({ sensors: [] }));
  },
  scanWifi() {
    return request("/api/wifi/scan", { method: "POST" });
  },
  postControl(body) {
    return request("/api/control", { method: "POST", body });
  },
  postNetworkConfig(body) {
    return request("/api/config/network", { method: "POST", body });
  },
  postMqttConfig(body) {
    return request("/api/config/mqtt", { method: "POST", body });
  },
  postSurplusConfig(body) {
    return request("/api/config/surplus-manager", { method: "POST", body });
  },
  postLoadConfig(body) {
    return request("/api/config/load", { method: "POST", body });
  },
  postTemperatureConfig(body) {
    return request("/api/config/temperature", { method: "POST", body });
  },
  postSystemConfig(body) {
    return request("/api/config/system", { method: "POST", body });
  },
  restore(configJson) {
    return request("/api/restore", { method: "POST", body: configJson });
  },
  factoryReset() {
    return request("/api/factory-reset", { method: "POST" });
  },
  reboot() {
    // No explicit endpoint was specified for reboot-without-reset; the legacy
    // UI used GET /reboot, kept here for parity.
    return request("/reboot", { method: "GET" }).catch(() => request("/reboot"));
  },
  eventsUrl() {
    return "/events";
  },
  uploadFirmware(file, onProgress) {
    return uploadMultipart("/update", "update", file, onProgress);
  },
};

function uploadMultipart(url, fieldName, file, onProgress) {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const form = new FormData();
    form.append(fieldName, file);
    xhr.open("POST", url, true);
    xhr.upload.onprogress = (e) => {
      if (onProgress && e.lengthComputable) {
        onProgress(Math.round((e.loaded / e.total) * 100));
      }
    };
    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) resolve(xhr.responseText);
      else reject(new ApiError(url, xhr.status, xhr.responseText));
    };
    xhr.onerror = () => reject(new ApiError(url, 0, "network"));
    xhr.send(form);
  });
}
