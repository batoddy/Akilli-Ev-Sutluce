# Sütlüce Akıllı Ev — Mimari & Karar Dokümanı

> Bu dosya projenin **tek referans noktası**. Karar değişince burası güncellenir.
> Son güncelleme: 2026-09-07

---

## 1. Amaç

ESP32 tabanlı, **modüler / dinamik / basit** bir akıllı ev sistemi. 3 ESP cihazı, bir PWA arayüz,
HiveMQ (MQTT) realtime omurga, Supabase kalıcı depo, Vercel statik hosting + serverless API.
Tüm servisler **free tier** — veriye cimri değil ama savurgan da değiliz.

---

## 2. Kararlar (kilitli)

| # | Konu | Karar |
|---|---|---|
| 1 | Uzaktan erişim | ESP32 dışında **7/24 açık cihaz yok**. Kamera akışı **MQTT üzerinden JPEG kare**. Varsayılan **VGA 640×480 (~480p) @ 5 fps**; çözünürlük ve fps **arayüzden canlı değiştirilebilir** (240p–768p, 2–10 fps). 180 sn hareketsizlikte otomatik dur. Opsiyonel bonus: DuckDNS + modem port yönlendirme (ISS gerçek IPv4 veriyorsa). İleride yardımcı cihaz eklenince Cloudflare Tunnel / Tailscale. |
| 2 | Kamera kullanım takibi | Dashboard'da kameranın altında **günlük + aylık kullanım barı**. Tarayıcı aldığı kare byte'larını sayar → `/api/usage` → `usage` tablosu. Bar, tahmini toplam trafiği (kayıtlı × ~2) HiveMQ 10 GB/ay limitine göre gösterir. |
| 3 | ESP → veritabanı yazımı | **Vercel `/api/ingest` aracısı**. Supabase `service_role` sadece sunucuda; ESP `INGEST_SECRET` ile POST atar. |
| 4 | Telemetri | Canlı gösterge: 60 sn MQTT (DB'ye yazılmaz). Geçmiş/grafik: 5 dk `/api/ingest` → `telemetry` tablosu. Retention 30 gün. |
| 5 | Arayüz yapısı | Düz statik dosyalara böl (`index.html` + `styles.css` + `app.js` + CDN kütüphaneleri). **Framework yok, build yok, npm/npx kurulumu yok.** |
| 6 | Kapı tetikleme | Sadece web. Fiziksel buton yok. 2 sn röle pulse'ı **firmware** yönetir. |
| 7 | MQTT topic prefix | `ev/` kalıyor. |
| 8 | Kamera düğümü donanımı | **Tek ESP32-CAM** (AI-Thinker). Kamera + **PIR hareket sensörü** + ışık. **RFID YOK.** SD kart yok. USB seri korunur (loglar hem seri hem MQTT). Işık = kart üstü flash LED (GPIO4) veya oradan sürülen harici röle. Pin haritası §9.1. |
| 9 | Repo | **Public.** Faz 0'da `git filter-repo` ile `.env*` geçmişten temizlenir + tüm sırlar rotate edilir. |
| 10 | Komut yolu | Tarayıcı canlı veriyi HiveMQ'dan **salt-okunur** kimlikle dinler. Komutlar `/api/command` (cookie ile doğrulanmış) üzerinden sunucudan yayınlanır. Komut gecikmesi ~1–2 sn. |
| 11 | Auth | **Kullanıcı adı + şifre.** Admin (sen) kullanıcıları oluşturur (`users` tablosu, hash'li şifre). Giriş `/api/session` → imzalı `HttpOnly` cookie (30 gün, her ziyarette yenilenir). Google/e-posta yok. |

---

## 3. Mimari

```
┌───────────┐   WSS (8884) salt-okunur   ┌─────────────┐  TLS (8883)   ┌──────────────────┐
│ Tarayıcı  │ ◄───────────────────────── │   HiveMQ    │ ◄───────────► │  3x ESP32        │
│  (PWA)    │  state / event / log /     │   Cloud     │               │  kapi / kamera / │
│           │  telemetry / ack /         │  (free)     │               │  salon           │
│           │  kamera kareleri           └─────────────┘               └────────┬─────────┘
│           │                                   ▲                               │ HTTPS POST
│           │  komut (HTTPS, cookie)            │ publish ev/<dev>/cmd          ▼ (olaylar +
│           ▼                                   │ (sunucudan)            ┌──────────────┐  5dk telemetri)
│     ┌──────────────┐                          │                       │ /api/ingest  │──► Supabase
│     │ /api/command │ ─────────────────────────┘                       │ (INGEST_     │   (service_role
│     │ (cookie auth)│ ──► commands tablosu                             │  SECRET)     │    sunucuda)
│     └──────────────┘                                                  └──────────────┘
│
│ HTTPS
▼
┌────────────────────┐  ┌────────────────┐  ┌──────────────────────┐
│ /api/history (GET) │  │ /api/session   │  │ /api/cron/cleanup    │ ──► Supabase
│ /api/usage  (POST) │  │ (kul.adı+şifre) │  │ (günde 1, retention) │
└────────────────────┘  └────────────────┘  └──────────────────────┘
```

**Realtime (okuma):** Tarayıcı `mqtt.js` (WSS) ile HiveMQ'ya salt-okunur kimlikle bağlanır,
`ev/#`'e abone olur. Canlı state / event / log / telemetry / ack / kamera kareleri buradan gelir.

**Komut (yazma):** Tarayıcı → `/api/command` (cookie auth) → sunucu `ev/<dev>/cmd`'e yayınlar +
`commands` satırı yazar. ACK yine salt-okunur MQTT aboneliğinden (`ev/<dev>/cmd/ack`) gelir.

**Kalıcılık:** ESP'ler önemli olayları `/api/ingest`'e POST eder → Supabase. Rutin debug logları
sadece MQTT'de kalır.

**Geçmiş:** Sayfa açılışında `/api/history` ile Supabase'den son kayıtlar; sonrası canlı MQTT.

---

## 4. Cihazlar / roller

| Cihaz ID | Fiziksel yer | Görev | Donanım |
|---|---|---|---|
| `kapi`   | Dış kapının **içi** (diafon yanı) | Kapı-açma rölesini 2 sn tetikle | ESP32 DevKit + röle modülü |
| `kamera` | Dış kapının **önü** | On-demand kamera akışı, hareket (PIR), ışık | ESP32-CAM (AI-Thinker) + PIR + ışık (flash LED / röle) |
| `salon`  | Salon | Klimaya IR aç/kapa | ESP32 DevKit + IR LED (+ ops. DHT22) |

---

## 5. MQTT sözleşmesi

Prefix `ev/`. Cihazlar: `kapi`, `kamera`, `salon`. Payload = JSON (kamera kareleri hariç — binary).

| Topic | Yön | Retained | QoS | Payload |
|---|---|---|---|---|
| `ev/<dev>/cmd`        | sunucu→esp | hayır | 1 | `{"id":"c-1699","cmd":"open","args":{}}` |
| `ev/<dev>/cmd/ack`    | esp→tarayıcı | hayır | 1 | `{"id":"c-1699","cmd":"open","result":"ok","detail":null,"ts":1699...}` |
| `ev/<dev>/state`      | esp→tarayıcı | **evet** | 1 | tam durum snapshot'ı (§5.2) |
| `ev/<dev>/event`      | esp→tarayıcı | hayır | 1 | `{"type":"motion","ts":...}` (§5.3) |
| `ev/<dev>/log`        | esp→tarayıcı | hayır | 0 | `{"lvl":"INFO","msg":"WiFi bağlandı 192.168.1.42"}` |
| `ev/<dev>/telemetry`  | esp→tarayıcı | **evet** | 0 | `{"rssi":-63,"heap":181000,"uptime":3600,"ip":"192.168.1.42"}` |
| `ev/<dev>/status`     | esp→tarayıcı (LWT) | **evet** | 1 | `"online"` / `"offline"` |
| `ev/kamera/stream`    | esp→tarayıcı | hayır | 0 | **binary JPEG** (bir kare) |

- Her ESP **sadece** `ev/<kendi>/cmd` dinler.
- Tarayıcı `ev/#` dinler; `ev/kamera/stream`'e sadece kamera sayfası açıkken abone olur.
- Tarayıcı hiçbir topic'e **publish etmez** — komutlar `/api/command` üzerinden.
- Data tasarrufu: log QoS 0 + retain yok; ESP'de saniyede maks ~5 mesaj rate-limit;
  log seviyesi `cmd: set_log_level` ile uzaktan ayarlanır.

### 5.1 Komut listesi

| Cihaz | `cmd` | `args` | Etki |
|---|---|---|---|
| ortak | `get_state` | — | `state` yeniden yayınla |
| ortak | `reboot` | — | ESP.restart() |
| ortak | `set_log_level` | `{"level":"DEBUG\|INFO\|WARN\|ERROR"}` | log filtresi |
| kapi | `open` | — | röle 2000 ms ON → OFF |
| kamera | `stream_start` | `{"res":"vga","fps":5}` | kare yayınına başla |
| kamera | `stream_set` | `{"res":"svga","fps":8}` | **yayın sürerken** çözünürlük/fps değiştir |
| kamera | `stream_stop` | — | yayını durdur |
| kamera | `snapshot` | — | tek kare yayınla |
| kamera | `light_on` / `light_off` | — | ışık (GPIO4) |
| salon | `ac_on` / `ac_off` | — | IR gönder |
| salon | `ac_set` *(ileride)* | `{"temp":24,"mode":"cool","fan":"auto"}` | IR gönder |

`res` değerleri: `qvga` (320×240 ~240p) · `hvga` (480×320 ~320p) · `vga` (640×480 ~480p) ·
`svga` (800×600 ~600p) · `xga` (1024×768 ~768p).

### 5.2 `state` snapshot örnekleri

```jsonc
// ev/kapi/state
{ "online": true, "relay": false, "last_open_ts": 1699000000, "fw": "1.0.0" }

// ev/kamera/state
{ "online": true, "light": false, "streaming": false, "res": "vga", "fps": 0,
  "last_motion_ts": 1699000000, "fw": "1.0.0" }

// ev/salon/state
{ "online": true, "ac": false, "temp": 26.4, "hum": 48, "fw": "1.0.0" }
```

### 5.3 `event` tipleri

| `type` | Ek alanlar | Kaynak |
|---|---|---|
| `motion` | — | kamera PIR |
| `door_open` | `source:"web"`, `user` | kapi |
| `stream` | `action:"start"\|"stop"`, `reason`, `res`, `fps` | kamera |
| `boot` | `fw`, `reset_reason` | ortak |

---

## 6. Kamera akışı (MQTT kare yöntemi)

Akış: `/api/command` → `stream_start` → ESP32-CAM kamerayı başlatır → her kareyi
`ev/kamera/stream`'e **binary JPEG** yayınlar (QoS 0, retain yok) → tarayıcı `Blob` +
`URL.createObjectURL` ile `<img>`'e basar, önceki URL'yi `revoke` eder → 180 sn hareketsizlikte
ESP otomatik `stream_stop`. Kullanıcı arayüzden `res`/`fps`'i canlı değiştirir → `stream_set`.

### 6.1 HiveMQ free bütçe

- Limit: **10 GB / ay toplam trafik**. Bir kare hem ESP→broker hem broker→tarayıcı sayılır → **×2**.
  İkinci eş zamanlı izleyici → +%50. **Tek izleyici hedefliyoruz.**
- Telemetri + log + komut trafiği: aylık ~0.3–0.5 GB. Kameraya ~9 GB pay.
- Değerler ~9 GB'a göre; kare boyutları sahne detayına göre ±%20.

| Çözünürlük | ≈p | ~kare | 3 fps | **5 fps** | 8 fps |
|---|---|---|---|---|---|
| QVGA 320×240 | 240p | ~12 KB | ~72 dk/gün · 36 s/ay | ~40 dk/gün · 20 s/ay | ~25 dk/gün · 12 s/ay |
| HVGA 480×320 | 320p | ~20 KB | ~43 dk/gün | ~26 dk/gün | ~16 dk/gün |
| **VGA 640×480** | **480p** | ~35 KB | ~24 dk/gün · 12 s/ay | **~15 dk/gün · 7 s/ay** | ~9 dk/gün · 4.5 s/ay |
| SVGA 800×600 | 600p | ~50 KB | ~17 dk/gün | ~10 dk/gün | ~6 dk/gün |
| XGA 1024×768 | 768p | ~90 KB | ~9 dk/gün | ~6 dk/gün | ~3.5 dk/gün |

> "dk/gün" = ayı eşit bölersek günlük ortalama; "s/ay" = aylık toplam saat. Aylık toplam limittir,
> boş gün ertesine devretmez.

**Varsayılan: VGA 480p @ 5 fps** → günde ~15 dk / ayda ~7 saat. Kapıda kısa bakışlar için yeterli.
Daha akıcı istersen fps'i, daha net istersen çözünürlüğü arayüzden yükseltirsin — ama bütçe
tablosundan takip et (yüksek ayarda ay çabuk biter).

### 6.2 Kullanım takibi (Dashboard barı — Karar #2)

- Tarayıcı akış sırasında aldığı her kare Blob'unun byte'ını toplar.
- Her 30 sn'de bir ve akış bitince `/api/usage` POST `{bytes}` → `usage` tablosuna
  `(device_id, day)` bazında **artımlı** yazılır (`on conflict do update set bytes = bytes + excluded`).
- Dashboard `/api/history`'den `usage_today` + `usage_month` alır.
- İki bar: **Bugün** (referans ~0.3 GB/gün) ve **Bu ay** (`kayıtlı × 2` ≈ tahmini toplam trafik,
  10 GB'a göre). %80'de sarı, %100'de kırmızı + akış başlatmayı uyar.
- Kesin rakam HiveMQ panelidir; bu bar yaklaşık göstergedir.

### 6.3 Opsiyonel bonus: DuckDNS + port yönlendirme
Yardımcı cihaz gerektirmez. ESP32-CAM MJPEG sunucusunu (port 81) açar; modemde dış port → ESP
LAN IP:81; DuckDNS ile sabit ad. **Şart:** ISS gerçek IPv4 vermeli (CGNAT ise çalışmaz — modem WAN
IP'si `whatismyip` ile aynı olmalı). Güvenlik: URL'de rastgele token; kapalıyken port kapalı.

### 6.4 Firmware riski
ESP32-CAM'de PubSubClient ile büyük binary publish sorunlu olabilir → `setBufferSize(20480)` şart
(SVGA/XGA için ~60–90 KB kare → daha büyük buffer veya parçalı gönderim), gerekirse kamera düğümü
`AsyncMqttClient`'a geçer. **En riskli parça** — önce kamera + MQTT kare akışı prototiplenir.

---

## 7. Supabase şema

```sql
create table devices (
  id text primary key,              -- 'kapi' | 'kamera' | 'salon'
  name text not null, kind text not null,
  fw_version text, last_seen timestamptz, online boolean default false
);

create table device_state (
  device_id text primary key references devices(id),
  snapshot jsonb not null,
  updated_at timestamptz not null default now()
);

create table events (
  id bigserial primary key,
  device_id text not null, type text not null, payload jsonb,
  created_at timestamptz not null default now()
);
create index on events (device_id, created_at desc);

create table commands (
  id bigserial primary key,
  device_id text not null, cmd text not null, args jsonb,
  source text, user_name text,
  status text default 'sent',       -- 'sent'|'ack_ok'|'ack_error'|'timeout'
  created_at timestamptz not null default now(), acked_at timestamptz
);
create index on commands (device_id, created_at desc);

create table logs (
  id bigserial primary key,
  device_id text not null, level text not null, message text not null,
  created_at timestamptz not null default now()
);
create index on logs (device_id, created_at desc);

create table telemetry (
  id bigserial primary key,
  device_id text not null, rssi int, heap_free int, uptime_s bigint,
  created_at timestamptz not null default now()
);
create index on telemetry (device_id, created_at desc);

create table usage (
  device_id text not null,
  day date not null,
  bytes bigint not null default 0,
  primary key (device_id, day)
);

create table users (
  username text primary key,
  password_hash text not null,      -- scrypt (Node crypto), tuz dahil
  is_admin boolean default false,
  disabled boolean default false,
  created_at timestamptz not null default now(),
  last_login timestamptz
);
```

### 7.1 RLS
Tüm tablolarda RLS **açık**, `anon` için politika **yok** → istemci doğrudan erişemez.
Tüm okuma/yazma Vercel API üzerinden `service_role` ile (RLS bypass). `users.password_hash`
hiçbir zaman API yanıtında dönmez.

### 7.2 Ne saklanır
| Veri | MQTT | Supabase |
|---|---|---|
| Komut + ACK | evet | `commands` (hepsi) |
| Olaylar (motion/door/stream/boot) | evet | `events` (hepsi) — **asıl değerli veri** |
| State snapshot | evet (retained) | `device_state` (cihaz başına 1 satır, upsert) |
| Rutin DEBUG/INFO log | evet | **hayır** |
| WARN/ERROR log + günde 1 heartbeat | evet | `logs` |
| Telemetri (canlı) | evet 60 sn | **hayır** |
| Telemetri (geçmiş) | — | `telemetry` 5 dk |
| Kamera kullanımı | — | `usage` (gün bazında artımlı) |

### 7.3 Retention (`/api/cron/cleanup`, Vercel Cron, günde 1 — `0 4 * * *`)
`events` > 90g · `commands` > 60g · `logs` > 30g · `telemetry` > 30g · `usage` > 400g → sil.

### 7.4 Boyut
Günlük ~1.000 satır × ~200 B ≈ 200 KB/gün → 90 günde ~18 MB. 500 MB limitin çok altında.
(Supabase free 7 gün inaktivitede durur — günlük yazım engeller.)

---

## 8. Vercel API

| Endpoint | Metot | İş |
|---|---|---|
| `/api/session` | POST | `{username,password}` → `users` doğrula (scrypt) → `HttpOnly; Secure; SameSite=Lax` imzalı cookie (30g) + tarayıcıya **salt-okunur** HiveMQ WSS kimliği. GET → cookie geçerli mi + kimlik yenile. DELETE → çıkış. |
| `/api/command` | POST | Cookie doğrula → `ev/<dev>/cmd`'e publish + `commands` satırı (`user_name`). HiveMQ **publish** kimliği sunucuda. |
| `/api/ingest` | POST | ESP'den `{secret,kind,device,data,ts}` → Supabase. `secret != INGEST_SECRET` → 401. |
| `/api/history` | GET | Cookie doğrula → `?device=&limit=` → `events` + `device_state` + son `telemetry` + `usage_today`/`usage_month`. |
| `/api/usage` | POST | Cookie doğrula → `{bytes}` → `usage` artımlı upsert. |
| `/api/admin/users` | GET/POST/PATCH | Cookie + `is_admin` → kullanıcı listele / ekle (şifre hash'le) / disable. |
| `/api/cron/cleanup` | GET (Cron) | §7.3. `CRON_SECRET` korumalı. |

- Eski `/api/control.js` → `/api/command`. Eski `/api/status.js` → sil.
- İlk admin: `scripts/adduser.mjs` (lokal Node script; kullanıcı adı + şifre sorar, scrypt hash'ler,
  `users` tablosuna `is_admin=true` yazar). Sonraki kullanıcıları admin arayüzden veya scriptle ekle.
- Env: `SUPABASE_URL`, `SUPABASE_SERVICE_ROLE_KEY`, `INGEST_SECRET`, `CRON_SECRET`, `SESSION_SECRET`,
  `HIVEMQ_HOST`, `HIVEMQ_PUB_USER`, `HIVEMQ_PUB_PASS` (sunucu publish),
  `HIVEMQ_SUB_USER`, `HIVEMQ_SUB_PASS` (salt-okunur, tarayıcıya verilir).
- `mqtt` paketi sadece `/api/command` + `/api/ingest` için; client modül kapsamında cache'lenir.

### 8.1 HiveMQ kullanıcıları
| Kullanıcı | Kim | Yetki |
|---|---|---|
| `esp-kapi` / `esp-kamera` / `esp-salon` | ESP'ler | publish `ev/<self>/#`, subscribe `ev/<self>/cmd` |
| `srv-pub` | Vercel `/api/command`,`/api/ingest` | publish `ev/+/cmd` |
| `web-sub` | Tarayıcı (salt-okunur) | subscribe `ev/#` — publish YOK |

*(HiveMQ Cloud free ACL sınırlıysa tek kullanıcıyla başlanır; komut yolu yine sunucudan geçtiği
için tarayıcı kimliği düşük riskli kalır.)*

### 8.2 Auth akışı (kullanıcı adı + şifre)
1. `/login` sayfası → kullanıcı adı + şifre → `POST /api/session`.
2. Sunucu `users`'tan çeker, `crypto.scrypt` ile doğrular, `disabled` değilse
   HMAC-imzalı token `{username, exp}` → `HttpOnly` cookie (30 gün, her `GET /api/session`'da yenilenir).
3. Cookie yoksa/expired → arayüz `/login`'e yönlendirir.
4. Şifre saklama: `scrypt(password, salt, 64)` + salt; sadece `password_hash` alanında.
5. Bir kullanıcıyı iptal: `users.disabled = true` (admin arayüz veya Supabase paneli).
6. Şifre sıfırlama: admin yeni şifre atar (self-service yok — basit tutuyoruz).

---

## 9. Firmware yapısı (PlatformIO — tek proje, 3 env)

```ini
[env]
platform = espressif32
framework = arduino
monitor_speed = 115200
lib_deps =
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^7
build_flags = -DMQTT_MAX_PACKET_SIZE=1024

[env:kapi]
board = esp32dev
build_flags = ${env.build_flags} -DROLE_KAPI

[env:salon]
board = esp32dev
build_flags = ${env.build_flags} -DROLE_SALON
lib_deps = ${env.lib_deps}
    crankyoldgit/IRremoteESP8266@^2.8

[env:kamera]
board = esp32cam
build_flags = ${env.build_flags} -DROLE_KAMERA -DBOARD_HAS_PSRAM
```

### Modüller
```
include/secrets.h            (gitignore)  — WiFi, HiveMQ (esp kullanıcısı), INGEST_SECRET, ingest URL
include/secrets.example.h    (commit)
src/core/Logger.{h,cpp}       — Serial + ev/<dev>/log aynası, seviye filtresi, rate-limit
src/core/NetworkManager.*     — WiFi + MQTT + LWT + reconnect backoff + NTP + setBufferSize
src/core/CommandRouter.*      — JSON parse, id eşleştir, dispatch, ACK yayınla
src/core/Telemetry.*          — 60 sn MQTT telemetri, 5 dk IngestClient
src/core/IngestClient.*       — HTTPS POST /api/ingest (event/state/ack/log/telemetry)
src/roles/DoorRelay.*         — cmd:open → 2000 ms pulse + event + ack
src/roles/CamNode.*           — kamera + MJPEG kare yayını + PIR + ışık
src/roles/AcNode.*            — IRremoteESP8266 ile klima aç/kapa
src/main.cpp                  — #if defined(ROLE_*) ile ilgili rolü kur
```

Mevcut koddan korunan: `Actuator` sınıfı (aktif-low röle lojiği), `NetworkManager` callback
tasarımı, genel modüler ayrım.

### 9.1 ESP32-CAM (AI-Thinker) pin haritası

Kamera + PSRAM sabit kullanır: `0, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39`.
Kalan kullanılabilir: `1, 2, 3, 4, 12, 13, 14, 15`. SD kart kullanılmaz.

| Fonksiyon | GPIO | Not |
|---|---|---|
| PIR OUT | 13 | dijital giriş |
| Işık | 4 | kart üstü beyaz flash LED; harici ışık için buradan transistör/röle |
| USB Serial | 1, 3 | **korunur** — loglar hem seri hem MQTT `ev/kamera/log` |
| Boşta | 2, 12, 14, 15 | ileride ihtiyaç olursa (DHT, ikinci röle, buzzer...) |

RFID yok → pin bolluğu var, seri debug açık kalıyor. Geliştirme sırası: (1) kamera + MQTT kare
akışı, (2) PIR (GPIO13), (3) ışık (GPIO4).

---

## 10. Faz 0 — Güvenlik (repo public olacağı için ZORUNLU)

Commit'lenmiş / push'lanmış sırlar — **sızmış kabul edilir, rotate edilir**:
- `sutluce-ev-app/.env` → HiveMQ user/pass
- `sutluce-ev-app/.env.local` → Supabase `service_role` key, JWT secret, Postgres şifresi, anon key
- `sutluce-ev-esp32/include/Config.h` → WiFi + HiveMQ

Adımlar:
1. **Rotate:**
   - HiveMQ: eski kullanıcıyı sil, §8.1'deki yeni kullanıcıları güçlü şifrelerle oluştur.
   - Supabase: Postgres şifresini sıfırla; JWT secret rotate (anon + service_role yenilenir).
   - WiFi: mümkünse ev ağı şifresini değiştir (en azından not et).
2. **Geçmiş temizliği** (`sutluce-ev-app`):
   ```
   pip install git-filter-repo
   git filter-repo --path sutluce-ev-app/.env --path sutluce-ev-app/.env.local --invert-paths
   git remote add origin <url>
   git push --force --all
   ```
3. `.gitignore`: `.env*`, `**/secrets.h`, `.pio`, `node_modules`, `.vercel`.
4. `.env.example` (app) + `include/secrets.example.h` (esp) — sahte değerlerle.
5. `sutluce-ev-esp32` + kök dizin: `git init` (önce `Config.h` → `secrets.h` yapısına geçir).
6. Yeni sırlar sadece: Vercel env paneli + lokal `secrets.h` (gitignore).

---

## 11. Free tier özet

| Servis | Limit | Kullanım | Risk |
|---|---|---|---|
| HiveMQ Cloud | 100 bağlantı, 10 GB/ay | ~6 bağlantı; kamera VGA5 ~15 dk/gün | Yüksek çözünürlük/fps'te ay çabuk biter → 180 sn auto-stop + dashboard barı |
| Vercel Hobby | 100 GB bant, cron var | statik + ~7 küçük fonksiyon | `/api/command` cold start ~1-2 sn |
| Supabase Free | 500 MB DB, 5 GB egress, 7g inaktivite | ~20 MB DB | düşük |

---

## 12. Yapılacaklar (faz faz)

### Faz 0 — Güvenlik (§10)
- [ ] Sırları rotate et (HiveMQ, Supabase, WiFi)
- [ ] `git filter-repo` ile `.env*` geçmişten temizle + force push
- [ ] `.gitignore` düzelt, `.env.example` / `secrets.example.h`
- [ ] `sutluce-ev-esp32` + kök dizin `git init` (secrets.h yapısıyla)

### Faz 1 — Sözleşmeler
- [ ] MQTT + payload şemasını dondur (bu doküman)
- [ ] Supabase SQL migration + RLS uygula (`users`, `usage` dahil)
- [ ] HiveMQ kullanıcıları / ACL kur
- [ ] `scripts/adduser.mjs` + ilk admin kullanıcı

### Faz 2 — Firmware çekirdeği
- [ ] PlatformIO 3 env + `ROLE_*`
- [ ] `NetworkManager`: LWT, backoff, NTP, `setBufferSize`
- [ ] `Logger` (Serial + MQTT + rate-limit)
- [ ] `CommandRouter` + ACK
- [ ] `Telemetry` (MQTT 60 sn + ingest 5 dk)
- [ ] `IngestClient`

### Faz 3 — Firmware roller
- [ ] `kamera`: MQTT kare akışı + `stream_set` canlı res/fps, sonra PIR + ışık
- [ ] `kapi`: 2 sn pulse + event + ack
- [ ] `salon`: IR klima aç/kapa

### Faz 4 — Backend (Vercel)
- [ ] `/api/session` (kul.adı+şifre, cookie) + `/api/admin/users`
- [ ] `/api/command` (cookie auth, sunucudan publish)
- [ ] `/api/ingest`
- [ ] `/api/history` (+ usage_today/month)
- [ ] `/api/usage`
- [ ] `/api/cron/cleanup` + `vercel.json` cron
- [ ] eski `control.js` / `status.js` kaldır, `package.json` sadeleştir

### Faz 5 — Arayüz
- [ ] `index.html` / `styles.css` / `app.js` böl
- [ ] `/login` ekranı → `/api/session`, cookie kontrolü
- [ ] `mqtt.js` (CDN) WSS salt-okunur bağlantı + reconnect + bağlantı göstergesi
- [ ] Cihaz sayfaları: kapi / kamera / salon
- [ ] Retained `state` + `/api/history`'den ilk yükleme
- [ ] Komutlar `/api/command`'e; ACK → toast/onay
- [ ] Terminal: gerçek `ev/+/log`, cihaz filtresi, seviye renkleri, satır limiti
- [ ] Telemetri kartlarını canlı doldur
- [ ] Kamera: on-demand `<img>` akışı, **çözünürlük + fps seçici**, 180 sn sayaç
- [ ] Dashboard: kamera altında **günlük + aylık kullanım barı** (usage)
- [ ] "Son hareket" (event:motion + history)
- [ ] Kırık `via.placeholder.com` → inline SVG; `manifest.json` ikon; `sw.js` ekle ya da SW kaldır
- [ ] Simülasyon/placeholder kodu temizle

### Faz 6 — Sağlamlaştırma
- [ ] Gerçek kullanım ölçümü (HiveMQ / Vercel / Supabase)
- [ ] Uzaktan log seviyesi
- [ ] Offline/presence uyarısı
- [ ] DuckDNS + port-forward denemesi
- [ ] README + pin haritası + kurulum

---

## 13. Açık sorular
- (yok — tüm kararlar netleşti. Faz 0'a hazır.)
