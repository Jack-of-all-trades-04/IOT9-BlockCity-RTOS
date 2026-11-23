-- ============================================================================
-- BlockCity RTOS — Supabase PostgreSQL Schema
-- Run this script in the Supabase SQL Editor (https://app.supabase.com)
-- ============================================================================

create table if not exists public.city_telemetry (
    id              bigint generated always as identity primary key,
    created_at      timestamptz not null default now(),
    tick_count      integer not null default 0,
    money           integer not null default 0,
    last_tick_net   integer not null default 0,
    city_aqi        real not null default 0,
    sun_phase       smallint not null default 0,
    is_night        boolean not null default false,
    block_b_unlocked boolean not null default false,
    block_b_online  boolean not null default false,
    mayor_on_site   boolean not null default false,
    land_a_temp     real,
    land_a_hum      real,
    land_a_mq_raw   integer,
    land_a_ldr_pct  real,
    land_b_temp     real,
    land_b_hum      real,
    land_b_mq_raw   integer,
    land_b_ldr_pct  real,
    active_houses   smallint not null default 0,
    active_factories smallint not null default 0,
    active_parks    smallint not null default 0,
    active_solars   smallint not null default 0
);

create table if not exists public.building_events (
    id              bigint generated always as identity primary key,
    created_at      timestamptz not null default now(),
    slot_name       text not null,
    building_type   text not null,
    event_type      text not null, -- PERMIT_BOUGHT, VERIFIED_ACTIVE, ILLEGAL_BUILD, SOLD, LAND_B_UNLOCKED
    money_after     integer not null default 0,
    details         text
);

create index if not exists idx_city_telemetry_created_at
    on public.city_telemetry (created_at desc);

create index if not exists idx_building_events_created_at
    on public.building_events (created_at desc);

create or replace view public.latest_city_status as
select
    created_at,
    tick_count,
    money,
    city_aqi,
    land_a_temp,
    land_a_hum,
    land_b_temp,
    land_b_hum,
    block_b_unlocked,
    mayor_on_site
from public.city_telemetry
order by created_at desc
limit 1;

-- Row Level Security (RLS) for ESP32 PostgREST Anon Key Ingestion
alter table public.city_telemetry enable row level security;
alter table public.building_events enable row level security;

drop policy if exists "Allow anon insert telemetry" on public.city_telemetry;
create policy "Allow anon insert telemetry"
    on public.city_telemetry for insert
    to anon
    with check (true);

drop policy if exists "Allow anon select telemetry" on public.city_telemetry;
create policy "Allow anon select telemetry"
    on public.city_telemetry for select
    to anon
    using (true);

drop policy if exists "Allow anon insert events" on public.building_events;
create policy "Allow anon insert events"
    on public.building_events for insert
    to anon
    with check (true);

drop policy if exists "Allow anon select events" on public.building_events;
create policy "Allow anon select events"
    on public.building_events for select
    to anon
    using (true);
