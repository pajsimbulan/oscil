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

The SQL is saved in the repo. Account creation and isolation tests are next;
the successful query confirms setup, not the results of those tests.

![Supabase schema created](screenshots_videos/supabase_schema_created.png)
