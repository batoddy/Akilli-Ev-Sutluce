-- ============================================================================
--  4/x — usage_daily atomik artırma (kamera trafiği sayacı)
--  Şemayı önce çalıştırdıysan bunu ayrıca çalıştır. (all_in_one.sql'de zaten var.)
-- ============================================================================

create or replace function usage_add(p_device text, p_day date, p_bytes bigint)
returns bigint
language plpgsql security definer set search_path = public, extensions
as $fn$
declare v_total bigint;
begin
  insert into usage_daily (device_id, day, bytes)
  values (p_device, p_day, p_bytes)
  on conflict (device_id, day) do update set bytes = usage_daily.bytes + excluded.bytes
  returning bytes into v_total;
  return v_total;
end;
$fn$;

revoke execute on function usage_add(text, date, bigint) from anon, authenticated;
