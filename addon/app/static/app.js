// Relative fetch paths only ("api/devices/...", not "/api/devices/...")
// -- HA ingress proxies this page under a dynamic path prefix, and an
// absolute path would bypass that prefix and 404.

const form = document.getElementById("config-form");
const thresholdInput = form.elements["threshold"];
const thresholdLabel = document.getElementById("threshold-value");
const previewImage = document.getElementById("preview-image");
const previewLoading = document.getElementById("preview-loading");
const actionError = document.getElementById("action-error");
const statusBadge = document.getElementById("status-badge");
const statusText = document.getElementById("status-text");
const deviceSelect = document.getElementById("device-select");
const addDeviceRow = document.getElementById("add-device-row");
const newDeviceNameInput = document.getElementById("new-device-name");
const invertCheckbox = form.elements["invert"];
const invertToggleBtn = document.getElementById("btn-invert-toggle");
const invertStateLabel = document.getElementById("invert-state");
const fullPageCheckbox = form.elements["capture_full_page"];
const fullPageToggleBtn = document.getElementById("btn-fullpage-toggle");
const fullPageStateLabel = document.getElementById("fullpage-state");

const buttons = {
  preview: document.getElementById("btn-preview"),
  save: document.getElementById("btn-save"),
  refresh: document.getElementById("btn-refresh"),
  addDevice: document.getElementById("btn-add-device"),
  deleteDevice: document.getElementById("btn-delete-device"),
  confirmAdd: document.getElementById("btn-confirm-add"),
  cancelAdd: document.getElementById("btn-cancel-add"),
  invertToggle: invertToggleBtn,
  fullPageToggle: fullPageToggleBtn,
};

let currentDevice = null; // name of the device currently shown in the form, or null while adding a new one

thresholdInput.addEventListener("input", () => {
  thresholdLabel.textContent = thresholdInput.value;
});

function setInvertState(on) {
  invertCheckbox.checked = on;
  invertToggleBtn.setAttribute("aria-pressed", String(on));
  invertToggleBtn.classList.toggle("toggle-btn-on", on);
  invertStateLabel.textContent = on ? "on" : "off";
}
invertToggleBtn.addEventListener("click", () => setInvertState(!invertCheckbox.checked));

function setFullPageState(on) {
  fullPageCheckbox.checked = on;
  fullPageToggleBtn.setAttribute("aria-pressed", String(on));
  fullPageToggleBtn.classList.toggle("toggle-btn-on", on);
  fullPageStateLabel.textContent = on ? "on" : "off";
}
fullPageToggleBtn.addEventListener("click", () => setFullPageState(!fullPageCheckbox.checked));

function setBusy(busy) {
  for (const b of Object.values(buttons)) b.disabled = busy;
}

// severity "error" (default): red, for an action that actually failed.
// "warning": amber, for a degraded-but-not-failed outcome (e.g. saved
// and active in memory, but not persisted to disk) -- looks identical
// to a hard failure otherwise, which reads as "my save didn't work"
// when it actually did, just not durably. Stays visible (no auto-hide
// timer) until the next action calls this again -- see doSave/doPreview/
// doRefresh, each of which clears it with showError(null) on start.
function showError(msg, severity = "error") {
  if (!msg) {
    actionError.hidden = true;
    actionError.textContent = "";
    return;
  }
  actionError.hidden = false;
  actionError.textContent = msg;
  actionError.classList.toggle("warning", severity === "warning");
}

function formValues() {
  const fd = new FormData(form);
  return {
    dashboard_url: fd.get("dashboard_url"),
    capture_width: parseInt(fd.get("capture_width"), 10),
    capture_height: parseInt(fd.get("capture_height"), 10),
    capture_full_page: fd.has("capture_full_page"),
    fit_mode: fd.get("fit_mode"),
    color_mode: fd.get("color_mode"),
    threshold: parseInt(fd.get("threshold"), 10),
    invert: fd.has("invert"),
    wait_seconds: parseInt(fd.get("wait_seconds"), 10),
    interval_minutes: parseInt(fd.get("interval_minutes"), 10),
    panel_width: parseInt(fd.get("panel_width"), 10),
    panel_height: parseInt(fd.get("panel_height"), 10),
  };
}

