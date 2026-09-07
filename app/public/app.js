'use strict';

const DEVICES = ['kapi', 'kamera', 'salon'];
const MAX_LOG = 300;
const HIVEMQ_MONTHLY = 10e9;     // 10 GB
const $  = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];

const S = {
  mqtt: null,
  connected: false,
  acTemp: 24,
  filter: 'all',
  page: 'dashboard',
  dev: {
    kapi:   { status: null, state: {}, telemetry: {} },
    kamera: { status: null, state: {}, telemetry: {} },
    salon:  { status: null, state: {}, telemetry: {} },
  },
  usage: { today: 0, month: 0 },
  stream: { active: false, bytes: 0, keepalive: null, usageT: null, urls: [] },
};

// ============================ helpers ============================
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));

function fmtWhen(sec) {
  if (!sec || sec < 1e9) return 'bilinmiyor';
  const d = new Date(sec * 1000), now = new Date();
  const hm = d.toLocaleTimeString('tr-TR', { hour: '2-digit', minute: '2-digit' });
  if (d.toDateString() === now.toDateString()) return hm;
  return d.toLocaleDateString('tr-TR', { day: 'numeric', month: 'long' }) + ' ' + hm;
}
function fmtUptime(s) {
  s = Number(s) || 0;
  const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600), m = Math.floor((s % 3600) / 60);
  if (d) return `${d}g ${h}s`;
  if (h) return `${h}s ${m}dk`;
  return `${m} dk`;
}
function fmtBytes(b) {
  b = Number(b) || 0;
  if (b < 1024) return b + ' B';
  if (b < 1048576) return (b / 1024).toFixed(0) + ' KB';
  if (b < 1073741824) return (b / 1048576).toFixed(1) + ' MB';
  return (b / 1073741824).toFixed(2) + ' GB';
}
const tagColor = (d) => ({ kapi: '#8d6e63', kamera: '#00bcd4', salon: '#2196F3', sys: '#9aa0a6' }[d] || '#9aa0a6');

function toast(msg, kind = '') {
  const t = document.createElement('div');
  t.className = 'toast ' + kind;
  t.textContent = msg;
  $('#toasts').appendChild(t);
  setTimeout(() => t.remove(), 3200);
}

function setField(name, val) { $$(`[data-field="${name}"]`).forEach((e) => (e.textContent = val)); }
async function api(path, opts) {
  const r = await fetch(path, opts);
  let j = {};
  try { j = await r.json(); } catch {}
  if (!r.ok) throw new Error(j.error || `HTTP ${r.status}`);
  return j;
}

// ============================ boot / auth ============================
async function boot() {
  applyTheme();
  wireUI();
  try {
    const s = await fetch('/api/session');
    if (s.ok) { onAuthed(await s.json()); return; }
  } catch {}
  $('#login').hidden = false;
}

$('#login-form').addEventListener('submit', async (e) => {
  e.preventDefault();
  const btn = $('#li-btn'); btn.disabled = true; $('#li-err').textContent = '';
  try {
    const j = await api('/api/session', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username: $('#li-user').value.trim(), password: $('#li-pass').value }),
    });
    $('#login').hidden = true;
    onAuthed(j);
  } catch (err) {
    $('#li-err').textContent = err.message;
  } finally { btn.disabled = false; }
});

function onAuthed(data) {
  addLog('sys', 'INFO', `giriş: ${data.username}`);
  connectMqtt(data.mqtt);
  loadHistory();
}

// ============================ MQTT ============================
function connectMqtt(creds) {
  if (!creds || !creds.url) { renderConn('err'); return; }
  const c = mqtt.connect(creds.url, {
    username: creds.username, password: creds.password,
    clean: true, reconnectPeriod: 5000, connectTimeout: 8000,
    clientId: 'web-' + Math.random().toString(16).slice(2, 10),
  });
  S.mqtt = c;
  renderConn('...');
  c.on('connect', () => {
    S.connected = true; renderConn();
    addLog('sys', 'INFO', 'MQTT bağlandı');
    ['state', 'status', 'event', 'telemetry', 'log', 'cmd/ack'].forEach((k) => c.subscribe(`ev/+/${k}`));
    if (S.stream.active) c.subscribe('ev/kamera/stream');
  });
  c.on('reconnect', () => renderConn('...'));
  c.on('close', () => { S.connected = false; renderConn(); });
  c.on('error', (e) => { console.error('mqtt', e); renderConn('err'); });
  c.on('message', onMqtt);
}

