"use strict";

const $ = (id) => document.getElementById(id);
const ui = {
  conversation: $("conversation"), input: $("message-input"), send: $("send-button"),
  connection: $("connection-pill"), provider: $("provider-value"), model: $("model-value"),
  memoryOwner: $("memory-owner-value"), topbar: $("topbar-label"), toast: $("toast")
};
let busy = false;
let toastTimer = 0;

function number(value, digits = 0) {
  const parsed = Number(value);
  if (!Number.isFinite(parsed)) return String(value ?? "0");
  return new Intl.NumberFormat("de-DE", { maximumFractionDigits: digits }).format(parsed);
}

function percent(value) {
  const bounded = Math.max(0, Math.min(1, Number(value) || 0));
  return `${Math.round(bounded * 100)} %`;
}

function setMeter(name, value) {
  const bounded = Math.max(0, Math.min(1, Number(value) || 0));
  $(`${name}-value`).textContent = percent(bounded);
  $(`${name}-meter`).style.width = `${bounded * 100}%`;
}

function showToast(message, error = false) {
  clearTimeout(toastTimer);
  ui.toast.textContent = message;
  ui.toast.className = `toast show${error ? " error" : ""}`;
  toastTimer = setTimeout(() => { ui.toast.className = "toast"; }, 3400);
}

async function request(path, options = {}) {
  const response = await fetch(path, {
    ...options,
    headers: { "Content-Type": "application/json", ...(options.headers || {}) }
  });
  let body = {};
  try { body = await response.json(); } catch { body = {}; }
  if (!response.ok) throw new Error(body.error || `HTTP ${response.status}`);
  return body;
}

function setConnection(mode, label) {
  ui.connection.className = `status-pill ${mode}`;
  ui.connection.querySelector("b").textContent = label;
}

function updateState(state = {}) {
  $("step-value").textContent = `#${state.step || 0}`;
  $("fingerprint-value").textContent = state.functional_fingerprint || "–";
  setMeter("confidence", state.confidence);
  setMeter("salience", state.salience);
  setMeter("novelty", state.novelty);
  const reps = Array.isArray(state.representations) ? state.representations : [];
  $("representation-count").textContent = String(reps.length);
  const list = $("representation-list");
  list.replaceChildren();
  if (!reps.length) {
    const empty = document.createElement("p");
    empty.className = "empty-state"; empty.textContent = "Noch keine Aktivität."; list.append(empty); return;
  }
  reps.slice(0, 8).forEach((rep) => {
    const row = document.createElement("div"); row.className = "representation";
    const id = document.createElement("code"); id.textContent = `#${rep.id}`;
    const bar = document.createElement("span"); bar.className = "rep-bar";
    const fill = document.createElement("i"); fill.style.width = `${Math.max(0, Math.min(100, Number(rep.activation) * 100))}%`; bar.append(fill);
    const value = document.createElement("b"); value.textContent = percent(rep.activation);
    row.append(id, bar, value); list.append(row);
  });
}

function updateMemory(data = {}) {
  $("episodes-value").textContent = number(data.episodes || data.recalled_episodes?.length || 0);
  $("synapses-value").textContent = number(data.synapses || data.episodic_memory_synapses || 0);
  $("spikes-value").textContent = number(data.last_reconstruction_spikes || data.episodic_reconstruction_spikes || 0);
  $("plasticity-value").textContent = number(data.plasticity_updates || data.episodic_plasticity_updates || 0);
  if (data.memory_owner) ui.memoryOwner.textContent = data.memory_owner;
}

function updateCommand(data = {}) {
  const command = data.command || {};
  $("attention-chip").textContent = command.attention || "balanced";
  $("intent-value").textContent = number(command.motor_intent || 0, 3);
  $("intent-strength-value").textContent = number(command.intent_strength || 0, 3);
  $("recall-cue-value").textContent = number(command.recall_cue || 0);
  $("recall-strength-value").textContent = number(command.recall_strength || 0, 3);
  const reward = Number(data.environment_reward || 0);
  $("reward-value").textContent = number(reward, 3);
  $("reward-value").style.color = reward < 0 ? "var(--red)" : reward > 0 ? "var(--green)" : "var(--muted)";
  $("planning-latency").textContent = `${number(data.planning_latency_ms || 0)} ms · ${data.model || "Modell"}`;
  $("language-latency").textContent = `${number(data.language_latency_ms || 0)} ms · sichtbar`;
}

function message(role, text, metadata = "") {
  const article = document.createElement("article");
  article.className = `message ${role}`;
  const avatar = document.createElement("div"); avatar.className = "avatar"; avatar.textContent = role === "user" ? "DU" : "T";
  const body = document.createElement("div"); body.className = "message-body";
  const meta = document.createElement("div"); meta.className = "message-meta"; meta.textContent = metadata || (role === "user" ? "Nutzer" : "TATARUS");
  const bubble = document.createElement("div"); bubble.className = "bubble"; bubble.textContent = text;
  body.append(meta, bubble); article.append(avatar, body); ui.conversation.append(article);
  ui.conversation.scrollTop = ui.conversation.scrollHeight;
  return { article, body, bubble, meta };
}

