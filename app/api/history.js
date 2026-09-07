import { supabase } from '../lib/supabase.js';
import { requireAuth } from '../lib/session.js';

export default async function handler(req, res) {
  if (req.method !== 'GET') return res.status(405).json({ error: 'method not allowed' });
  if (!requireAuth(req, res)) return;

  const device = req.query.device || null;
  const limit = Math.min(parseInt(req.query.limit, 10) || 50, 200);

  const today = new Date().toISOString().slice(0, 10);          // YYYY-MM-DD
  const monthStart = today.slice(0, 8) + '01';

  let eventsQ = supabase.from('events').select('*')
    .order('created_at', { ascending: false }).limit(limit);
  if (device) eventsQ = eventsQ.eq('device_id', device);

  let telQ = supabase.from('telemetry').select('*')
    .order('created_at', { ascending: false }).limit(30);
  if (device) telQ = telQ.eq('device_id', device);

  const [states, events, telemetry, uToday, uMonth] = await Promise.all([
    supabase.from('device_state').select('*'),
    eventsQ,
    telQ,
    supabase.from('usage_daily').select('bytes').gte('day', today),
    supabase.from('usage_daily').select('bytes').gte('day', monthStart),
  ]);

  const sum = (r) => (r.data || []).reduce((a, x) => a + Number(x.bytes || 0), 0);

  return res.status(200).json({
    states: states.data || [],
    events: events.data || [],
    telemetry: telemetry.data || [],
    usage_today: sum(uToday),
    usage_month: sum(uMonth),
  });
}
