import crypto from 'crypto';

const SECRET  = process.env.SESSION_SECRET || '';
const COOKIE  = 'sev_session';
const MAX_AGE = 30 * 24 * 3600;            // 30 gün (saniye)

function hmac(body) {
  return crypto.createHmac('sha256', SECRET).update(body).digest('base64url');
}

export function issueToken(username, isAdmin) {
  const payload = { u: username, a: !!isAdmin, exp: Math.floor(Date.now() / 1000) + MAX_AGE };
  const body = Buffer.from(JSON.stringify(payload)).toString('base64url');
  return `${body}.${hmac(body)}`;
}

export function verifyToken(token) {
  if (!token || token.indexOf('.') < 0) return null;
  const [body, sig] = token.split('.');
  const expect = hmac(body);
  if (sig.length !== expect.length) return null;
  if (!crypto.timingSafeEqual(Buffer.from(sig), Buffer.from(expect))) return null;
  let p;
  try { p = JSON.parse(Buffer.from(body, 'base64url').toString()); } catch { return null; }
  if (!p.exp || p.exp < Math.floor(Date.now() / 1000)) return null;
  return p;
}

export function parseCookies(req) {
  const out = {};
  const raw = req.headers.cookie || '';
  for (const part of raw.split(';')) {
    const i = part.indexOf('=');
    if (i < 0) continue;
    out[part.slice(0, i).trim()] = decodeURIComponent(part.slice(i + 1).trim());
  }
  return out;
}

export function getSession(req) {
  return verifyToken(parseCookies(req)[COOKIE]);
}

export function setSessionCookie(res, token) {
  res.setHeader('Set-Cookie',
    `${COOKIE}=${token}; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=${MAX_AGE}`);
}

export function clearSessionCookie(res) {
  res.setHeader('Set-Cookie',
    `${COOKIE}=; HttpOnly; Secure; SameSite=Lax; Path=/; Max-Age=0`);
}

// Handler başında çağır: yetki yoksa 401 yazar ve null döner.
export function requireAuth(req, res) {
  const s = getSession(req);
  if (!s) { res.status(401).json({ error: 'oturum yok' }); return null; }
  return s;
}

// Tarayıcının HiveMQ'ya WSS ile bağlanması için salt-okunur kimlik.
export function subCreds() {
  const port = process.env.HIVEMQ_WSS_PORT || '8884';
  return {
    url: `wss://${process.env.HIVEMQ_HOST}:${port}/mqtt`,
    username: process.env.HIVEMQ_SUB_USER || '',
    password: process.env.HIVEMQ_SUB_PASS || '',
  };
}
