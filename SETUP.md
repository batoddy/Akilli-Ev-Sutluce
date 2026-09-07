# SETUP — Panel adımları (senin yapacakların)

Kod dışı, panellerden yapılacak işler. Sıra önemli. Yaptıkça `[x]` işaretle.
Toplanan değerleri en alttaki tabloya yaz — Faz 4'te Vercel'e girilecek.

---

## Faz 0 ✅
- [x] Vercel → Project → Settings → Build & Deployment → **Root Directory = `app`** → Save

---

## Faz 1 — Supabase + HiveMQ

### 1. Supabase şema
**Supabase Dashboard → SQL Editor'i kullan** (supabase.com/dashboard → projen → SQL Editor).
VS Code eklentisi (SQLTools vb.) çoklu komutu reddeder — "cannot insert multiple commands" hatası bu yüzden.

- [ ] Web SQL Editor'de: `app/db/01_schema.sql` → `02_functions.sql` → `03_seed.sql` (sırayla, Run)
- [ ] **VEYA** hepsi bir arada: `app/db/all_in_one.sql` (tek `DO` bloğu, tek komut — her araçtan geçer)
- [ ] Kontrol: Table Editor'de 8 tablo; `select id,name from devices;` → 3 satır.

### 2. İlk admin kullanıcı
- [ ] SQL Editor'de (kendi değerlerinle):
      ```sql
      select app_create_user('batuhan', 'BURAYA_GUCLU_PAROLA', true);
      ```
- [ ] Kontrol: `select username, is_admin, disabled from users;` → 1 satır.

### 3. Supabase anahtarlarını topla
- [ ] Settings → API → şu 3 değeri not et (alt tabloya):
  - `Project URL`  → `SUPABASE_URL`
  - `service_role` secret → `SUPABASE_SERVICE_ROLE_KEY`  *(gizli! sadece Vercel'e)*
  - `anon` public → `SUPABASE_ANON_KEY`

### 4. HiveMQ kullanıcıları
HiveMQ Cloud → cluster → **Access Management → Credentials**. 5 kimlik oluştur.
Her biri için güçlü, farklı parola (parola yöneticisi kullan). Mümkünse **Permissions**
(topic izinleri) tablodaki gibi kısıtla.

| Username | Permission — Publish | Permission — Subscribe |
|---|---|---|
| `esp-kapi`   | `ev/kapi/#`   | `ev/kapi/cmd`   |
| `esp-kamera` | `ev/kamera/#` | `ev/kamera/cmd` |
| `esp-salon`  | `ev/salon/#`  | `ev/salon/cmd`  |
| `srv-pub`    | `ev/+/cmd`    | *(yok)* |
| `web-sub`    | *(yok)*       | `ev/#` |

- [ ] 5 kimlik oluşturuldu
- [ ] Permission (ACL) ayarı yapılabildi mi?  ☐ Evet  ☐ Hayır (panelde böyle bir seçenek yok) → **bana söyle**
- [ ] Eski `batoddy` kimliğini **sil**
- [ ] Cluster **hostname**'ini not et (`xxxx.s1.eu.hivemq.cloud`)
- [ ] WebSocket portunu kontrol et (genelde **8884** TLS) → `HIVEMQ_WSS_PORT`

### 5. HiveMQ hostname + portlar
- [ ] Overview sayfasından: host, MQTT port (8883), WebSocket port (8884)

---

## Faz 4'te Vercel'e girilecek (şimdi sadece TOPLA)

| Env değişkeni | Değer | Kaynak |
|---|---|---|
| `SUPABASE_URL` | | Supabase → Settings → API |
| `SUPABASE_SERVICE_ROLE_KEY` | | aynı sayfa (gizli) |
| `SUPABASE_ANON_KEY` | | aynı sayfa |
| `HIVEMQ_HOST` | | HiveMQ Overview |
| `HIVEMQ_WSS_PORT` | `8884` | HiveMQ Overview |
| `HIVEMQ_PUB_USER` | `srv-pub` | — |
| `HIVEMQ_PUB_PASS` | | senin belirlediğin |
| `HIVEMQ_SUB_USER` | `web-sub` | — |
| `HIVEMQ_SUB_PASS` | | senin belirlediğin |
| `INGEST_SECRET` | | rastgele üret (aşağıya bak) |
| `SESSION_SECRET` | | rastgele üret |
| `CRON_SECRET` | | rastgele üret |

**Rastgele secret üretmek** (herhangi biri):
- Tarayıcı konsolu (F12): `crypto.randomUUID() + crypto.randomUUID()`
- Ya da: https://generate-secret.vercel.app/32

**ESP tarafı** (`esp32/include/secrets.h`, lokal — git'e girmez):
| Makro | Değer |
|---|---|
| `SECRET_WIFI_SSID` / `_PASS` | ev WiFi'n |
| `SECRET_MQTT_HOST` | `HIVEMQ_HOST` ile aynı |
| `SECRET_MQTT_USER` / `_PASS` | flash edilen role göre `esp-kapi/kamera/salon` |
| `SECRET_INGEST_URL` | `https://<vercel-domain>/api/ingest` |
| `SECRET_INGEST_TOKEN` | `INGEST_SECRET` ile **aynı** değer |
