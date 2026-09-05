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

  // 2. Gizli Çevre Değişkenleri ile HiveMQ Bağlantısı (TLS 8883)
  const brokerUrl = `mqtts://${process.env.MQTT_HOST}:8883`;
  
  const client = mqtt.connect(brokerUrl, {
    username: process.env.MQTT_USER,
    password: process.env.MQTT_PASSWORD,
    connectTimeout: 4000
  });

  return new Promise((resolve) => {
    client.on("connect", () => {
      client.publish(topic, String(state), { qos: 0 }, (err) => {
        client.end(true);
        if (err) {
          res.status(500).json({ error: "Mesaj gonderilemedi." });
        } else {
          res.status(200).json({ success: true, message: "Komut iletildi." });
        }
        resolve();
      });
    });

    client.on("error", (err) => {
      client.end(true);
      res.status(500).json({ error: "Broker baglanti hatasi." });
      resolve();
    });
  });
}