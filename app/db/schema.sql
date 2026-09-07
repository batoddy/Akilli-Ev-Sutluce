-- ============================================================================
--  Sütlüce Akıllı Ev — Supabase şema (idempotent, tekrar çalıştırılabilir)
--  Çalıştırma: Supabase → SQL Editor → yapıştır → Run
--  Şifreleme: bcrypt (pgcrypto). Parola doğrulama DB içinde (app_login) —
--  API katmanında crypto kütüphanesine gerek yok.
-- ============================================================================

create extension if not exists pgcrypto;

-- ---------------------------------------------------------------------------
--  Tablolar
-- ---------------------------------------------------------------------------

create table if not exists devices (
  id          text primary key,          -- 'kapi' | 'kamera' | 'salon'
  name        text not null,
  kind        text not null,
  fw_version  text,
  last_seen   timestamptz,
  online      boolean not null default false
);

create table if not exists device_state (
  device_id   text primary key references devices(id) on delete cascade,
  snapshot    jsonb not null,
  updated_at  timestamptz not null default now()
);

create table if not exists events (
  id          bigserial primary key,
  device_id   text not null,
  type        text not null,             -- motion | door_open | stream | boot ...
  payload     jsonb,
  created_at  timestamptz not null default now()
);
create index if not exists events_dev_time_idx on events (device_id, created_at desc);
create index if not exists events_type_time_idx on events (type, created_at desc);

create table if not exists commands (
  id          text primary key,          -- istemci üretir: 'c-<ts>-<rand>'
  device_id   text not null references devices(id) on delete cascade,
  cmd         text not null,
  args        jsonb,
  source      text not null default 'web',
  user_name   text,
  status      text not null default 'sent',   -- sent | ack_ok | ack_error | timeout
  detail      text,
  created_at  timestamptz not null default now(),
  acked_at    timestamptz
);
create index if not exists commands_dev_time_idx on commands (device_id, created_at desc);

create table if not exists logs (
  id          bigserial primary key,
  device_id   text not null,
  level       text not null,             -- WARN | ERROR | INFO(heartbeat)
  message     text not null,
  created_at  timestamptz not null default now()
);
create index if not exists logs_dev_time_idx on logs (device_id, created_at desc);

create table if not exists telemetry (
  id          bigserial primary key,
  device_id   text not null,
  rssi        int,
  heap_free   int,
  uptime_s    bigint,
  created_at  timestamptz not null default now()
);
create index if not exists telemetry_dev_time_idx on telemetry (device_id, created_at desc);

create table if not exists usage (
  device_id   text not null,
  day         date not null,
  bytes       bigint not null default 0,
  primary key (device_id, day)
);

create table if not exists users (
  username        text primary key,
  password_hash   text not null,         -- bcrypt (pgcrypto crypt)
  is_admin        boolean not null default false,
  disabled        boolean not null default false,
  created_at      timestamptz not null default now(),
  last_login      timestamptz
);

-- ---------------------------------------------------------------------------
--  RLS: hepsi açık, politika YOK → sadece service_role (Vercel API) erişir.
--  anon / authenticated hiçbir tabloyu okuyamaz/yazamaz.
-- ---------------------------------------------------------------------------

alter table devices       enable row level security;
alter table device_state  enable row level security;
alter table events        enable row level security;
alter table commands      enable row level security;
alter table logs          enable row level security;
alter table telemetry     enable row level security;
alter table usage         enable row level security;
alter table users         enable row level security;

-- ---------------------------------------------------------------------------
--  Fonksiyonlar (SECURITY DEFINER — RLS'i aşar, sadece service_role çağırır)
-- ---------------------------------------------------------------------------

-- Giriş doğrulama: geçerliyse 1 satır, değilse 0 satır döner.
create or replace function app_login(p_username text, p_password text)
returns table(username text, is_admin boolean)
language sql
security definer
set search_path = public
as $$
  select u.username, u.is_admin
  from users u
  where u.username = p_username
    and u.disabled = false
    and u.password_hash = crypt(p_password, u.password_hash);
$$;

-- Kullanıcı oluştur / parola sıfırla (aynı username -> parolayı günceller, disabled=false).
create or replace function app_create_user(p_username text, p_password text, p_is_admin boolean default false)
returns void
language sql
security definer
set search_path = public
as $$
  insert into users (username, password_hash, is_admin)
  values (p_username, crypt(p_password, gen_salt('bf', 12)), p_is_admin)
  on conflict (username) do update
    set password_hash = excluded.password_hash,
        is_admin      = excluded.is_admin,
        disabled      = false;
$$;

create or replace function app_set_user_disabled(p_username text, p_disabled boolean)
returns void
language sql
security definer
set search_path = public
as $$
  update users set disabled = p_disabled where username = p_username;
$$;

-- Retention (Vercel Cron günde 1 kez rpc ile çağırır).
create or replace function cleanup_old_data()
returns void
language sql
security definer
set search_path = public
as $$
  delete from events    where created_at < now() - interval '90 days';
  delete from commands  where created_at < now() - interval '60 days';
  delete from logs      where created_at < now() - interval '30 days';
  delete from telemetry where created_at < now() - interval '30 days';
  delete from usage     where day < current_date - 400;
$$;

-- anon/authenticated bu fonksiyonları PostgREST üzerinden ÇAĞIRAMAZ.
revoke execute on function app_login(text, text)              from anon, authenticated;
revoke execute on function app_create_user(text, text, boolean) from anon, authenticated;
revoke execute on function app_set_user_disabled(text, boolean) from anon, authenticated;
revoke execute on function cleanup_old_data()                 from anon, authenticated;

-- ---------------------------------------------------------------------------
--  Seed: cihazlar
-- ---------------------------------------------------------------------------

insert into devices (id, name, kind) values
  ('kapi',   'Dış Kapı (röle)',      'relay'),
  ('kamera', 'Dış Kamera + Hareket', 'camera'),
  ('salon',  'Salon Klima (IR)',     'ir')
on conflict (id) do nothing;

-- ---------------------------------------------------------------------------
--  İLK ADMİN — <> içini kendi değerinle değiştirip çalıştır:
--
--    select app_create_user('KULLANICI_ADI', 'GUCLU_PAROLA', true);
--
--  (Bu dosyayı tekrar çalıştırırsan yukarıdaki satır yorumda kaldığı için
--   admin ikinci kez oluşturulmaz.)
-- ---------------------------------------------------------------------------
