/* M1KE PROJECT site — command reference rendering, search, and a live m1ss playground. */

/* ---------------- command reference data ---------------- */
const SHELL = [
  ["help", "List available commands (friendly entry point)."],
  ["gui", "Launch the graphical glass desktop."],
  ["ls / cd / pwd / tree", "Navigate the in-memory filesystem."],
  ["mkdir / touch / rm / rmdir", "Create and remove files and directories."],
  ["cat / write", "Read a file / write text into a file."],
  ["echo / clear / history", "Print text, clear the screen, recall commands (↑/↓)."],
  ["mem / free / uptime", "Heap usage and how long the system has run."],
  ["uname / whoami / date / ps", "System, user, clock (real RTC) and process info."],
  ["neofetch / cowsay / fortune", "Show system info, an ASCII cow, hacker fortunes."],
  ["m1pkg ...", "Package manager — install/remove/search/info/list/update."],
  ["m1kectl ...", "The control plane — inspect & control everything."],
  ["dmesg", "Show the live system event log."],
  ["reboot / halt", "Restart or stop the machine."],
];

const CTL = [
  ["m1kectl list [kind]", "List every registered subsystem / service / driver."],
  ["m1kectl inspect &lt;obj&gt;", "Show the live state of an object (kernel, mem, wm…)."],
  ["m1kectl inspect process [pid]", "Inspect running processes."],
  ["m1kectl events  /  dmesg", "Dump the timestamped event bus."],
  ["m1kectl desktop theme set &lt;color&gt;", "Recolor the whole UI live (and save it)."],
  ["m1kectl wm border-radius &lt;n&gt;", "Change window corner radius."],
  ["m1kectl wm blur &lt;n&gt;", "Change the frosted-glass blur strength."],
  ["m1kectl theme show | reload | edit", "View, hot-reload, or edit the m1ss style sheet."],
  ["m1kectl theme set &lt;prop&gt; &lt;val&gt;", "Set any m1ss property (e.g. window.radius 22)."],
  ["m1kectl shell prompt set &lt;txt&gt;", "Change the shell prompt label."],
  ["m1kectl scheduler set &lt;policy&gt;", "cooperative | round-robin | priority."],
  ["m1kectl module list", "List loadable modules and their state."],
  ["m1kectl module load|unload &lt;name&gt;", "Hot-(un)load a driver — really masks its IRQ."],
  ["m1kectl keyboard layout es | us", "Switch keyboard layout (Spanish / US), persisted."],
  ["m1kectl service restart network", "Manage system services."],
  ["m1kectl config get|set|save|reload", "Read/write the text config plane."],
];

const M1SS = [
  ["desktop { accent }", "UI accent color: a name (orange, rose, green, blue, purple, gold) or #RRGGBB."],
  ["desktop { wallpaper }", "aurora | gradient | grid | solid."],
  ["desktop { glow1 / glow2 }", "The two aurora glow colors."],
  ["window { radius }", "Corner radius in px (0–40)."],
  ["window { blur }", "Frosted-glass blur radius (0–12)."],
  ["window { opacity }", "Glass tint strength (0–100)."],
  ["window { shadow }", "Drop-shadow spread in px."],
  ["window { border }", "Hairline accent border (0 or 1)."],
  ["window { tint }", "Glass tint color (#RRGGBB)."],
  ["taskbar { visible }", "true | false — yes, you can remove the taskbar."],
  ["taskbar { position }", "bottom | top."],
  ["taskbar { height / opacity }", "Taskbar size and translucency."],
  ["terminal { background / foreground }", "Colors of the in-window terminal."],
];