function onMqtt(topic, payload) {
  const p = topic.split('/');        // ev / <dev> / <kind...>
  const dev = p[1], kind = p.slice(2).join('/');
  if (kind === 'stream') { renderFrame(payload); return; }

  const text = payload.toString();
  if (kind === 'status') { S.dev[dev] && (S.dev[dev].status = text); renderDevice(dev); return; }

  let m; try { m = JSON.parse(text); } catch { return; }
  if (!S.dev[dev]) return;
  if (kind === 'state')       { S.dev[dev].state = m; renderDevice(dev); }
  else if (kind === 'telemetry') { S.dev[dev].telemetry = m; renderTelemetry(dev); }
  else if (kind === 'event')     { onEvent(dev, m); }
  else if (kind === 'log')       { addLog(dev, m.lvl || 'INFO', m.msg || ''); }
  else if (kind === 'cmd/ack')   { onAck(dev, m); }
}

function onEvent(dev, m) {
  addLog(dev, 'INFO', 'olay · ' + JSON.stringify(m));
  if (m.type === 'motion') { S.dev.kamera.state.last_motion_ts = m.ts; renderKamera(); toast('Kamerada hareket'); }
  if (m.type === 'door_open') { S.dev.kapi.state.last_open_ts = m.ts; renderKapi(); }
}
function onAck(dev, m) {
  const ok = m.result === 'ok';
  toast(`${dev}: ${m.cmd} ${ok ? 'onaylandı' : 'HATA' + (m.detail ? ' — ' + m.detail : '')}`, ok ? 'ok' : 'err');
  addLog(dev, ok ? 'INFO' : 'ERROR', `ack ${m.cmd} → ${m.result}${m.detail ? ' (' + m.detail + ')' : ''}`);
}

// ============================ commands ============================
async function sendCmd(device, cmd, args = {}) {
  try {
    await api('/api/command', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ device, cmd, args }),
    });
    addLog(device, 'INFO', `komut → ${cmd}`);
  } catch (e) {
    toast('Komut hatası: ' + e.message, 'err');
    addLog(device, 'ERROR', `komut ${cmd} başarısız: ${e.message}`);
  }
}

// ============================ history ============================
async function loadHistory() {
  try {
    const j = await api('/api/history');
    (j.states || []).forEach((s) => { if (S.dev[s.device_id]) S.dev[s.device_id].state = s.snapshot || {}; });
    (j.telemetry || []).forEach((t) => {
      const d = S.dev[t.device_id];
      if (d && d.telemetry.rssi == null) d.telemetry = { rssi: t.rssi, heap: t.heap_free, uptime: t.uptime_s };
    });
    S.usage.today = j.usage_today || 0;
    S.usage.month = j.usage_month || 0;
    DEVICES.forEach((d) => { renderDevice(d); renderTelemetry(d); });
    renderUsage();
  } catch (e) { console.error(e); addLog('sys', 'WARN', 'geçmiş yüklenemedi: ' + e.message); }
}

// ============================ render ============================
function renderConn(mode) {
  const el = $('#conn'), txt = $('#conn-txt');
  el.className = 'conn-pill' + (S.connected ? ' ok' : mode === 'err' ? ' err' : '');
  txt.textContent = S.connected ? 'canlı' : mode === 'err' ? 'bağlantı hatası' : 'bağlanıyor…';
}

function renderDevice(dev) {
  const st = S.dev[dev];
  const online = st.status ? st.status === 'online' : !!st.state.online;
  $$(`[data-badge="${dev}"]`).forEach((b) => { b.textContent = online ? 'çevrimiçi' : 'çevrimdışı'; b.className = 'badge ' + (online ? 'online' : 'offline'); });
  const dot = $(`[data-dot="${dev}"]`); if (dot) dot.className = 'dot ' + (online ? 'online' : 'offline');
  ({ kapi: renderKapi, kamera: renderKamera, salon: renderSalon }[dev] || (() => {}))();
}

