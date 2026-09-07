import { supabase } from '../../lib/supabase.js';

// Vercel Cron günde 1 çağırır (Authorization: Bearer <CRON_SECRET> otomatik gelir).
export default async function handler(req, res) {
  const auth = req.headers.authorization || '';
  if (!process.env.CRON_SECRET || auth !== `Bearer ${process.env.CRON_SECRET}`) {
    return res.status(401).json({ error: 'yetkisiz' });
  }
  const { error } = await supabase.rpc('cleanup_old_data');
  if (error) {
    console.error('cleanup_old_data', error);
    return res.status(500).json({ error: error.message });
  }
  return res.status(200).json({ ok: true, at: new Date().toISOString() });
}
