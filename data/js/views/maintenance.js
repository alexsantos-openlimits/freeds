import { Icon } from "../components/icons.js";
import { toast } from "../components/toast.js";
import { confirmModal } from "../components/modal.js";

export default async function mount(container, ctx) {
  const { i18n, api } = ctx;
  ctx.setTitle("maintenance.title");

  container.innerHTML = `
    <div class="view-header">
      <h2 data-i18n="maintenance.title"></h2>
      <p data-i18n="maintenance.subtitle"></p>
    </div>

    <div class="card">
      <div class="card-title">${Icon.download()} <span data-i18n="maintenance.backup_section"></span></div>
      <p class="text-muted" data-i18n="maintenance.backup_desc"></p>
      <div class="btn-row">
        <button type="button" class="btn btn-secondary" id="backup-btn">${Icon.download()} <span data-i18n="maintenance.download_backup"></span></button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">${Icon.upload()} <span data-i18n="maintenance.restore_section"></span></div>
      <p class="text-muted" data-i18n="maintenance.restore_desc"></p>
      <div class="input-group" style="max-width:420px;">
        <input type="file" id="restore-file" accept="application/json,.json">
      </div>
      <div class="btn-row">
        <button type="button" class="btn btn-danger-outline" id="restore-btn" disabled>${Icon.upload()} <span data-i18n="maintenance.restore_action"></span></button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">${Icon.cpu()} <span data-i18n="maintenance.firmware_section"></span></div>
      <p class="text-muted" data-i18n="maintenance.firmware_desc"></p>
      <div class="input-group" style="max-width:420px;">
        <input type="file" id="firmware-file" accept=".bin">
      </div>
      <div class="progress-bar-outer hidden" id="firmware-progress-outer" style="margin-top:12px; max-width:420px;">
        <div class="progress-bar-inner" id="firmware-progress-inner"></div>
      </div>
      <p class="text-muted hidden" id="firmware-uploading-note" data-i18n="maintenance.uploading" style="margin-top:8px;"></p>
      <div class="btn-row">
        <button type="button" class="btn btn-primary" id="firmware-btn" disabled>${Icon.upload()} <span data-i18n="maintenance.start_update"></span></button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">${Icon.power()} <span data-i18n="maintenance.reboot_section"></span></div>
      <p class="text-muted" data-i18n="maintenance.reboot_desc"></p>
      <div class="btn-row">
        <button type="button" class="btn btn-secondary" id="reboot-btn">${Icon.power()} <span data-i18n="maintenance.reboot_action"></span></button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">${Icon.rotateCcw()} <span data-i18n="maintenance.factory_section"></span></div>
      <p class="text-muted" data-i18n="maintenance.factory_desc"></p>
      <div class="btn-row">
        <button type="button" class="btn btn-danger" id="factory-btn">${Icon.rotateCcw()} <span data-i18n="maintenance.factory_action"></span></button>
      </div>
    </div>
  `;
  i18n.applyTo(container);

  // Backup
  container.querySelector("#backup-btn").addEventListener("click", async () => {
    try {
      const data = await api.getBackup();
      const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `freeds-backup-${new Date().toISOString().slice(0, 10)}.json`;
      document.body.appendChild(a);
      a.click();
      a.remove();
      URL.revokeObjectURL(url);
    } catch {
      toast(i18n.t("common.error"), "error");
    }
  });

  // Restore
  const restoreFile = container.querySelector("#restore-file");
  const restoreBtn = container.querySelector("#restore-btn");
  restoreFile.addEventListener("change", () => {
    restoreBtn.disabled = !restoreFile.files.length;
  });
  restoreBtn.addEventListener("click", async () => {
    const file = restoreFile.files[0];
    if (!file) return;
    const ok = await confirmModal({
      title: i18n.t("maintenance.restore_confirm_title"),
      text: i18n.t("maintenance.restore_confirm_text"),
      confirmLabel: i18n.t("maintenance.restore_action"),
    });
    if (!ok) return;
    try {
      const text = await file.text();
      const json = JSON.parse(text);
      await api.restore(json);
      toast(i18n.t("maintenance.update_success"), "success");
    } catch (e) {
      toast(e instanceof SyntaxError ? i18n.t("errors.invalid_file") : i18n.t("errors.save_failed"), "error");
    }
  });

  // Firmware update
  const fwFile = container.querySelector("#firmware-file");
  const fwBtn = container.querySelector("#firmware-btn");
  const fwProgressOuter = container.querySelector("#firmware-progress-outer");
  const fwProgressInner = container.querySelector("#firmware-progress-inner");
  const fwNote = container.querySelector("#firmware-uploading-note");
  fwFile.addEventListener("change", () => {
    fwBtn.disabled = !fwFile.files.length;
  });
  fwBtn.addEventListener("click", async () => {
    const file = fwFile.files[0];
    if (!file) return;
    fwBtn.disabled = true;
    fwFile.disabled = true;
    fwProgressOuter.classList.remove("hidden");
    fwNote.classList.remove("hidden");
    fwProgressInner.style.width = "0%";
    try {
      await api.uploadFirmware(file, (pct) => { fwProgressInner.style.width = `${pct}%`; });
      toast(i18n.t("maintenance.update_success"), "success");
    } catch {
      toast(i18n.t("maintenance.update_error"), "error");
    } finally {
      fwNote.classList.add("hidden");
      fwBtn.disabled = false;
      fwFile.disabled = false;
    }
  });

  // Reboot
  container.querySelector("#reboot-btn").addEventListener("click", async () => {
    const ok = await confirmModal({
      title: i18n.t("maintenance.reboot_confirm_title"),
      text: i18n.t("maintenance.reboot_confirm_text"),
      confirmLabel: i18n.t("maintenance.reboot_action"),
      danger: false,
    });
    if (!ok) return;
    try {
      await api.reboot();
    } catch { /* device is rebooting; connection drop is expected */ }
    toast(i18n.t("maintenance.reboot_action"), "success");
  });

  // Factory reset
  container.querySelector("#factory-btn").addEventListener("click", async () => {
    const ok = await confirmModal({
      title: i18n.t("maintenance.factory_confirm_title"),
      text: i18n.t("maintenance.factory_confirm_text"),
      confirmLabel: i18n.t("maintenance.factory_action"),
    });
    if (!ok) return;
    try {
      await api.factoryReset();
      toast(i18n.t("maintenance.update_success"), "success");
    } catch {
      toast(i18n.t("errors.save_failed"), "error");
    }
  });
}
