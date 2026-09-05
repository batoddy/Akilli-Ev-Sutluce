import mqtt from "mqtt";

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Sadece POST kabul edilir." });
  }

  // 1. PIN Doğrulaması
  const clientPin = req.headers["x-secret-pin"];
  if (!clientPin || clientPin !== process.env.MY_SECRET_PIN) {
    return res.status(401).json({ error: "Gecersiz veya eksik PIN!" });
  }

  const { topic, state } = req.body;
  if (!topic || state === undefined) {
    return res.status(400).json({ error: "Topic ve state alanlari zorunludur." });
  }

  // 2. Host Temizleme (protokol veya port kalıntılarını ayıklar)
  const cleanHost = (process.env.MQTT_HOST || "")
    .replace(/^mqtts?:\/\//, "")
    .replace(/^wss?:\/\//, "")
    .replace(/:[0-9]+$/, "")
    .trim();

  const brokerUrl = `mqtts://${cleanHost}:8883`;

  return new Promise((resolve) => {
    let isHandled = false;

    // 7 saniyelik güvenlik zaman aşımı
    const timer = setTimeout(() => {
      if (!isHandled) {
        isHandled = true;
        if (client) client.end(true);
        res.status(504).json({ error: "HiveMQ zaman asimi (Timeout)!" });
        resolve();
      }
    }, 7000);

    const client = mqtt.connect(brokerUrl, {
      username: process.env.MQTT_USER,
      password: process.env.MQTT_PASSWORD,
      clientId: `vercel_sender_${Date.now()}_${Math.random().toString(16).substring(2, 6)}`,
      connectTimeout: 5000,
      clean: true,
      rejectUnauthorized: false
    });

    client.on("connect", () => {
      // QoS 1: Broker'dan onay paketi (PUBACK) gelmeden bağlantıyı kesmez
      client.publish(topic, String(state), { qos: 1 }, (err) => {
        if (!isHandled) {
          isHandled = true;
          clearTimeout(timer);

          // Soketi zorla değil, tampon boşalınca nazikçe kapatıyoruz
          client.end(false, () => {
            if (err) {
              res.status(500).json({ error: "Yayinlama hatasi: " + err.message });
            } else {
              res.status(200).json({ success: true, message: "Komut HiveMQ'ya teslim edildi." });
            }
            resolve();
          });
        }
      });
    });

    client.on("error", (err) => {
      if (!isHandled) {
        isHandled = true;
        clearTimeout(timer);
        client.end(true);
        res.status(500).json({ error: "Broker baglanti hatasi: " + err.message });
        resolve();
      }
    });
  });
}