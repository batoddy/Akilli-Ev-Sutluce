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

  // 2. Broker URL ve İstemci Yapılandırması
  const cleanHost = (process.env.MQTT_HOST || "").replace(/^mqtts?:\/\//, "").replace(/:8883$/, "");
  const brokerUrl = `mqtts://${cleanHost}:8883`;

  return new Promise((resolve) => {
    let resolved = false;

    // Güvenlik zaman aşımı: 6 saniyede bağlantı tamamlanmazsa fonksiyonu zorla kapat
    const timer = setTimeout(() => {
      if (!resolved) {
        resolved = true;
        if (client) client.end(true);
        res.status(504).json({ error: "HiveMQ baglanti zaman asimi (Timeout)!" });
        resolve();
      }
    }, 6000);

    const client = mqtt.connect(brokerUrl, {
      username: process.env.MQTT_USER,
      password: process.env.MQTT_PASSWORD,
      clientId: `vercel_${Math.random().toString(16).substring(2, 8)}`,
      connectTimeout: 5000,
      rejectUnauthorized: false // Sunucu sertifika doğrulama takılmalarını önler
    });

    client.on("connect", () => {
      client.publish(topic, String(state), { qos: 0 }, (err) => {
        if (!resolved) {
          resolved = true;
          clearTimeout(timer);
          client.end(true);

          if (err) {
            res.status(500).json({ error: "Mesaj gonderilemedi: " + err.message });
          } else {
            res.status(200).json({ success: true, message: "Komut iletildi." });
          }
          resolve();
        }
      });
    });

    client.on("error", (err) => {
      if (!resolved) {
        resolved = true;
        clearTimeout(timer);
        client.end(true);
        res.status(500).json({ error: "Broker baglanti hatasi: " + err.message });
        resolve();
      }
    });
  });
}