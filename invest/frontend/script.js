/* =============================================
   INVESTOPEDIA — script.js

   Frontend sirf localhost:8080 se baat karta hai.
   Alpha Vantage ka kaam C++ backend karta hai.

   Browser → localhost:8080 → stockmarket.cpp → Alpha Vantage
   ============================================= */

const API = "http://localhost:8080";

let currentUser   = null;
let chartInstance = null;
let currentTicker = null;
let liveStockData = {};
let pollInterval  = null;

// =============================================
//  SERVER STATUS
// =============================================
function setServerStatus(online) {
  const dot    = document.getElementById("live-dot-span");
  const text   = document.getElementById("live-dot-text");
  const banner = document.getElementById("offline-banner");
  if (online) {
    dot.style.background = "var(--accent2)";
    dot.style.animation  = "pulse 1.6s ease-in-out infinite";
    text.textContent     = "LIVE";
    text.style.color     = "var(--accent2)";
    if (banner) banner.style.display = "none";
  } else {
    dot.style.background = "var(--red)";
    dot.style.animation  = "none";
    text.textContent     = "SERVER OFFLINE";
    text.style.color     = "var(--red)";
    if (banner) banner.style.display = "flex";
  }
}

// =============================================
//  LIVE PRICES — backend se (har 2 second)
// =============================================
async function fetchLiveStocks() {
  try {
    const res  = await fetch(`${API}/stocks`, { signal: AbortSignal.timeout(3000) });
    const data = await res.json();
    if (data.stocks) {
      data.stocks.forEach(s => { liveStockData[s.symbol] = s; });
      setServerStatus(true);
      updateTickerBar();
      updateMoverCards();
      if (currentTicker && chartInstance) updatePriceTicker();
    }
  } catch {
    setServerStatus(false);
  }
}

// =============================================
//  CHART — backend se history data
// =============================================
function loadChart() {
  const ticker = document.getElementById("company-select").value;
  if (!ticker) { alert("Pehle company select karo."); return; }
  currentTicker = ticker;

  const stock = liveStockData[ticker];
  if (!stock) {
    document.getElementById("chart-placeholder").innerHTML =
      `<p style="color:var(--red)">Server se data nahi mila. stockmarket.exe chala raha hai?</p>`;
    return;
  }

  drawChart(ticker, stock.history || []);
}