function fill(id, rows){
  const tb = document.getElementById(id);
  tb.innerHTML = rows.map(r => `<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join("");
}
fill("tbl-shell", SHELL);
fill("tbl-ctl", CTL);
fill("tbl-m1ss", M1SS);

/* ---------------- command search ---------------- */
document.getElementById("cmd-filter").addEventListener("input", e => {
  const q = e.target.value.toLowerCase();
  document.querySelectorAll("table.cmd tr").forEach(tr => {
    tr.style.display = tr.textContent.toLowerCase().includes(q) ? "" : "none";
  });
});

/* ---------------- m1ss playground (same grammar as the OS) ---------------- */
const NAMED = { orange:"#ff8c1a", rose:"#ff4d6d", green:"#46e07a",
  blue:"#5aa8ff", purple:"#b98cff", gold:"#ffd24a", white:"#eaeaea", black:"#000000", gray:"#909090" };

function parseColor(v){
  v = v.trim();
  if (v[0] === "#") return v;
  return NAMED[v] || null;
}

function parseM1ss(text){
  const t = {}, vars = {};
  let sel = "";
  // strip block + line/hash comments
  text = text.replace(/\/\*[\s\S]*?\*\//g, "").replace(/(\/\/|#).*$/gm, "");
  // collect variables (@name: value;) — same as the kernel parser
  text.replace(/@([a-zA-Z0-9_-]+)\s*:\s*([^;]+);/g, (_, n, v) => { vars[n] = v.trim(); return ""; });
  const resolve = v => { v = v.trim(); return v[0] === "@" ? (vars[v.slice(1)] || v) : v; };
  text.split(/\n/).forEach(line => {
    const open = line.match(/([a-zA-Z0-9_-]+)\s*\{/);
    if (open) { sel = open[1]; return; }
    if (line.includes("}")) { sel = ""; }
    if (line.trim()[0] === "@") return;            // variable definition, already captured
    const m = line.match(/([a-zA-Z0-9_-]+)\s*:\s*([^;]+);?/);
    if (m && sel) t[sel + "." + m[1]] = resolve(m[2]);
  });
  return t;
}

function applyTheme(){
  const t = parseM1ss(document.getElementById("m1ss-input").value);
  const pv = document.getElementById("preview");
  const wall = document.getElementById("pv-wall");
  const win = document.getElementById("pv-win");
  const bar = document.getElementById("pv-bar");

  const acc = parseColor(t["desktop.accent"] || "orange") || "#ff8c1a";
  const acc2 = parseColor(t["desktop.glow2"] || "purple") || "#6a4cff";
  pv.style.setProperty("--accent", acc);
  pv.style.setProperty("--accent2", acc2);

  const radius = Math.max(0, Math.min(40, parseInt(t["window.radius"]) || 14));
  win.style.borderRadius = radius + "px";

  const blur = Math.max(0, Math.min(16, parseInt(t["window.blur"]) || 8));
  win.style.backdropFilter = `blur(${blur}px)`;

  const op = Math.max(0, Math.min(100, isNaN(parseInt(t["window.opacity"])) ? 58 : parseInt(t["window.opacity"])));
  win.style.background = `rgba(22,22,30,${(op/100).toFixed(2)})`;

  // wallpaper
  const wp = (t["desktop.wallpaper"] || "aurora").trim();
  if (wp === "solid") wall.style.background = "#0c0c12";
  else if (wp === "grid") wall.style.background =
    "repeating-linear-gradient(0deg,#0a0a12 0 39px,#1c1822 39px 40px),repeating-linear-gradient(90deg,#0a0a12 0 39px,#1c1822 39px 40px)";
  else if (wp === "gradient") wall.style.background = `linear-gradient(180deg,#0a0a12,${acc2})`;
  else wall.style.background =
    `radial-gradient(40% 50% at 75% 20%, color-mix(in srgb,${acc} 55%,transparent), transparent 60%),`+
    `radial-gradient(45% 55% at 20% 85%, color-mix(in srgb,${acc2} 55%,transparent), transparent 60%), #0a0a12`;

  // taskbar
  const visible = (t["taskbar.visible"] || "true").trim() !== "false";
  bar.style.display = visible ? "" : "none";
  if ((t["taskbar.position"] || "bottom").trim() === "top") { bar.style.top = "0"; bar.style.bottom = "auto"; }
  else { bar.style.bottom = "0"; bar.style.top = "auto"; }
}
document.getElementById("m1ss-input").addEventListener("input", applyTheme);
applyTheme();

/* ---------------- download button ---------------- */
document.getElementById("iso-link").addEventListener("click", e => {
  e.preventDefault();
  alert("Build it yourself in seconds:\n\n  make all   → m1keos.iso\n  make gui   → boot the desktop\n\nPrebuilt ISOs are attached to each GitHub Release.");
});

/* ---------------- smooth scroll reveal (fast, GPU-friendly) ---------------- */
(function () {
  if (matchMedia("(prefers-reduced-motion: reduce)").matches) return;
  const sel = "section > h2, .card, .chips, .phases li, .playground, .cmd-search, .hero-text, .window.term, .dl";
  const targets = Array.from(document.querySelectorAll(sel));
  targets.forEach(el => el.setAttribute("data-reveal", ""));
  document.querySelectorAll(".grid .card").forEach((el, i) => el.style.setProperty("--d", (i * 60) + "ms"));
  const io = new IntersectionObserver((entries) => {
    entries.forEach(e => { if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); } });
  }, { threshold: 0.12, rootMargin: "0px 0px -8% 0px" });
  targets.forEach(t => io.observe(t));
})();