function renderKapi() {
  const s = S.dev.kapi.state;
  setField('kapi-last', fmtWhen(s.last_open_ts));
  setField('kapi-relaystate', s.relay ? 'AÇIK' : 'kapalı');
  $$('[data-icon="kapi-relay"]').forEach((i) => (i.textContent = s.relay ? 'lock_open' : 'lock'));
  $$('[data-cmd="kapi:open"]').forEach((b) => b.classList.toggle('active-state', !!s.relay));
}
function renderKamera() {
  const s = S.dev.kamera.state;
  setField('kamera-motion', fmtWhen(s.last_motion_ts));
  setField('kamera-streamstate', s.streaming ? `açık · ${s.res || ''} ${s.fps || 0}fps` : 'kapalı');
  $$('[data-light]').forEach((c) => (c.checked = !!s.light));
}
function renderSalon() {
  const s = S.dev.salon.state, on = !!s.ac, t = s.temp || S.acTemp;
  if (s.temp) S.acTemp = s.temp;
  setField('salon-status', on ? `AÇIK · ${t}°C` : 'KAPALI');
  setField('salon-label', on ? 'Klimayı Kapat' : 'Klimayı Aç');
  setField('salon-temp', on ? `${t}°C` : '—');
  setField('salon-settemp', `${S.acTemp}°C`);
  $$('[data-icon="salon-ac"]').forEach((i) => (i.style.color = on ? 'var(--primary)' : 'var(--text-sec)'));
  $$('[data-ac-toggle]').forEach((b) => b.classList.toggle('active-state', on));
}
function renderTelemetry(dev) {
  const t = S.dev[dev].telemetry;
  const g = $(`[data-telemetry="${dev}"]`); if (!g) return;
  const put = (k, v) => { const e = $(`[data-t="${k}"]`, g); if (e) e.textContent = v; };
  put('rssi', t.rssi != null ? t.rssi + ' dBm' : '—');
  put('heap', t.heap != null ? Math.round(t.heap / 1024) + ' KB' : '—');
  put('uptime', t.uptime != null ? fmtUptime(t.uptime) : '—');
  put('ip', t.ip || '—');
}
function renderUsage() {
  const em = S.usage.month * 2, et = S.usage.today * 2;   // çift yön tahmini
  setField('usage-today', fmtBytes(et));
  setField('usage-month', fmtBytes(em) + ' / 10 GB');
  const bar = (name, val, max) => {
    const el = $(`[data-bar="${name}"]`); if (!el) return;
    const pct = Math.min(100, (val / max) * 100);
    $('span', el).style.width = pct + '%';
    el.className = 'usage-bar' + (pct >= 99 ? ' danger' : pct > 80 ? ' warn' : '');
  };
  bar('today', et, HIVEMQ_MONTHLY / 30);
  bar('month', em, HIVEMQ_MONTHLY);
}

// ============================ camera stream ============================
function renderFrame(payload) {
  S.stream.bytes += payload.length || payload.byteLength || 0;
  const url = URL.createObjectURL(new Blob([payload], { type: 'image/jpeg' }));
  S.stream.urls.push(url);
  while (S.stream.urls.length > 3) URL.revokeObjectURL(S.stream.urls.shift());
  const box = S.page === 'kamera' ? $('#detail-cam') : $('#dash-cam');
  if (box) { box.classList.add('live'); $('img', box).src = url; }
}
function toggleStream() { S.stream.active ? stopStream() : startStream(); }
function startStream() {
  const res = ($('[data-res]') || {}).value || 'vga';
  const fps = +(($('[data-fps]') || {}).value || 5);
  S.stream.active = true; S.stream.bytes = 0;
  if (S.mqtt && S.connected) S.mqtt.subscribe('ev/kamera/stream');
  sendCmd('kamera', 'stream_start', { res, fps });
  S.stream.keepalive = setInterval(() => sendCmd('kamera', 'stream_start', {
    res: ($('[data-res]') || {}).value || 'vga', fps: +(($('[data-fps]') || {}).value || 5),
  }), 60000);
  S.stream.usageT = setInterval(reportUsage, 30000);
  renderStreamBtn();
}
function stopStream() {
  S.stream.active = false;
  clearInterval(S.stream.keepalive); clearInterval(S.stream.usageT);
  reportUsage();
  try { S.mqtt && S.mqtt.unsubscribe('ev/kamera/stream'); } catch {}
  sendCmd('kamera', 'stream_stop');
  $$('.cam-box').forEach((b) => b.classList.remove('live'));
  renderStreamBtn();
}
async function reportUsage() {
  const b = S.stream.bytes; S.stream.bytes = 0;
  if (b <= 0) return;
  try {
    const j = await api('/api/usage', {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ bytes: b }),
    });
    if (j.day_bytes != null) S.usage.today = j.day_bytes;
    S.usage.month += b;
    renderUsage();
  } catch {}
}
function renderStreamBtn() {
  $$('[data-stream-toggle]').forEach((b) => {
    b.classList.toggle('stop', S.stream.active);
    $('.material-symbols-outlined', b).textContent = S.stream.active ? 'stop_circle' : 'play_arrow';
    $('span:last-child', b).textContent = S.stream.active ? 'Yayını Durdur' : 'Yayını Başlat';
  });
}