function populateForm(opts) {
  form.elements["dashboard_url"].value = opts.dashboard_url ?? "";
  form.elements["capture_width"].value = opts.capture_width ?? 800;
  form.elements["capture_height"].value = opts.capture_height ?? 480;
  setFullPageState(!!opts.capture_full_page);
  form.elements["fit_mode"].value = opts.fit_mode ?? "letterbox";
  form.elements["color_mode"].value = opts.color_mode ?? "bw";
  form.elements["threshold"].value = opts.threshold ?? 160;
  thresholdLabel.textContent = form.elements["threshold"].value;
  setInvertState(!!opts.invert);
  form.elements["wait_seconds"].value = opts.wait_seconds ?? 3;
  form.elements["interval_minutes"].value = opts.interval_minutes ?? 10;
  form.elements["panel_width"].value = opts.panel_width ?? 800;
  form.elements["panel_height"].value = opts.panel_height ?? 480;
}

async function loadDeviceList() {
  const r = await fetch("api/devices");
  if (!r.ok) return;
  const data = await r.json();

  deviceSelect.innerHTML = "";
  for (const name of data.names) {
    const opt = document.createElement("option");
    opt.value = name;
    opt.textContent = name;
    deviceSelect.appendChild(opt);
  }
  buttons.deleteDevice.hidden = data.names.length <= 1;

  if (!currentDevice || !data.names.includes(currentDevice)) {
    currentDevice = data.names[0] ?? null;
  }
  deviceSelect.value = currentDevice;
}

async function selectDevice(name) {
  currentDevice = name;
  addDeviceRow.hidden = true;
  showError(null);
  const r = await fetch(`api/devices/${encodeURIComponent(name)}`);
  if (r.ok) populateForm(await r.json());
  loadStatus();
  loadLivePreview();
}

async function loadLivePreview() {
  if (!currentDevice) return;
  // Cache-busted: the browser would otherwise happily keep showing a
  // stale image for a same-named URL after a refresh.
  const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}/live-preview?t=${Date.now()}`);
  if (r.ok) {
    const blob = await r.blob();
    previewImage.src = URL.createObjectURL(blob);
  } else {
    previewImage.removeAttribute("src");
  }
}

function fmtRelativeTime(iso) {
  if (!iso) return "never";
  const then = new Date(iso).getTime();
  const diffMin = Math.round((Date.now() - then) / 60000);
  if (diffMin < 1) return "just now";
  if (diffMin === 1) return "1 minute ago";
  if (diffMin < 60) return `${diffMin} minutes ago`;
  const diffHr = Math.round(diffMin / 60);
  return `${diffHr} hour${diffHr === 1 ? "" : "s"} ago`;
}

async function loadStatus() {
  if (!currentDevice) return;
  const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}/status`);
  if (r.status === 404) {
    // The selected device no longer exists -- e.g. renamed or deleted in
    // another tab/session since this page loaded. Recover instead of
    // silently polling a 404 forever (which looks exactly like "nothing
    // happens" when you click Preview/Refresh too).
    showError(`Device "${currentDevice}" no longer exists -- switching to an available one.`);
    currentDevice = null;
    await loadDeviceList();
    if (currentDevice) await selectDevice(currentDevice);
    return;
  }
  if (!r.ok) return;
  const s = await r.json();

  if (s.last_run_ok === null) {
    statusBadge.className = "badge badge-unknown";
    statusBadge.textContent = "—";
    statusText.textContent = "No run yet";
  } else if (s.last_run_ok) {
    statusBadge.className = "badge badge-ok";
    statusBadge.textContent = "OK";
    statusText.textContent = `Last successful run: ${fmtRelativeTime(s.last_run_at)}`;
  } else {
    statusBadge.className = "badge badge-fail";
    statusBadge.textContent = "FAILED";
    statusText.textContent = `Last run failed ${fmtRelativeTime(s.last_run_at)}: ${s.last_run_error ?? "unknown error"}`;
  }
  if (s.next_run_at) {
    const t = new Date(s.next_run_at);
    statusText.textContent += ` · next scheduled run at ${t.toLocaleTimeString([], {hour: "2-digit", minute: "2-digit"})}`;
  }
}

