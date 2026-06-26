const metricCards = [
  ["nearest_obstacle_m", "Nearest obstacle", "m"],
  ["speed_mps", "Speed", "m/s"],
  ["braking_distance_m", "Braking distance", "m"],
  ["required_clearance_m", "Required clearance", "m"],
];

function setCardColor(id, color) {
  const element = document.getElementById(id);
  if (!element) {
    return;
  }

  element.classList.remove("green", "yellow", "red", "gray");
  element.classList.add(color || "gray");
}

function valueText(value) {
  if (value === null || value === undefined) {
    return "--";
  }

  return value;
}

function renderMetrics(data) {
  const grid = document.getElementById("metricsGrid");
  const safety = data.safety || {};
  const metrics = data.metrics || {};

  const values = {
    nearest_obstacle_m: safety.nearest_obstacle_m ?? metrics.nearest_obstacle_m,
    speed_mps: safety.speed_mps ?? metrics.speed_mps,
    braking_distance_m: safety.braking_distance_m,
    required_clearance_m: safety.required_clearance_m,
  };

  grid.innerHTML = metricCards.map(([key, label, unit]) => `
    <article class="metric-card">
      <div class="metric-label">${label}</div>
      <div class="metric-value">${valueText(values[key])}<span class="metric-unit">${unit}</span></div>
    </article>
  `).join("");
}

function renderHealth(data) {
  const grid = document.getElementById("healthGrid");
  const health = data.health || {};
  const items = Object.values(health).sort((a, b) =>
    (a.topic_name || "").localeCompare(b.topic_name || "")
  );

  if (!items.length) {
    grid.innerHTML = '<p class="health-meta">Waiting for health status...</p>';
    return;
  }

  grid.innerHTML = items.map(item => `
    <article class="health-tile ${item.color}">
      <div class="health-title">${item.node_name || "unknown_node"}</div>
      <div class="health-meta">${item.topic_name || "unknown_topic"}</div>
      <div class="health-status">${item.status} / ${item.reason}</div>
      <div class="health-meta">${item.message || ""} ${item.age_s ? `· age ${item.age_s}s` : ""}</div>
    </article>
  `).join("");
}

function renderEvents(data) {
  const list = document.getElementById("eventList");
  if (!list) {
    return;
  }

  const events = data.events || [];

  if (!events.length) {
    list.innerHTML = '<p class="health-meta">No events yet.</p>';
    return;
  }

  list.innerHTML = events.map(event => `
    <div class="event-row ${event.color}">
      <div>${event.label}</div>
      <div class="event-time">${event.time}</div>
    </div>
  `).join("");
}

function renderManagement(data) {
  const management = data.management || {};

  document.getElementById("missionActiveText").textContent =
    String(Boolean(management.mission_active));
  document.getElementById("maintenanceModeText").textContent =
    String(Boolean(management.maintenance_mode));
  document.getElementById("managementReasonText").textContent =
    management.reason || "NO_DATA";
  document.getElementById("managementMessage").textContent =
    management.message || "No management data";

  const inactiveList = document.getElementById("inactiveList");
  const modules = management.planned_inactive_modules || [];
  const moduleReasons = management.planned_inactive_module_reasons || [];
  const topics = management.planned_inactive_topics || [];
  const topicReasons = management.planned_inactive_topic_reasons || [];

  if (modules.length === 0 && topics.length === 0) {
    inactiveList.innerHTML = '<p class="health-meta">No planned inactive modules or topics.</p>';
    return;
  }

  const moduleRows = modules.map((name, index) => `
    <div class="inactive-item module">
      <strong>Module</strong>
      <span>${name}</span>
      <em>${moduleReasons[index] || ""}</em>
    </div>
  `);

  const topicRows = topics.map((name, index) => `
    <div class="inactive-item topic">
      <strong>Topic</strong>
      <span>${name}</span>
      <em>${topicReasons[index] || ""}</em>
    </div>
  `);

  inactiveList.innerHTML = [...moduleRows, ...topicRows].join("");
}

