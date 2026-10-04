# Software

Supabase setup for Oscil accounts and cloud photos. Device code lives in
[`firmware/`](../firmware/).

| File | Job |
|---|---|
| [`schema.sql`](schema.sql) | Tables, row-level security and the private screenshots bucket |
| [`index.ts`](index.ts) | Account function: username signup and secret-phrase password reset |
| [`tests/isolation_test.py`](tests/isolation_test.py) | Two-user isolation, password reset and lockout tests |

## Setup

The project is `oscil-dev`, in West US. Email sign-in is enabled and email
confirmation is off because device accounts use `<username>@oscil.local`.

Run `schema.sql` once in the Supabase SQL editor. Keep the same SQL here so
the setup can be recreated. The private screenshots bucket allows 2 MB
per photo; the device uploads BMP images without saving them locally.
Project URL and publishable key go in the
firmware's git-ignored `secrets.h`. The service-role key stays on the server.

Paste `index.ts` into the Supabase Edge Function editor and deploy it as
`account`. Turn Verify JWT off so signup and reset can be called before
sign-in. Supabase supplies the service-role key in the function environment.

## Progress

| What | Status |
|---|---|
| Project and email settings | Done |
| Tables, security policies and private bucket | SQL applied successfully |
| Account function | Signup, duplicate rejection, password reset and lockout passed |
| Isolation tests | All 17 checks passed |

## Tests

Run from the repo root in PowerShell, with Windows Python and `requests`.
Set `SUPABASE_URL` and `SUPABASE_ANON_KEY` in that terminal, then run:

```powershell
python software/tests/isolation_test.py
```

Use the Windows Python executable directly if `python` points to MSYS2.
The script creates two test users and prints their names for cleanup.
Remove the test storage objects before deleting their Auth users.

## Build log

[Build log](docs/build_log.md)

[Schema creation screenshot](docs/screenshots_videos/supabase_schema_created.png)

[Signup and duplicate test](docs/screenshots_videos/account_signup_duplicate_test.png)

[Isolation test results](docs/screenshots_videos/supabase_isolation_all_pass.png)