async function doPreview() {
  if (!currentDevice) return;
  showError(null);
  setBusy(true);
  previewLoading.hidden = false;
  try {
    const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}/preview`, {
      method: "POST",
      headers: {"Content-Type": "application/json"},
      body: JSON.stringify(formValues()),
    });
    if (!r.ok) {
      const body = await r.json().catch(() => ({}));
      throw new Error(body.error || `HTTP ${r.status}`);
    }
    const blob = await r.blob();
    previewImage.src = URL.createObjectURL(blob);
  } catch (e) {
    showError(`Preview failed: ${e.message}`);
  } finally {
    previewLoading.hidden = true;
    setBusy(false);
  }
}

// Raw save request -- no busy/error UI handling, callers own that, since
// doRefresh() needs to run this as one step of a longer operation rather
// than as a standalone user action.
async function saveCurrentDevice() {
  const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}`, {
    method: "POST",
    headers: {"Content-Type": "application/json"},
    body: JSON.stringify(formValues()),
  });
  const body = await r.json();
  if (!r.ok || !body.ok) throw new Error(body.error || `HTTP ${r.status}`);
  if (body.created) await loadDeviceList();
  return body;
}

async function doSave() {
  if (!currentDevice) return;
  showError(null);
  setBusy(true);
  try {
    const body = await saveCurrentDevice();
    if (body.persisted === false) {
      showError(`Saved and active now, but not persisted to disk: ${body.warning}`, "warning");
    }
  } catch (e) {
    showError(`Save failed: ${e.message}`);
  } finally {
    setBusy(false);
    loadStatus();
  }
}

async function doRefresh() {
  if (!currentDevice) return;
  showError(null);
  setBusy(true);
  previewLoading.hidden = false;
  try {
    // Save first -- otherwise "Refresh now" would silently push whatever
    // was last *saved*, ignoring any settings changed in the form since
    // then, which reads as "my changes didn't do anything" exactly like
    // the confusion Preview caused before.
    await saveCurrentDevice();
    const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}/refresh`, {method: "POST"});
    const body = await r.json();
    if (!r.ok || !body.ok) throw new Error(body.error || `HTTP ${r.status}`);
    await loadLivePreview();
  } catch (e) {
    showError(`Refresh failed: ${e.message}`);
  } finally {
    previewLoading.hidden = true;
    setBusy(false);
    loadStatus();
  }
}

async function doDeleteDevice() {
  if (!currentDevice) return;
  if (!confirm(`Delete device "${currentDevice}"? This can't be undone.`)) return;
  showError(null);
  setBusy(true);
  try {
    const r = await fetch(`api/devices/${encodeURIComponent(currentDevice)}`, {method: "DELETE"});
    const body = await r.json();
    if (!r.ok || !body.ok) throw new Error(body.error || `HTTP ${r.status}`);
    currentDevice = null;
    await loadDeviceList();
    await selectDevice(deviceSelect.value);
    // Same persist-failure case as doSave -- the delete already applied
    // in memory (the device is gone from the list above), but without
    // this the add-on would bring it back on its next restart, silently,
    // with no indication anything was wrong.
    if (body.persisted === false) {
      showError(`Deleted and gone for now, but not persisted to disk: ${body.warning}`, "warning");
    }
  } catch (e) {
    showError(`Delete failed: ${e.message}`);
  } finally {
    setBusy(false);
  }
}

deviceSelect.addEventListener("change", () => selectDevice(deviceSelect.value));
buttons.preview.addEventListener("click", doPreview);
buttons.save.addEventListener("click", doSave);
buttons.refresh.addEventListener("click", doRefresh);
buttons.deleteDevice.addEventListener("click", doDeleteDevice);

buttons.addDevice.addEventListener("click", () => {
  addDeviceRow.hidden = false;
  newDeviceNameInput.value = "";
  newDeviceNameInput.focus();
});
buttons.cancelAdd.addEventListener("click", () => {
  addDeviceRow.hidden = true;
});
buttons.confirmAdd.addEventListener("click", async () => {
  const name = newDeviceNameInput.value.trim().toLowerCase();
  if (!name) return;
  showError(null);
  // Populate the form with sensible defaults for a brand-new device
  // before switching to it, rather than showing the previous device's
  // values under a new name.
  populateForm({});
  currentDevice = name;
  addDeviceRow.hidden = true;
  await doSave();
});

loadDeviceList().then(() => {
  if (currentDevice) selectDevice(currentDevice);
});
setInterval(loadStatus, 20000);
