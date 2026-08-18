// FreeDS — minimal i18n engine. No dependencies.
// Dictionaries live in /i18n/<lang>.json as nested objects; keys are
// referenced with dot notation, e.g. "dashboard.solar_power".

const SUPPORTED = ["pt", "en"];
const STORAGE_KEY = "freeds.lang";

let currentLang = "pt";
let dict = {};
const listeners = new Set();

function get(obj, path) {
  return path.split(".").reduce((acc, part) => (acc && typeof acc === "object" ? acc[part] : undefined), obj);
}

export const I18n = {
  get lang() { return currentLang; },
  supported: SUPPORTED,

  async init(preferredLang) {
    const stored = localStorage.getItem(STORAGE_KEY);
    const lang = SUPPORTED.includes(stored) ? stored
      : SUPPORTED.includes(preferredLang) ? preferredLang
      : "pt";
    await this.setLang(lang, { persist: false });
  },

  async setLang(lang, { persist = true } = {}) {
    if (!SUPPORTED.includes(lang)) lang = "pt";
    const res = await fetch(`i18n/${lang}.json`);
    dict = await res.json();
    currentLang = lang;
    if (persist) localStorage.setItem(STORAGE_KEY, lang);
    document.documentElement.setAttribute("lang", lang);
    this.applyTo(document.body);
    listeners.forEach((cb) => cb(lang));
  },

  onChange(cb) {
    listeners.add(cb);
    return () => listeners.delete(cb);
  },

  t(key, vars) {
    const val = get(dict, key);
    let text = typeof val === "string" ? val : key;
    if (vars) {
      Object.keys(vars).forEach((k) => {
        text = text.replace(new RegExp(`{{${k}}}`, "g"), vars[k]);
      });
    }
    return text;
  },

  // Walks a DOM subtree applying data-i18n / data-i18n-* attributes.
  // Call after inserting new markup that used those attributes.
  applyTo(root) {
    root.querySelectorAll("[data-i18n]").forEach((el) => {
      el.textContent = this.t(el.getAttribute("data-i18n"));
    });
    root.querySelectorAll("[data-i18n-placeholder]").forEach((el) => {
      el.setAttribute("placeholder", this.t(el.getAttribute("data-i18n-placeholder")));
    });
    root.querySelectorAll("[data-i18n-title]").forEach((el) => {
      el.setAttribute("title", this.t(el.getAttribute("data-i18n-title")));
    });
    root.querySelectorAll("[data-i18n-aria-label]").forEach((el) => {
      el.setAttribute("aria-label", this.t(el.getAttribute("data-i18n-aria-label")));
    });
  },
};
