import { supabase } from '../lib/supabase.js';
import { requireAuth } from '../lib/session.js';
import { publishCmd } from '../lib/mqtt.js';

const DEVICES = ['kapi', 'kamera', 'salon'];

export default async function handler(req, res) {
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });

  const sess = requireAuth(req, res);
  if (!sess) return;

  const { device, cmd, args } = req.body || {};
  if (!DEVICES.includes(device) || !cmd || typeof cmd !== 'string') {
    return res.status(400).json({ error: 'geçersiz cihaz veya komut' });
  }

  const id = `c-${Date.now()}-${Math.random().toString(16).slice(2, 6)}`;
  const payload = JSON.stringify({ id, cmd, args: args || {} });

  try {
    await publishCmd(`ev/${device}/cmd`, payload);
  } catch (e) {
    console.error('publishCmd', e);
    return res.status(502).json({ error: 'MQTT yayın hatası: ' + e.message });
  }

  const { error } = await supabase.from('commands').insert({
    id, device_id: device, cmd, args: args || {},
    source: 'web', user_name: sess.u, status: 'sent',
  });
  if (error) console.error('commands insert', error);

  return res.status(200).json({ id });
}