function drawChart(ticker, historyINR) {
  document.getElementById("chart-placeholder").classList.add("hidden");
  const canvas = document.getElementById("stockChart");
  canvas.classList.remove("hidden");

  // Labels: last N points
  const now    = new Date();
  const labels = historyINR.map((_, i) => {
    const d = new Date(now - (historyINR.length - 1 - i) * 2000);
    return d.toLocaleTimeString("en-IN", { hour: "2-digit", minute: "2-digit" });
  });

  const stock     = liveStockData[ticker];
  const priceINR  = stock ? Math.round(stock.priceINR) : historyINR[historyINR.length - 1];
  const changeINR = stock ? Math.round(stock.changeINR) : 0;
  const pct       = stock ? stock.changePercent : 0;
  const isUp      = changeINR >= 0;
  const hex       = getColor(ticker);

  // Update ticker info
  document.getElementById("ti-symbol").textContent = ticker;
  document.getElementById("ti-price").textContent  = formatINR(priceINR);
  const d = document.getElementById("ti-delta");
  d.textContent = `${isUp ? "+" : ""}${changeINR} (${pct.toFixed(2)}%)`;
  d.className   = `ticker-delta ${isUp ? "positive" : "negative"}`;
  document.getElementById("ti-vol").textContent = stock ? `Vol: ${(stock.volume / 1e6).toFixed(1)}M` : "";
  const cap = document.getElementById("ti-cap");
  cap.textContent  = "Alpha Vantage";
  cap.style.color  = "#4af0c4";
  cap.style.fontSize = ".7rem";
  document.getElementById("ticker-info").classList.remove("hidden");

  if (chartInstance) chartInstance.destroy();

  const ctx  = canvas.getContext("2d");
  const grad = ctx.createLinearGradient(0, 0, 0, 300);
  grad.addColorStop(0, hex + "33");
  grad.addColorStop(1, hex + "00");

  chartInstance = new Chart(ctx, {
    type: "line",
    data: {
      labels,
      datasets: [{
        label: ticker, data: historyINR,
        borderColor: hex, borderWidth: 2.5,
        backgroundColor: grad,
        pointRadius: 0, pointHoverRadius: 5,
        pointHoverBackgroundColor: hex,
        fill: true, tension: 0.35
      }]
    },
    options: {
      responsive: true, maintainAspectRatio: false,
      animation: { duration: 400 },
      interaction: { mode: "index", intersect: false },
      plugins: {
        legend: { display: false },
        tooltip: {
          backgroundColor: "#161c28", borderColor: "#1e2636", borderWidth: 1,
          titleColor: "#e8edf5", bodyColor: hex,
          titleFont: { family: "'DM Mono', monospace", size: 11 },
          bodyFont:  { family: "'DM Mono', monospace", size: 13, weight: "500" },
          padding: 12,
          callbacks: { label: ctx => ` ₹${ctx.parsed.y.toLocaleString("en-IN")}` }
        }
      },
      scales: {
        x: {
          grid:  { color: "#1e2636", drawBorder: false },
          ticks: { color: "#6b7691", font: { family: "'DM Mono', monospace", size: 10 }, maxTicksLimit: 6 }
        },
        y: {
          position: "right",
          grid:  { color: "#1a2030", drawBorder: false },
          ticks: { color: "#6b7691", font: { family: "'DM Mono', monospace", size: 10 },
                   callback: v => `₹${v.toLocaleString("en-IN")}` }
        }
      }
    }
  });

  document.getElementById("trade-panel").style.display = "flex";
  updateTradeCost();
}

// Live price update karo chart ke niche (chart redraw nahi)
function updatePriceTicker() {
  const s = liveStockData[currentTicker]; if (!s) return;
  const p = Math.round(s.priceINR), c = Math.round(s.changeINR), up = c >= 0;
  document.getElementById("ti-price").textContent = formatINR(p);
  const d = document.getElementById("ti-delta");
  d.textContent = `${up ? "+" : ""}${c} (${s.changePercent.toFixed(2)}%)`;
  d.className   = `ticker-delta ${up ? "positive" : "negative"}`;
  // Live price ko chart mein add karo
  if (chartInstance && s.history) {
    const now = new Date();
    chartInstance.data.labels.push(now.toLocaleTimeString("en-IN", { hour: "2-digit", minute: "2-digit" }));
    chartInstance.data.datasets[0].data.push(Math.round(s.priceINR));
    if (chartInstance.data.labels.length > 60) {
      chartInstance.data.labels.shift();
      chartInstance.data.datasets[0].data.shift();
    }
    chartInstance.update("none");
  }
  updateTradeCost();
}

// =============================================
//  NAVIGATION
// =============================================
function goTo(id) {
  const cur = document.querySelector(".screen.active");
  if (cur) {
    cur.classList.add("fade-out");
    setTimeout(() => { cur.classList.remove("active", "fade-out"); cur.style.display = "none"; }, 380);
  }
  setTimeout(() => {
    const next = document.getElementById(id);
    next.style.display = "flex";
    requestAnimationFrame(() => next.classList.add("active"));
  }, 200);
}

// =============================================
//  AUTH
// =============================================
async function handleLogin() {
  const username = document.getElementById("login-username").value.trim();
  const password = document.getElementById("login-password").value;
  const err      = document.getElementById("login-error");
  if (!username || !password) {
    err.textContent = "Username aur password daalo.";
    err.classList.remove("hidden"); return;
  }
  try {
    const res  = await fetch(`${API}/login`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password })
    });
    const data = await res.json();
    if (!data.success) {
      err.textContent = data.message || "Invalid credentials.";
      err.classList.remove("hidden");
      shake("#screen-login .auth-card"); return;
    }
    err.classList.add("hidden");
    currentUser = { username: data.username, name: data.name, balance: data.balance };
    enterMarket();
  } catch {
    err.textContent = "Server nahi mila! stockmarket.exe chalaao pehle.";
    err.classList.remove("hidden");
  }
}

