-- ============================================================================
--  2/3 — Fonksiyonlar (SECURITY DEFINER — RLS'i aşar, sadece service_role çağırır)
--
--  ÖNCE hepsini birden yapıştırıp Run dene. "cannot insert multiple commands"
--  hatası alırsan: aşağıdaki ░░ blokları TEK TEK seç → Run (5 blok var).
-- ============================================================================


-- ░░ BLOK 1 ░░ ---------------------------------------------------------------
create or replace function app_login(p_username text, p_password text)
returns table(out_username text, out_is_admin boolean)
language sql security definer set search_path = public
as $func$
  select u.username, u.is_admin
  from users u
  where u.username = p_username
    and u.disabled = false
    and u.password_hash = crypt(p_password, u.password_hash);
$func$;


-- ░░ BLOK 2 ░░ ---------------------------------------------------------------
create or replace function app_create_user(p_username text, p_password text, p_is_admin boolean default false)
returns void
language sql security definer set search_path = public
as $func$
  insert into users (username, password_hash, is_admin)
  values (p_username, crypt(p_password, gen_salt('bf', 12)), p_is_admin)
  on conflict (username) do update
    set password_hash = excluded.password_hash,
        is_admin      = excluded.is_admin,
        disabled      = false;
$func$;


-- ░░ BLOK 3 ░░ ---------------------------------------------------------------
create or replace function app_set_user_disabled(p_username text, p_disabled boolean)
returns void
language sql security definer set search_path = public
as $func$
  update users set disabled = p_disabled where username = p_username;
$func$;


-- ░░ BLOK 4 ░░ ---------------------------------------------------------------
create or replace function cleanup_old_data()
returns void
language sql security definer set search_path = public
as $func$
  delete from events      where created_at < now() - interval '90 days';
  delete from commands    where created_at < now() - interval '60 days';
  delete from logs        where created_at < now() - interval '30 days';
  delete from telemetry   where created_at < now() - interval '30 days';
  delete from usage_daily where day < current_date - 400;
$func$;


-- ░░ BLOK 5 ░░ ---------------------------------------------------------------
revoke execute on function app_login(text, text)                from anon, authenticated;
revoke execute on function app_create_user(text, text, boolean) from anon, authenticated;
revoke execute on function app_set_user_disabled(text, boolean) from anon, authenticated;
revoke execute on function cleanup_old_data()                   from anon, authenticated;
