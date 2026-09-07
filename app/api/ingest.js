import { supabase } from '../lib/supabase.js';

// ESP'ler buraya POST eder: {secret, kind, device, ts, data}
// kind: event | state | telemetry | command_ack | log
export default async function handler(req, res) {
  if (req.method !== 'POST') return res.status(405).json({ error: 'method not allowed' });

  const { secret, kind, device, ts, data } = req.body || {};
  if (!secret || secret !== process.env.INGEST_SECRET) {
    return res.status(401).json({ error: 'yetkisiz' });
  }
  if (!kind || !device) return res.status(400).json({ error: 'kind ve device gerekli' });

  const iso = Number.isFinite(ts) && ts > 1e9
    ? new Date(ts * 1000).toISOString()
    : new Date().toISOString();

  try {
    if (kind === 'event') {
      await supabase.from('events').insert({
        device_id: device, type: (data && data.type) || 'unknown', payload: data || {}, created_at: iso,
      });
      await supabase.from('devices').update({ last_seen: iso, online: true }).eq('id', device);

    } else if (kind === 'state') {
      await supabase.from('device_state').upsert({ device_id: device, snapshot: data || {}, updated_at: iso });
      await supabase.from('devices').update({ last_seen: iso, online: true }).eq('id', device);

    } else if (kind === 'telemetry') {
      await supabase.from('telemetry').insert({
        device_id: device,
        rssi: data && data.rssi, heap_free: data && data.heap, uptime_s: data && data.uptime,
        created_at: iso,
      });
      await supabase.from('devices').update({ last_seen: iso, online: true }).eq('id', device);

    } else if (kind === 'command_ack') {
      if (!data || !data.id) return res.status(400).json({ error: 'ack id yok' });
      await supabase.from('commands').update({
        status: data.result === 'ok' ? 'ack_ok' : 'ack_error',
        detail: data.detail ?? null,
        acked_at: iso,
      }).eq('id', data.id);

    } else if (kind === 'log') {
      await supabase.from('logs').insert({
        device_id: device, level: (data && data.lvl) || 'INFO', message: (data && data.msg) || '', created_at: iso,
      });

    } else {
      return res.status(400).json({ error: 'bilinmeyen kind: ' + kind });
    }
  } catch (e) {
    console.error('ingest', kind, e);
    return res.status(500).json({ error: e.message });
  }

  return res.status(200).json({ ok: true });
}
