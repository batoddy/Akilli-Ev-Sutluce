import mqtt from "mqtt";
import { createClient } from "@supabase/supabase-js";

// Supabase Bağlantısı
const supabaseUrl = process.env.SUPABASE_URL || process.env.NEXT_PUBLIC_SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_SERVICE_ROLE_KEY || process.env.SUPABASE_ANON_KEY || process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY;
const supabase = createClient(supabaseUrl, supabaseKey);

export default async function handler(req, res) {
  if (req.method !== "POST") {
    return res.status(405).json({ error: "Sadece POST kabul edilir." });
  }

  // Arayüzden gelen parametreleri yakala
  const { pin, device, command } = req.body;
  
  if (pin !== "1234") {
    return res.status(401).json({ error: "Gecersiz veya eksik PIN!" });
  }
  if (!device || !command) {
    return res.status(400).json({ error: "Cihaz ve komut alanlari zorunludur." });
  }

  // Host Temizleme
  const cleanHost = (process.env.MQTT_URL || process.env.MQTT_HOST || "")
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
      const topic = `ev/${device}/komut`;
      
      client.publish(topic, String(command), { qos: 1 }, async (err) => {
        if (!isHandled) {
          isHandled = true;
          clearTimeout(timer);

          client.end(false, async () => {
            if (err) {
              res.status(500).json({ error: "Yayinlama hatasi: " + err.message });
            } else {
              // VERİTABANINA YAZMA (Senin kodunda eksik olan kısım)
              await supabase.from('cihaz_durumlari').upsert({ 
                  cihaz_id: device, 
                  son_komut: command, 
                  son_guncelleme: new Date().toISOString() 
              });
              await supabase.from('sistem_loglari').insert([{ cihaz_id: device, aksiyon: command }]);

              res.status(200).json({ success: true, message: "Komut iletildi ve loglandı." });
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