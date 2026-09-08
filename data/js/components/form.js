// Lusol — tiny helpers to reduce boilerplate in settings forms.
import { toast } from "./toast.js";

export function section(titleKey, i18n, bodyHtml) {
  return `
    <div class="form-section">
      <div class="form-section-title">${i18n.t(titleKey)}</div>
      ${bodyHtml}
    </div>`;
}

export function row({ labelKey, i18n, controlHtml, hintKey = null }) {
  return `
    <div class="form-row">
      <label class="field-label">${i18n.t(labelKey)}</label>
      <div>
        <div class="field-control">${controlHtml}</div>
        ${hintKey ? `<div class="field-hint">${i18n.t(hintKey)}</div>` : ""}
      </div>
    </div>`;
}

export function textInput({ id, value = "", type = "text", maxlength = null, placeholder = "" }) {
  return `<input type="${type}" id="${id}" value="${value ?? ""}" ${maxlength ? `maxlength="${maxlength}"` : ""} placeholder="${placeholder}">`;
}

export function numberInput({ id, value = 0, min = null, max = null, step = null }) {
  return `<input type="number" id="${id}" value="${value ?? 0}" ${min !== null ? `min="${min}"` : ""} ${max !== null ? `max="${max}"` : ""} ${step !== null ? `step="${step}"` : ""}>`;
}

export function timeInput({ id, value }) {
  return `<input type="time" id="${id}" value="${value}">`;
}

export function switchInput({ id, checked = false, labelKey, i18n }) {
  return `
    <label class="switch">
      <input type="checkbox" id="${id}" ${checked ? "checked" : ""}>
      <span class="track"></span>
      <span class="switch-label">${labelKey ? i18n.t(labelKey) : ""}</span>
    </label>`;
}

export function selectInput({ id, options, value }) {
  const opts = options.map(([v, label]) => `<option value="${v}" ${String(v) === String(value) ? "selected" : ""}>${label}</option>`).join("");
  return `<select id="${id}">${opts}</select>`;
}

export function readVal(root, id) {
  const el = root.querySelector(`#${id}`);
  if (!el) return undefined;
  if (el.type === "checkbox") return el.checked;
  if (el.type === "number") return el.value === "" ? 0 : Number(el.value);
  return el.value;
}

export async function handleSave(fn, i18n) {
  try {
    await fn();
    toast(i18n.t("common.saved"), "success");
  } catch (e) {
    toast(i18n.t("common.save_error"), "error");
    throw e;
  }
}