async function handleRegister() {
  const name     = document.getElementById("reg-name").value.trim();
  const username = document.getElementById("reg-username").value.trim();
  const password = document.getElementById("reg-password").value;
  const err      = document.getElementById("reg-error");
  const suc      = document.getElementById("reg-success");
  if (!name || !username || !password) {
    err.textContent = "Sab fields bharo.";
    err.classList.remove("hidden");
    shake("#screen-register .auth-card"); return;
  }
  try {
    const res  = await fetch(`${API}/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username, password, name })
    });
    const data = await res.json();
    if (!data.success) { err.textContent = data.message || "Failed."; err.classList.remove("hidden"); return; }
    err.classList.add("hidden");
    suc.textContent = "Account ban gaya! Login karo."; suc.classList.remove("hidden");
    setTimeout(() => {
      suc.classList.add("hidden");
      ["reg-name", "reg-username", "reg-password"].forEach(id => document.getElementById(id).value = "");
      goTo("screen-login");
    }, 1800);
  } catch {
    err.textContent = "Server nahi mila!"; err.classList.remove("hidden");
  }
}

function handleLogout() {
  currentUser = null; currentTicker = null;
  if (chartInstance) { chartInstance.destroy(); chartInstance = null; }
  if (pollInterval)  { clearInterval(pollInterval); pollInterval = null; }
  document.getElementById("chart-placeholder").classList.remove("hidden");
  document.getElementById("chart-placeholder").innerHTML = `
    <svg viewBox="0 0 64 64" fill="none" stroke="currentColor" stroke-width="1.5" width="48" height="48">
      <polyline points="4 48 16 32 26 38 38 18 50 24 60 8"/>
      <line x1="4" y1="56" x2="60" y2="56"/>
    </svg><p>Company select karo aur <strong>Show Chart</strong> click karo</p>`;
  document.getElementById("stockChart").classList.add("hidden");
  document.getElementById("ticker-info").classList.add("hidden");
  document.getElementById("trade-panel").style.display = "none";
  document.getElementById("company-select").value = "";
  document.getElementById("login-username").value = "";
  document.getElementById("login-password").value = "";
  goTo("screen-login");
}

async function enterMarket() {
  document.getElementById("user-display").textContent = currentUser.name || currentUser.username;
  document.getElementById("user-avatar").textContent  = (currentUser.name || currentUser.username)[0].toUpperCase();
  updateBalanceDisplay(currentUser.balance);
  goTo("screen-market");
  await fetchLiveStocks();
  await loadPortfolio();
  if (pollInterval) clearInterval(pollInterval);
  pollInterval = setInterval(fetchLiveStocks, 2000);
}

function shake(sel) {
  const el = document.querySelector(sel); if (!el) return;
  el.style.animation = "none"; el.offsetHeight; el.style.animation = "shake .4s ease";
}
document.head.insertAdjacentHTML("beforeend", `<style>
@keyframes shake {
  0%,100%{ transform:translateX(0) }
  20%    { transform:translateX(-8px) }
  40%    { transform:translateX(8px) }
  60%    { transform:translateX(-5px) }
  80%    { transform:translateX(5px) }
}
</style>`);
document.addEventListener("keydown", e => {
  if (e.key !== "Enter") return;
  if (document.getElementById("screen-login").classList.contains("active")) handleLogin();
  if (document.getElementById("screen-register").classList.contains("active")) handleRegister();
});

// =============================================
//  HELPERS
// =============================================
function formatINR(n) { return "₹" + Math.round(n).toLocaleString("en-IN"); }
function updateBalanceDisplay(b) { document.getElementById("stat-balance").textContent = formatINR(b); }
function getColor(t) {
  return { AAPL:"#c8f135", MSFT:"#4af0c4", GOOGL:"#ff4f6d", AMZN:"#c8f135",
           TSLA:"#ff4f6d", NVDA:"#c8f135", META:"#4af0c4", NFLX:"#c8f135",
           AMD:"#4af0c4",  INTC:"#ff4f6d" }[t] || "#c8f135";
}

// =============================================
//  TICKER BAR
// =============================================
function updateTickerBar() {
  const items = Object.values(liveStockData); if (!items.length) return;
  const html  = [...items, ...items].map(s => {
    const up = s.changeINR >= 0;
    return `<span>${s.symbol} <strong class="${up ? "t-up" : "t-down"}">₹${Math.round(s.priceINR).toLocaleString("en-IN")}</strong>
            <span class="${up ? "t-up" : "t-down"}">${up ? "▲" : "▼"}</span></span>`;
  }).join("");
  ["ticker-track", "ticker-track-2"].forEach(id => {
    const el = document.getElementById(id); if (el) el.innerHTML = html;
  });
}

// =============================================
//  MOVERS
// =============================================
function updateMoverCards() {
  const grid  = document.getElementById("movers-grid");
  const items = Object.values(liveStockData); if (!items.length) return;
  grid.innerHTML = [...items]
    .sort((a, b) => Math.abs(b.changePercent) - Math.abs(a.changePercent))
    .map(s => {
      const up = s.changeINR >= 0;
      return `<div class="mover-card" onclick="selectMover('${s.symbol}')">
        <div class="mover-ticker">${s.symbol}</div>
        <div class="mover-price">${formatINR(Math.round(s.priceINR))}</div>
        <div class="mover-change ${up ? "up" : "down"}">${up ? "▲" : "▼"} ${Math.abs(s.changePercent).toFixed(2)}%</div>
      </div>`;
    }).join("");
}

function selectMover(sym) {
  document.getElementById("company-select").value = sym;
  currentTicker = sym;
  loadChart();
  document.querySelector(".chart-panel").scrollIntoView({ behavior: "smooth" });
}

function setTimeframe(btn, tf) {
  document.querySelectorAll(".tf-btn").forEach(b => b.classList.remove("active"));
  btn.classList.add("active");
  if (currentTicker) loadChart();
}

// =============================================
//  TRADE COST
// =============================================
function updateTradeCost() {
  if (!currentTicker) return;
  const qty = parseInt(document.getElementById("trade-qty").value) || 0;
  const s   = liveStockData[currentTicker]; if (!s) return;
  document.getElementById("trade-cost").textContent = formatINR(Math.round(s.priceINR) * qty);
}
document.addEventListener("DOMContentLoaded", () => {
  const q = document.getElementById("trade-qty");
  if (q) q.addEventListener("input", updateTradeCost);
});

// =============================================
//  BUY
// =============================================
async function handleBuy() {
  if (!currentUser || !currentTicker) return;
  const qty = parseInt(document.getElementById("trade-qty").value);
  if (!qty || qty <= 0) { showMsg("Valid quantity daalo.", false); return; }
  const s = liveStockData[currentTicker];
  if (!s) { showMsg("Price data nahi mila!", false); return; }
  const price = Math.round(s.priceINR);
  try {
    const res  = await fetch(`${API}/buy`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username: currentUser.username, ticker: currentTicker, quantity: qty, price })
    });
    const data = await res.json();
    if (!data.balance) { showMsg(data.message || "Buy failed.", false); return; }
    currentUser.balance = data.balance;
    updateBalanceDisplay(data.balance);
    showMsg(`✅ Bought ${qty}x ${currentTicker} @ ${formatINR(price)}`, true);
    renderPortfolioData(data);
  } catch { showMsg("Server error!", false); }
}

// =============================================
//  SELL
// =============================================
async function handleSell() {
  if (!currentUser || !currentTicker) return;
  const qty = parseInt(document.getElementById("trade-qty").value);
  if (!qty || qty <= 0) { showMsg("Valid quantity daalo.", false); return; }
  const s = liveStockData[currentTicker];
  if (!s) { showMsg("Price data nahi mila!", false); return; }
  const price = Math.round(s.priceINR);
  try {
    const res  = await fetch(`${API}/sell`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ username: currentUser.username, ticker: currentTicker, quantity: qty, price })
    });
    const data = await res.json();
    if (!data.balance) { showMsg(data.message || "Sell failed.", false); return; }
    currentUser.balance = data.balance;
    updateBalanceDisplay(data.balance);
    showMsg(`✅ Sold ${qty}x ${currentTicker} @ ${formatINR(price)}`, true);
    renderPortfolioData(data);
  } catch { showMsg("Server error!", false); }
}

function showMsg(msg, ok) {
  const el = document.getElementById("trade-msg");
  el.textContent = msg;
  el.style.background = ok ? "rgba(74,240,196,.1)"           : "rgba(255,79,109,.1)";
  el.style.border     = ok ? "1px solid rgba(74,240,196,.3)" : "1px solid rgba(255,79,109,.3)";
  el.style.color      = ok ? "var(--accent2)"                : "var(--red)";
  el.classList.remove("hidden");
  setTimeout(() => el.classList.add("hidden"), 3500);
}

// =============================================
//  PORTFOLIO
// =============================================
async function loadPortfolio() {
  if (!currentUser) return;
  try {
    const res  = await fetch(`${API}/portfolio?username=${encodeURIComponent(currentUser.username)}`);
    const data = await res.json();
    currentUser.balance = data.balance;
    updateBalanceDisplay(data.balance);
    renderPortfolioData(data);
  } catch {}
}

function renderPortfolioData(data) {
  const portfolio = data.portfolio || [];
  const emptyEl   = document.getElementById("portfolio-empty");
  const tableEl   = document.getElementById("portfolio-table");
  const tbody     = document.getElementById("portfolio-tbody");
  document.getElementById("stat-positions").textContent = portfolio.length;
  if (!portfolio.length) { emptyEl.classList.remove("hidden"); tableEl.classList.add("hidden"); return; }
  emptyEl.classList.add("hidden"); tableEl.classList.remove("hidden");
  let pnl = 0;
  tbody.innerHTML = portfolio.map(item => {
    const live = liveStockData[item.ticker];
    const cur  = live ? Math.round(live.priceINR) : item.avgBuyPrice;
    const p    = (cur - item.avgBuyPrice) * item.quantity;
    pnl += p;
    return `<tr>
      <td><strong>${item.ticker}</strong></td>
      <td>${item.quantity}</td>
      <td>${formatINR(item.avgBuyPrice)}</td>
      <td>${formatINR(cur)}</td>
      <td class="${p >= 0 ? "pnl-pos" : "pnl-neg"}">${p >= 0 ? "+" : ""}${formatINR(Math.round(p))}</td>
    </tr>`;
  }).join("");
  const pl = document.getElementById("stat-pnl");
  const pc = document.getElementById("stat-pnl-change");
  pl.textContent = (pnl >= 0 ? "+" : "") + formatINR(Math.round(pnl));
  pl.style.color = pnl >= 0 ? "var(--accent)" : "var(--red)";
  pc.textContent = pnl >= 0 ? "Profit" : "Loss";
  pc.className   = "stat-change " + (pnl >= 0 ? "positive" : "negative");
}

// =============================================
//  INIT
// =============================================
document.querySelectorAll(".screen").forEach(s => {
  s.style.display = s.id === "screen-login" ? "flex" : "none";
});
document.getElementById("screen-login").classList.add("active");

// Har 3 second server ping — login screen pe bhi
setInterval(async () => {
  try {
    const r = await fetch(`${API}/stocks`, { signal: AbortSignal.timeout(2000) });
    setServerStatus(r.ok);
  } catch { setServerStatus(false); }
}, 3000);