function thinkingMessage() {
  const item = message("assistant", "", "TATARUS · neuronale Verarbeitung");
  item.article.classList.add("thinking");
  item.bubble.replaceChildren();
  const dots = document.createElement("span"); dots.className = "thinking-dots";
  dots.append(document.createElement("i"), document.createElement("i"), document.createElement("i"));
  item.bubble.append(dots);
  return item;
}

function attachRecall(body, episodes) {
  if (!Array.isArray(episodes) || !episodes.length) return;
  const details = document.createElement("details"); details.className = "recall-box";
  const summary = document.createElement("summary"); summary.textContent = `${episodes.length} spike-rekonstruierte Erinnerung${episodes.length === 1 ? "" : "en"}`;
  const list = document.createElement("ul");
  episodes.forEach((episode) => {
    const item = document.createElement("li");
    const content = String(episode.content || "");
    item.textContent = `${episode.role || "Episode"} · Score ${number(episode.retrieval_score || 0, 3)} · ${content.length > 180 ? `${content.slice(0, 180)}…` : content}`;
    list.append(item);
  });
  details.append(summary, list); body.append(details);
}

function resizeInput() {
  ui.input.style.height = "auto";
  ui.input.style.height = `${Math.min(ui.input.scrollHeight, 160)}px`;
}

async function send(text = ui.input.value.trim()) {
  if (busy || !text) return;
  busy = true; ui.send.disabled = true; ui.input.disabled = true;
  message("user", text);
  ui.input.value = ""; resizeInput();
  const pending = thinkingMessage();
  ui.topbar.textContent = "TATARUS verarbeitet den aktuellen Turn";
  try {
    const data = await request("/v1/step", { method: "POST", body: JSON.stringify({ user_input: text }) });
    pending.article.classList.remove("thinking");
    pending.meta.textContent = `TATARUS · ${data.language_model || data.model || "LLM-Kortex"}`;
    pending.bubble.textContent = data.response || "[Keine sichtbare Sprachantwort]";
    if (data.language_error) pending.article.classList.add("error");
    attachRecall(pending.body, data.recalled_episodes);
    updateState(data.cognitive_state);
    updateMemory(data);
    updateCommand(data);
    ui.model.textContent = data.language_model || data.model || "geladen";
    ui.provider.textContent = data.provider || "–";
    ui.topbar.textContent = `Zustand fortgesetzt · Schritt ${data.cognitive_state?.step || "–"}`;
    setConnection("online", "Aktiv");
    if (data.language_error) showToast(`Sprachphase: ${data.language_error}`, true);
  } catch (error) {
    pending.article.classList.remove("thinking"); pending.article.classList.add("error");
    pending.meta.textContent = "TATARUS · Verbindungsfehler";
    pending.bubble.textContent = error.message;
    ui.topbar.textContent = "Der Turn konnte nicht abgeschlossen werden";
    setConnection("error", "Fehler"); showToast(error.message, true);
  } finally {
    busy = false; ui.send.disabled = false; ui.input.disabled = false; ui.input.focus();
    ui.conversation.scrollTop = ui.conversation.scrollHeight;
  }
}

async function initialize() {
  try {
    const [health, state, memory] = await Promise.all([request("/health"), request("/v1/state"), request("/v1/memory")]);
    ui.provider.textContent = health.provider || "–";
    ui.model.textContent = health.model || "wird beim ersten Turn erkannt";
    ui.memoryOwner.textContent = health.memory_owner || memory.memory_owner || "–";
    ui.topbar.textContent = "TATARUS ist bereit";
    updateState(state); updateMemory(memory); setConnection("online", "Bereit");
  } catch (error) {
    setConnection("error", "Offline"); ui.topbar.textContent = "Gateway nicht erreichbar"; showToast(error.message, true);
  }
}

ui.send.addEventListener("click", () => send());
ui.input.addEventListener("input", resizeInput);
ui.input.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && event.ctrlKey) { event.preventDefault(); send(); }
});
document.querySelectorAll("[data-prompt]").forEach((button) => button.addEventListener("click", () => send(button.dataset.prompt)));
$("clear-button").addEventListener("click", () => {
  ui.conversation.querySelectorAll(".message").forEach((item) => item.remove());
  showToast("Nur die sichtbare Browseransicht wurde geleert. TATARUS blieb unverändert.");
});
$("save-button").addEventListener("click", async () => {
  try { const data = await request("/v1/save", { method: "POST", body: "{}" }); showToast(`Snapshot gespeichert: ${data.path}`); }
  catch (error) { showToast(error.message, true); }
});
$("load-button").addEventListener("click", async () => {
  if (!window.confirm("Gespeicherten TATARUS-Zustand laden? Der aktuelle nicht gespeicherte Zustand wird ersetzt.")) return;
  try {
    const data = await request("/v1/load", { method: "POST", body: "{}" });
    const [state, memory] = await Promise.all([request("/v1/state"), request("/v1/memory")]);
    updateState(state); updateMemory(memory); showToast(`Snapshot geladen: ${data.path}`);
  } catch (error) { showToast(error.message, true); }
});

initialize();
