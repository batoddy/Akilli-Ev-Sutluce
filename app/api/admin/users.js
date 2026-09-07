import { supabase } from '../../lib/supabase.js';
import { requireAuth } from '../../lib/session.js';

export default async function handler(req, res) {
  const sess = requireAuth(req, res);
  if (!sess) return;
  if (!sess.a) return res.status(403).json({ error: 'admin yetkisi gerekli' });

  if (req.method === 'GET') {
    const { data, error } = await supabase.from('users')
      .select('username, is_admin, disabled, created_at, last_login')
      .order('created_at', { ascending: true });
    if (error) return res.status(500).json({ error: error.message });
    return res.status(200).json({ users: data || [] });
  }

  if (req.method === 'POST') {
    const { username, password, is_admin } = req.body || {};
    if (!username || !password || String(password).length < 6) {
      return res.status(400).json({ error: 'kullanıcı adı ve en az 6 karakter şifre gerekli' });
    }
    const { error } = await supabase.rpc('app_create_user', {
      p_username: username, p_password: password, p_is_admin: !!is_admin,
    });
    if (error) return res.status(500).json({ error: error.message });
    return res.status(200).json({ ok: true });
  }

  if (req.method === 'PATCH') {
    const { username, disabled } = req.body || {};
    if (!username) return res.status(400).json({ error: 'username gerekli' });
    if (username === sess.u && disabled) {
      return res.status(400).json({ error: 'kendini devre dışı bırakamazsın' });
    }
    const { error } = await supabase.rpc('app_set_user_disabled', {
      p_username: username, p_disabled: !!disabled,
    });
    if (error) return res.status(500).json({ error: error.message });
    return res.status(200).json({ ok: true });
  }

  return res.status(405).json({ error: 'method not allowed' });
}
