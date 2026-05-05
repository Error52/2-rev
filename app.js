const STORAGE_KEY = "industrial_tasks_v1";

const defaultTasks = [
  {
    id: crypto.randomUUID(),
    title: "Boiler Room",
    category: "Web",
    points: 100,
    description: "Найди уязвимость в админке котельной.",
    flag: "flag{demo}",
    fileName: "",
    fileData: "",
    mimeType: "",
    solves: 12,
  },
];

const demoScores = [
  { team: "slagboys", score: 2450 },
  { team: "zero_day_shift", score: 2130 },
  { team: "night_welders", score: 1880 },
];

const state = { tasks: loadTasks() };
const grid = document.getElementById("challengeGrid");
const scores = document.getElementById("scores");

document.getElementById("taskForm").addEventListener("submit", onCreateTask);
document.getElementById("search").addEventListener("input", (e) => renderTasks(e.target.value.trim().toLowerCase()));

function loadTasks() {
  const raw = localStorage.getItem(STORAGE_KEY);
  if (!raw) return defaultTasks;
  try { return JSON.parse(raw); } catch { return defaultTasks; }
}

function saveTasks() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(state.tasks));
}

function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result).split(",")[1] || "");
    reader.onerror = reject;
    reader.readAsDataURL(file);
  });
}

async function onCreateTask(e) {
  e.preventDefault();
  const file = document.getElementById("taskFile").files[0];
  const task = {
    id: crypto.randomUUID(),
    title: document.getElementById("title").value,
    category: document.getElementById("category").value,
    points: Number(document.getElementById("points").value),
    description: document.getElementById("description").value,
    flag: document.getElementById("flag").value,
    fileName: file ? file.name : "",
    fileData: file ? await fileToBase64(file) : "",
    mimeType: file ? file.type || "application/octet-stream" : "",
    solves: 0,
  };
  state.tasks.unshift(task);
  saveTasks();
  e.target.reset();
  renderTasks();
}

function downloadTaskFile(task) {
  if (!task.fileData) return;
  const byteChars = atob(task.fileData);
  const bytes = new Uint8Array(byteChars.length);
  for (let i = 0; i < byteChars.length; i++) bytes[i] = byteChars.charCodeAt(i);
  const blob = new Blob([bytes], { type: task.mimeType || "application/octet-stream" });
  const link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = task.fileName || "task.bin";
  link.click();
  URL.revokeObjectURL(link.href);
}

function submitFlag(taskId) {
  const input = document.getElementById(`flag-${taskId}`);
  const msg = document.getElementById(`msg-${taskId}`);
  const task = state.tasks.find((t) => t.id === taskId);
  if (!task || !input || !msg) return;
  if (input.value.trim() === task.flag) {
    msg.textContent = "✅ Верный флаг";
    msg.className = "ok";
    task.solves += 1;
    saveTasks();
    renderTasks(document.getElementById("search").value.trim().toLowerCase());
  } else {
    msg.textContent = "❌ Неверно";
    msg.className = "";
  }
}

function renderTasks(query = "") {
  grid.innerHTML = "";
  const list = state.tasks.filter((t) => `${t.title} ${t.category}`.toLowerCase().includes(query));
  list.forEach((task) => {
    const card = document.createElement("article");
    card.className = "card";
    card.innerHTML = `
      <h3>${task.title}</h3>
      <p>${task.description}</p>
      <div class="meta"><span>${task.category}</span><span class="points">${task.points} pts</span></div>
      <div class="meta"><span>solved:</span><span>${task.solves}</span></div>
      <div class="actions">
        <button data-download="${task.id}" ${task.fileData ? "" : "disabled"}>${task.fileData ? "Download" : "No file"}</button>
        <input id="flag-${task.id}" placeholder="flag{...}" />
        <button data-submit="${task.id}">Submit</button>
      </div>
      <small id="msg-${task.id}"></small>
    `;
    grid.appendChild(card);
  });

  grid.querySelectorAll("button[data-download]").forEach((b) => {
    b.addEventListener("click", () => {
      const task = state.tasks.find((t) => t.id === b.dataset.download);
      if (task) downloadTaskFile(task);
    });
  });

  grid.querySelectorAll("button[data-submit]").forEach((b) => {
    b.addEventListener("click", () => submitFlag(b.dataset.submit));
  });
}

function renderScores() {
  scores.innerHTML = "";
  demoScores.forEach((s, i) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `<td>${i + 1}</td><td>${s.team}</td><td>${s.score}</td>`;
    scores.appendChild(tr);
  });
}

renderTasks();
renderScores();
