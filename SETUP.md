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

- [ ] Web SQL Editor'de: `01_schema.sql` → `02_functions.sql` → `03_seed.sql` → `04_usage_fn.sql` (sırayla)
- [ ] **VEYA** hepsi bir arada: `app/db/all_in_one.sql` (tek `DO` bloğu — her araçtan geçer, usage_add dahil)
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
HiveMQ Cloud → cluster → **Access Management → Credentials**. Free tier'da sadece **Permission
Type** (yön) seçilebilir, topic bazlı ACL yok — sorun değil, `web-sub`'ın publish edememesi
asıl korumamız. 5 kimlik, her birine **farklı güçlü parola** (min 8):

| Username | Permission Type |
|---|---|
| `esp-kapi`   | Publish and Subscribe |
| `esp-kamera` | Publish and Subscribe |
| `esp-salon`  | Publish and Subscribe |
| `srv-pub`    | **Publish Only** |
| `web-sub`    | **Subscribe Only** |

- [ ] 5 kimlik oluşturuldu, parolalar not edildi
- [ ] Eski `batoddy` kimliğini **sil**
- [ ] Overview sayfasından **hostname** (`xxxx.s1.eu.hivemq.cloud`) ve WebSocket portu (8884) not et

---

## Faz 4 — Vercel ortam değişkenleri

Vercel → projen → **Settings → Environment Variables** → her satırı ekle (Production + Preview).
Ekledikten sonra **Deployments → son deployment → Redeploy**.

| Env değişkeni | Değer | Kaynak |
|---|---|---|
| `SUPABASE_URL` | | Supabase → Settings → API |
| `SUPABASE_SERVICE_ROLE_KEY` | | aynı sayfa (gizli) |
| `SUPABASE_ANON_KEY` | | aynı sayfa |
| `HIVEMQ_HOST` | | HiveMQ Overview (`xxxx.s1.eu.hivemq.cloud`) |
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

**Test (env girip redeploy sonrası):**
- `https://<domain>/api/session` → GET → `{"error":"oturum yok"}` dönmeli (401 = endpoint çalışıyor)
- `https://<domain>/api/ingest` → POST `{"secret":"yanlis","kind":"log","device":"kapi"}` → 401 dönmeli

**ESP tarafı** (`esp32/include/secrets.h`, lokal — git'e girmez):
| Makro | Değer |
|---|---|
| `SECRET_WIFI_SSID` / `_PASS` | ev WiFi'n |
| `SECRET_MQTT_HOST` | `HIVEMQ_HOST` ile aynı |
| `SECRET_MQTT_USER` / `_PASS` | flash edilen role göre `esp-kapi/kamera/salon` |
| `SECRET_INGEST_URL` | `https://<vercel-domain>/api/ingest` |
| `SECRET_INGEST_TOKEN` | `INGEST_SECRET` ile **aynı** değer |
