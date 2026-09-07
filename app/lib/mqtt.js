import mqtt from 'mqtt';

// Serverless örnekleri sıcak kaldığında bağlantıyı tekrar kullan.
let client = null;
let connecting = null;

function connect() {
  if (client && client.connected) return Promise.resolve(client);
  if (connecting) return connecting;

  const host = process.env.HIVEMQ_HOST;
  const url = `mqtts://${host}:8883`;

  connecting = new Promise((resolve, reject) => {
    const c = mqtt.connect(url, {
      username: process.env.HIVEMQ_PUB_USER,
      password: process.env.HIVEMQ_PUB_PASS,
      protocolVersion: 4,
      clean: true,
      connectTimeout: 6000,
      reconnectPeriod: 0,
      clientId: `srv-pub-${Math.random().toString(16).slice(2, 10)}`,
    });
    const fail = (e) => { connecting = null; try { c.end(true); } catch {} reject(e); };
    c.once('connect', () => { client = c; connecting = null; resolve(c); });
    c.once('error', fail);
    setTimeout(() => { if (!client) fail(new Error('mqtt connect timeout')); }, 7000);
  });
  return connecting;
}

export async function publishCmd(topic, payload) {
  const c = await connect();
  return new Promise((resolve, reject) => {
    c.publish(topic, payload, { qos: 1 }, (err) => (err ? reject(err) : resolve()));
  });
}
