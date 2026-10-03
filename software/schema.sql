-- Oscil: accounts and screenshots. Run once in the Supabase SQL editor.

-- User: id, userName, createdAt, updatedAt, lastActivity.
-- hashedPassword is Supabase Auth's (auth.users.encrypted_password, bcrypt): never our column.
create table public.profiles (
  id            uuid primary key references auth.users(id) on delete cascade,
  username      text unique not null check (username ~ '^[a-z0-9_]{3,20}$'),
  created_at    timestamptz not null default now(),
  updated_at    timestamptz not null default now(),
  last_activity timestamptz
);

-- Secret-phrase hash for "forgot password". No client can read this table.
create table public.recovery (
  user_id      uuid primary key references auth.users(id) on delete cascade,
  phrase_hash  text not null,              -- pbkdf2$<iterations>$<salt hex>$<hash hex>
  failed       int  not null default 0,
  locked_until timestamptz
);

-- Screenshots: id, createdAt, link. "A user's screenshots" = rows where owner = that user.
create table public.screenshots (
  id          bigint generated always as identity primary key,
  owner       uuid not null default auth.uid() references auth.users(id) on delete cascade,
  shot_id     int  not null,               -- the device's counter, from the .osc header
  created_at  timestamptz not null default now(),
  captured_at timestamptz,                 -- the .osc unix_time, if the clock was set
  link        text not null,               -- object path in bucket 'screenshots'
  bytes       int  not null,
  unique (owner, shot_id)                  -- uploading the same capture twice changes nothing
);

create function public.touch_updated_at() returns trigger language plpgsql as $$
begin new.updated_at = now(); return new; end $$;
create trigger profiles_updated before update on public.profiles
  for each row execute function public.touch_updated_at();

-- Row-level security: on, everywhere.
alter table public.profiles    enable row level security;
alter table public.recovery    enable row level security;
alter table public.screenshots enable row level security;

-- recovery: no policies at all, and no grants: only the account function (service role) touches it.
revoke all on public.recovery from anon, authenticated;

-- profiles: read your own; the only column you may change is last_activity.
create policy "read own profile" on public.profiles for select to authenticated
  using (id = (select auth.uid()));
create policy "touch own profile" on public.profiles for update to authenticated
  using (id = (select auth.uid())) with check (id = (select auth.uid()));
revoke update on public.profiles from authenticated;
grant  update (last_activity) on public.profiles to authenticated;

-- screenshots: read and add your own, and only with a link inside your own folder.
create policy "read own screenshots" on public.screenshots for select to authenticated
  using (owner = (select auth.uid()));
create policy "add own screenshots" on public.screenshots for insert to authenticated
  with check (owner = (select auth.uid()) and link like (select auth.uid())::text || '/%');

-- Bucket: private; each user writes and reads only <their uid>/...
insert into storage.buckets (id, name, public, file_size_limit)
values ('screenshots', 'screenshots', false, 65536);

create policy "upload own files" on storage.objects for insert to authenticated
  with check (bucket_id = 'screenshots' and (storage.foldername(name))[1] = (select auth.uid())::text);
create policy "read own files" on storage.objects for select to authenticated
  using (bucket_id = 'screenshots' and (storage.foldername(name))[1] = (select auth.uid())::text);