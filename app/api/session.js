import { supabase } from '../lib/supabase.js';
import {
  issueToken, getSession, setSessionCookie, clearSessionCookie, subCreds,
} from '../lib/session.js';

export default async function handler(req, res) {
  // --- Çıkış ---
  if (req.method === 'DELETE') {
    clearSessionCookie(res);
    return res.status(200).json({ ok: true });
  }

  // --- Oturum kontrolü + yenileme ---
  if (req.method === 'GET') {
    const s = getSession(req);
    if (!s) return res.status(401).json({ error: 'oturum yok' });
    setSessionCookie(res, issueToken(s.u, s.a));
    return res.status(200).json({ username: s.u, is_admin: s.a, mqtt: subCreds() });
  }

  // --- Giriş ---
  if (req.method === 'POST') {
    const { username, password } = req.body || {};
    if (!username || !password) return res.status(400).json({ error: 'kullanıcı adı ve şifre gerekli' });

    const { data, error } = await supabase.rpc('app_login', {
      p_username: username, p_password: password,
    });
    if (error) {
      console.error('app_login', error);
      return res.status(500).json({ error: 'sunucu hatası' });
    }
    if (!data || data.length === 0) return res.status(401).json({ error: 'hatalı kullanıcı adı veya şifre' });

    const row = data[0];
    await supabase.from('users')
      .update({ last_login: new Date().toISOString() })
      .eq('username', row.out_username);

    setSessionCookie(res, issueToken(row.out_username, row.out_is_admin));
    return res.status(200).json({
      username: row.out_username, is_admin: row.out_is_admin, mqtt: subCreds(),
    });
  }

  return res.status(405).json({ error: 'method not allowed' });
}
