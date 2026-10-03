# Software

Supabase setup for Oscil accounts and saved captures. Device code lives in
[`firmware/`](../firmware/).

| File | Job |
|---|---|
| [`schema.sql`](schema.sql) | Tables, row-level security and the private screenshots bucket |
| [`index.ts`](index.ts) | Account function: username signup and secret-phrase password reset |
| `tests/` | Account and isolation tests, not implemented yet |

## Setup

The project is `oscil-dev`, in West US. Email sign-in is enabled and email
confirmation is off because device accounts use `<username>@oscil.local`.

Run `schema.sql` once in the Supabase SQL editor. Keep the same SQL here so
the setup can be recreated. Project URL and publishable key go in the
firmware's git-ignored `secrets.h`. The service-role key stays on the server.

Paste `index.ts` into the Supabase Edge Function editor and deploy it as
`account`. Turn Verify JWT off so signup and reset can be called before
sign-in. Supabase supplies the service-role key in the function environment.

## Progress

| What | Status |
|---|---|
| Project and email settings | Done |
| Tables, security policies and private bucket | SQL applied successfully |
| Account function | Deployed; signup and duplicate rejection passed, reset tests pending |
| Isolation tests | Not run |

## Build log

[Build log](docs/build_log.md)

[Schema creation screenshot](docs/screenshots_videos/supabase_schema_created.png)

[Signup and duplicate test](docs/screenshots_videos/account_signup_duplicate_test.png)
