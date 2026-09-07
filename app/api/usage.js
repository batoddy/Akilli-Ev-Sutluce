import { supabase } from '../lib/supabase.js';
import { requireAuth } from '../lib/session.js';

// Tarayıcı akış sırasında aldığı kare byte'larını periyodik olarak buraya yollar.
export default async function handler(req, res) {
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });
  if (!requireAuth(req, res)) return;

  const bytes = Math.round(Number((req.body || {}).bytes || 0));
  if (!Number.isFinite(bytes) || bytes <= 0 || bytes > 200_000_000) {
    return res.status(400).json({ error: 'geçersiz bytes' });
  }

  const day = new Date().toISOString().slice(0, 10);
  const { data, error } = await supabase.rpc('usage_add', {
    p_device: 'kamera', p_day: day, p_bytes: bytes,
  });
  if (error) {
    console.error('usage_add', error);
    return res.status(500).json({ error: error.message });
  }
  return res.status(200).json({ ok: true, day_bytes: data });
}
