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
per photo, the device uploads BMP images without saving them locally.
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

<details>
<summary><b>Open the software build log</b>: setting up Supabase and proving one user can never reach another's data</summary>

## 2026-10-03

Created `oscil-dev` in Supabase, in West US. Email sign-in is enabled,
confirmation is off. The device uses local account addresses that cannot
receive mail.

Applied `schema.sql` in the SQL editor. Supabase returned "Success. No rows
returned." The script creates `profiles`, `recovery` and `screenshots`, enables
row-level security, and creates a private screenshots bucket with a 64 KiB
file limit. Profile and screenshot policies restrict access to the signed-in
owner. Recovery has no client policies and its client grants are revoked.

The SQL is saved in the repo. The successful query confirms setup,
account and isolation checks were run separately below.

![Supabase schema created](docs/screenshots_videos/supabase_schema_created.png)

Deployed the `account` Edge Function from `index.ts`, with Verify JWT off.
The source uses Supabase's server-side service-role key to create the Auth
user, profile and recovery hash. The secret phrase is normalised and hashed
with PBKDF2-SHA256, a random salt and 100,000 iterations.

Tested signup from PowerShell with `paul_test`. The first request returned
`ok: True`. Repeating it returned HTTP 409 Conflict, rejecting the duplicate
username. Password reset, lockout and cross-account isolation were checked
with the two-user test below.

![Account signup and duplicate rejection](docs/screenshots_videos/account_signup_duplicate_test.png)

Ran `tests/isolation_test.py` against `oscil-dev`. All 17 checks passed.
Both users could sign up and sign in. A uploaded a file and added its row,
B could not list or download it, write into A's folder, or insert a row as A.
The signed-out request returned no screenshots, and the recovery hash read
was refused. A could still read its own row.

Password reset refused the wrong phrase and accepted the right phrase with
different case and spacing. The old password stopped working, the new one
worked, and five wrong phrases locked further resets.

The PC's `python` command first resolved to MSYS2. Used Windows Python 3.13,
installed `requests` there, and set the Supabase URL and publishable key in
the PowerShell environment. The final run returned `ALL PASS`.

![Supabase isolation tests passed](docs/screenshots_videos/supabase_isolation_all_pass.png)

Raised the screenshots bucket limit from 64 KiB to 2 MB for the device's
800 x 480 BMP photos (1,152,054 bytes each). The change is saved in
`schema.sql`, the bucket stays private with the same owner policies.

</details>
