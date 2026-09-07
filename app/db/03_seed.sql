-- ============================================================================
--  3/3 — Seed (cihazlar) + ilk admin
-- ============================================================================

insert into devices (id, name, kind) values
  ('kapi',   'Dış Kapı (röle)',      'relay'),
  ('kamera', 'Dış Kamera + Hareket', 'camera'),
  ('salon',  'Salon Klima (IR)',     'ir')
on conflict (id) do nothing;

-- İLK ADMİN — tırnak içini kendi değerinle değiştir, sonra bu satırı çalıştır:
-- select app_create_user('KULLANICI_ADI', 'GUCLU_PAROLA', true);

-- Kontrol:
-- select id, name from devices;
-- select username, is_admin, disabled from users;
