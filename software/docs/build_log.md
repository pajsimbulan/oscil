# Software build log

## 2026-10-03

Created `oscil-dev` in Supabase, in West US. Email sign-in is enabled,
confirmation is off. The device uses local account addresses that cannot
receive mail.

Applied `schema.sql` in the SQL editor. Supabase returned "Success. No rows
returned." The script creates `profiles`, `recovery` and `screenshots`, enables
row-level security, and creates a private screenshots bucket with a 64 KiB
file limit. Profile and screenshot policies restrict access to the signed-in
owner. Recovery has no client policies and its client grants are revoked.

The SQL is saved in the repo. Isolation tests are still pending;
the successful query confirms setup, not the results of those tests.

![Supabase schema created](screenshots_videos/supabase_schema_created.png)

Deployed the `account` Edge Function from `index.ts`, with Verify JWT off.
The source uses Supabase's server-side service-role key to create the Auth
user, profile and recovery hash. The secret phrase is normalised and hashed
with PBKDF2-SHA256, a random salt and 100,000 iterations.

Tested signup from PowerShell with `paul_test`. The first request returned
`ok: True`. Repeating it returned HTTP 409 Conflict, rejecting the duplicate
username. Password reset, lockout and cross-account isolation tests have not
run yet.

![Account signup and duplicate rejection](screenshots_videos/account_signup_duplicate_test.png)
