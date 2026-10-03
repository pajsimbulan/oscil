# Software

Supabase setup for Oscil accounts and saved captures. Device code lives in
[`firmware/`](../firmware/).

| File | Job |
|---|---|
| [`schema.sql`](schema.sql) | Tables, row-level security and the private screenshots bucket |
| `account.ts` | Account function source, not implemented yet |
| `tests/` | Account and isolation tests, not implemented yet |

## Setup

The project is `oscil-dev`, in West US. Email sign-in is enabled and email
confirmation is off because device accounts use `<username>@oscil.local`.

Run `schema.sql` once in the Supabase SQL editor. Keep the same SQL here so
the setup can be recreated. Project URL and publishable key go in the
firmware's git-ignored `secrets.h`. The service-role key stays on the server.

## Progress

| What | Status |
|---|---|
| Project and email settings | Done |
| Tables, security policies and private bucket | SQL applied successfully |
| Account function | Not started |
| Isolation tests | Not run |

## Build log

[Build log](docs/build_log.md)

[Schema creation screenshot](docs/screenshots_videos/supabase_schema_created.png)
