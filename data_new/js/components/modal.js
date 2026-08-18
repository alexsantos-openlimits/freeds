// FreeDS — minimal confirmation modal, no dependencies.
import { Icon } from "./icons.js";
import { I18n } from "../i18n.js";

export function confirmModal({ title, text, confirmLabel, cancelLabel, danger = true }) {
  return new Promise((resolve) => {
    const backdrop = document.createElement("div");
    backdrop.className = "modal-backdrop";
    backdrop.innerHTML = `
      <div class="modal" role="dialog" aria-modal="true">
        <h3>${Icon.alertTriangle()}<span></span></h3>
        <p></p>
        <div class="btn-row">
          <button type="button" class="btn btn-secondary" data-act="cancel"></button>
          <button type="button" class="btn ${danger ? "btn-danger" : "btn-primary"}" data-act="confirm"></button>
        </div>
      </div>`;
    backdrop.querySelector("h3 span").textContent = title;
    backdrop.querySelector("p").textContent = text;
    backdrop.querySelector('[data-act="cancel"]').textContent = cancelLabel || I18n.t("common.cancel");
    backdrop.querySelector('[data-act="confirm"]').textContent = confirmLabel || I18n.t("common.confirm");

    function close(result) {
      backdrop.remove();
      resolve(result);
    }
    backdrop.addEventListener("click", (e) => {
      if (e.target === backdrop) close(false);
    });
    backdrop.querySelector('[data-act="cancel"]').addEventListener("click", () => close(false));
    backdrop.querySelector('[data-act="confirm"]').addEventListener("click", () => close(true));
    document.body.appendChild(backdrop);
  });
}