function renderMissionScene(data) {
  const management = data.management || {};
  const supervisor = data.supervisor || {};
  const missionActive = Boolean(management.mission_active);
  const mode = supervisor.mode || "NO DATA";

  const badge = document.getElementById("missionBadge");
  const summary = document.getElementById("missionSummary");
  const drone = document.getElementById("droneVisual");
  const warning = document.getElementById("sceneWarning");

  badge.className = "mission-badge";
  drone.className = "drone-visual";
  warning.classList.add("hidden");

  if (!missionActive) {
    badge.classList.add("idle");
    badge.textContent = "AT BASE";
    drone.classList.add("grounded");
    summary.textContent = "Mission inactive. Drone is at base.";
    return;
  }

  if (mode === "NORMAL") {
    badge.classList.add("active");
    badge.textContent = "MISSION ACTIVE";
    drone.classList.add("flying");
    summary.textContent = "Mission active. System is normal.";
    return;
  }

  if (mode === "FAILSAFE") {
    badge.classList.add("failsafe");
    badge.textContent = "FAILSAFE";
    drone.classList.add("returning");
    warning.classList.remove("hidden");
    summary.textContent = "Failsafe active. Drone should return or hold safely.";
    return;
  }

  if (mode === "EMERGENCY_STOP") {
    badge.classList.add("emergency");
    badge.textContent = "EMERGENCY";
    drone.classList.add("emergency");
    warning.classList.remove("hidden");
    summary.textContent = "Emergency stop active. Movement blocked.";
    return;
  }

  badge.classList.add("hold");
  badge.textContent = mode;
  drone.classList.add("hold");
  summary.textContent = `Mission active, supervisor state: ${mode}`;
}

function render(data) {
  document.getElementById("connectionText").textContent =
    "Connected to ROS dashboard bridge";
  document.getElementById("updatedAt").textContent =
    new Date().toLocaleTimeString();

  const supervisor = data.supervisor || {};
  document.getElementById("supervisorMode").textContent =
    supervisor.mode || "NO DATA";
  document.getElementById("supervisorReason").textContent =
    supervisor.reason || "NO DATA";
  document.getElementById("supervisorMessage").textContent =
    supervisor.message || "";
  setCardColor("supervisorCard", supervisor.color);

  const safety = data.safety || {};
  document.getElementById("safetyState").textContent =
    safety.state || "NO DATA";
  document.getElementById("safetyReason").textContent =
    safety.reason || "NO DATA";
  setCardColor("safetyCard", safety.color);

  const commandAllowed = Boolean(supervisor.command_allowed);
  document.getElementById("commandAllowed").textContent =
    commandAllowed ? "ALLOWED" : "BLOCKED";
  setCardColor("commandCard", commandAllowed ? "green" : "red");

  renderMissionScene(data);
  renderManagement(data);
  renderMetrics(data);
  renderHealth(data);
  renderEvents(data);
}

const events = new EventSource("/events");

events.onmessage = event => {
  render(JSON.parse(event.data));
};

events.onerror = () => {
  document.getElementById("connectionText").textContent =
    "DASHBOARD DISCONNECTED - live ROS data is not updating";
  document.getElementById("updatedAt").textContent =
    new Date().toLocaleTimeString();

  document.getElementById("supervisorMode").textContent = "STALE";
  document.getElementById("supervisorReason").textContent =
    "DASHBOARD_DISCONNECTED";
  document.getElementById("supervisorMessage").textContent =
    "dashboard bridge is not connected";
  setCardColor("supervisorCard", "gray");

  document.getElementById("safetyState").textContent = "STALE";
  document.getElementById("safetyReason").textContent =
    "DASHBOARD_DISCONNECTED";
  setCardColor("safetyCard", "gray");

  document.getElementById("commandAllowed").textContent = "BLOCKED";
  setCardColor("commandCard", "gray");

  document.getElementById("missionBadge").className = "mission-badge idle";
  document.getElementById("missionBadge").textContent = "DISCONNECTED";
  document.getElementById("missionSummary").textContent =
    "Dashboard bridge disconnected. Live ROS state is unavailable.";
  document.getElementById("droneVisual").className = "drone-visual grounded";
  document.getElementById("sceneWarning").classList.remove("hidden");

  document.getElementById("managementReasonText").textContent = "STALE";
  document.getElementById("managementMessage").textContent =
    "dashboard bridge is not connected";

  const healthTiles = document.querySelectorAll(".health-tile");
  if (healthTiles.length === 0) {
    document.getElementById("healthGrid").innerHTML = `
      <article class="health-tile gray">
        <div class="health-title">Dashboard bridge</div>
        <div class="health-meta">/events</div>
        <div class="health-status">STALE / DASHBOARD_DISCONNECTED</div>
        <div class="health-meta">dashboard bridge is not connected</div>
      </article>
    `;
  } else {
    healthTiles.forEach(tile => {
      tile.classList.remove("green", "yellow", "red");
      tile.classList.add("gray");

      const status = tile.querySelector(".health-status");
      const metas = tile.querySelectorAll(".health-meta");

      if (status) {
        status.textContent = "STALE / DASHBOARD_DISCONNECTED";
      }

      if (metas.length > 1) {
        metas[1].textContent = "dashboard bridge is not connected";
      }
    });
  }

  const eventList = document.getElementById("eventList");
  if (eventList) {
    eventList.innerHTML = `
      <div class="event-row gray">
        <div>Dashboard disconnected</div>
        <div class="event-time">${new Date().toLocaleTimeString()}</div>
      </div>
    `;
  }
};
