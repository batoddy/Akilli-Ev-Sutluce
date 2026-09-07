-- ============================================================================
--  1/3 — Tablolar + indeksler + RLS
--  Supabase → SQL Editor → yapıştır → Run  (bu dosyada $$ gövde yok, sorunsuz çalışır)
--  Idempotent: tekrar çalıştırılabilir.
-- ============================================================================

create extension if not exists pgcrypto with schema extensions;

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
create index if not exists events_dev_time_idx  on events (device_id, created_at desc);
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

create table if not exists usage_daily (
  device_id   text not null,
  day         date not null,
  bytes       bigint not null default 0,
  primary key (device_id, day)
);

create table if not exists users (
  username        text primary key,
  password_hash   text not null,         -- bcrypt (pgcrypto)
  is_admin        boolean not null default false,
  disabled        boolean not null default false,
  created_at      timestamptz not null default now(),
  last_login      timestamptz
);

-- RLS: hepsi açık, politika YOK → sadece service_role (Vercel API) erişir.
alter table devices       enable row level security;
alter table device_state  enable row level security;
alter table events        enable row level security;
alter table commands      enable row level security;
alter table logs          enable row level security;
alter table telemetry     enable row level security;
alter table usage_daily   enable row level security;
alter table users         enable row level security;