// ============================ terminal ============================
function addLog(dev, lvl, msg) {
  const box = $('#log-box'); if (!box) return;
  const line = document.createElement('div');
  line.className = 'log-line ' + (lvl || 'INFO');
  line.dataset.dev = dev;
  line.innerHTML =
    `<span class="log-time">${new Date().toLocaleTimeString('tr-TR')}</span>` +
    `<span class="log-tag" style="color:${tagColor(dev)}">[${dev.toUpperCase()}]</span>` +
    `<span class="log-msg">${esc(msg)}</span>`;
  if (S.filter !== 'all' && S.filter !== dev) line.hidden = true;
  box.appendChild(line);
  while (box.children.length > MAX_LOG) box.removeChild(box.firstChild);
  box.scrollTop = box.scrollHeight;
}

// ============================ UI wiring ============================
function wireUI() {
  $$('.nav-item').forEach((n) => n.addEventListener('click', () => nav(n.dataset.page)));
  $$('[data-goto]').forEach((e) => e.addEventListener('click', () => nav(e.dataset.goto)));

  $('#btn-theme').addEventListener('click', toggleTheme);
  $('#btn-logout').addEventListener('click', async () => {
    try { await fetch('/api/session', { method: 'DELETE' }); } catch {}
    location.reload();
  });

  $$('[data-cmd]').forEach((b) => b.addEventListener('click', () => {
    const [dev, cmd] = b.dataset.cmd.split(':'); sendCmd(dev, cmd);
  }));
  $$('[data-ac-toggle]').forEach((b) => b.addEventListener('click', () => {
    const on = !!S.dev.salon.state.ac;
    sendCmd('salon', on ? 'ac_off' : 'ac_on', on ? {} : { temp: S.acTemp });
  }));
  $$('[data-ac-temp]').forEach((b) => b.addEventListener('click', () => {
    S.acTemp = Math.max(16, Math.min(30, S.acTemp + (+b.dataset.acTemp)));
    setField('salon-settemp', `${S.acTemp}°C`);
    if (S.dev.salon.state.ac) sendCmd('salon', 'ac_on', { temp: S.acTemp });
  }));
  $$('[data-light]').forEach((c) => c.addEventListener('change', () => sendCmd('kamera', c.checked ? 'light_on' : 'light_off')));
  $$('[data-stream-toggle]').forEach((b) => b.addEventListener('click', toggleStream));
  $$('[data-res],[data-fps]').forEach((s) => s.addEventListener('change', () => {
    if (S.stream.active) sendCmd('kamera', 'stream_set', {
      res: ($('[data-res]') || {}).value, fps: +(($('[data-fps]') || {}).value || 5),
    });
  }));

  $$('.chip').forEach((c) => c.addEventListener('click', () => {
    S.filter = c.dataset.filter;
    $$('.chip').forEach((x) => x.classList.toggle('active', x === c));
    $$('#log-box .log-line').forEach((l) => (l.hidden = S.filter !== 'all' && l.dataset.dev !== S.filter));
    const box = $('#log-box'); box.scrollTop = box.scrollHeight;
  }));
}

const PAGE_TITLES = { dashboard: 'Genel Bakış', kapi: 'Dış Kapı', kamera: 'Kamera', salon: 'Salon Klima', terminal: 'Sistem Terminali' };
function nav(page) {
  S.page = page;
  $$('.nav-item').forEach((n) => n.classList.toggle('active', n.dataset.page === page));
  $$('.page-section').forEach((s) => s.classList.toggle('active', s.id === 'page-' + page));
  $('#page-title').textContent = PAGE_TITLES[page] || page;
}

// ============================ theme ============================
function applyTheme() {
  const t = localStorage.getItem('theme') || (matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light');
  document.documentElement.setAttribute('data-theme', t);
  $('#theme-icon').textContent = t === 'dark' ? 'dark_mode' : 'light_mode';
}
function toggleTheme() {
  const t = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
  document.documentElement.setAttribute('data-theme', t);
  localStorage.setItem('theme', t);
  $('#theme-icon').textContent = t === 'dark' ? 'dark_mode' : 'light_mode';
}

// ============================ PWA ============================
if ('serviceWorker' in navigator) {
  addEventListener('load', () => navigator.serviceWorker.register('sw.js').catch(() => {}));
}

boot();
