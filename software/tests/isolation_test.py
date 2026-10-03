"""Two users, one screenshot: prove row-level security and the account function.
Usage: python software/tests/isolation_test.py   (needs SUPABASE_URL, SUPABASE_ANON_KEY)"""
import os, secrets, sys, requests

URL, KEY = os.environ["SUPABASE_URL"].rstrip("/"), os.environ["SUPABASE_ANON_KEY"]
H = {"apikey": KEY}
fails = 0

def check(name, ok):
    global fails
    print(("PASS " if ok else "FAIL ") + name)
    fails += 0 if ok else 1

def account(**body):
    return requests.post(f"{URL}/functions/v1/account", json=body, headers=H, timeout=20)

def sign_in(user, password):
    r = requests.post(f"{URL}/auth/v1/token?grant_type=password", headers=H, timeout=20,
                      json={"email": f"{user}@oscil.local", "password": password})
    return r.json() if r.status_code == 200 else None

def auth(tok):
    return {**H, "Authorization": f"Bearer {tok['access_token']}"}

tag = secrets.token_hex(3)
a, b = f"test_a_{tag}", f"test_b_{tag}"
for u in (a, b):
    check(f"sign up {u}", account(action="signup", username=u, password="password-123", phrase="red probe blue").ok)
ta, tb = sign_in(a, "password-123"), sign_in(b, "password-123")
check("both sign in", bool(ta and tb))
uid_a = ta["user"]["id"]
path = f"{uid_a}/000001.osc"

# A saves a screenshot: object + row
r = requests.post(f"{URL}/storage/v1/object/screenshots/{path}", data=b"OSC1test",
                  headers={**auth(ta), "content-type": "application/octet-stream"}, timeout=20)
check("A uploads into own folder", r.status_code == 200)
r = requests.post(f"{URL}/rest/v1/screenshots", json={"shot_id": 1, "link": path, "bytes": 8},
                  headers={**auth(ta), "Prefer": "return=minimal"}, timeout=20)
check("A adds own row", r.status_code == 201)

# B tries everything
r = requests.get(f"{URL}/rest/v1/screenshots?select=*", headers=auth(tb), timeout=20)
check("B lists screenshots: sees none of A's", r.status_code == 200 and r.json() == [])
r = requests.get(f"{URL}/storage/v1/object/authenticated/screenshots/{path}", headers=auth(tb), timeout=20)
check("B downloads A's file: refused", r.status_code in (400, 403, 404))
r = requests.post(f"{URL}/storage/v1/object/screenshots/{uid_a}/000099.osc", data=b"x",
                  headers={**auth(tb), "content-type": "application/octet-stream"}, timeout=20)
check("B writes into A's folder: refused", r.status_code in (400, 403))
r = requests.post(f"{URL}/rest/v1/screenshots", json={"owner": uid_a, "shot_id": 99, "link": path, "bytes": 1},
                  headers=auth(tb), timeout=20)
check("B adds a row as A: refused", r.status_code in (401, 403))
r = requests.get(f"{URL}/rest/v1/recovery?select=*", headers=auth(ta), timeout=20)
check("nobody reads phrase hashes", r.status_code in (401, 403, 404) or r.json() == [])
r = requests.get(f"{URL}/rest/v1/screenshots?select=*", headers=H, timeout=20)
check("signed out: sees nothing", r.status_code in (401, 403) or r.json() == [])

# A still sees exactly one
r = requests.get(f"{URL}/rest/v1/screenshots?select=*", headers=auth(ta), timeout=20)
check("A sees own row", r.status_code == 200 and len(r.json()) == 1)

# Forgot password
check("reset, wrong phrase: refused", account(action="reset", username=a, phrase="green", new_password="new-pass-456").status_code == 401)
check("reset, right phrase (any case/spacing)", account(action="reset", username=a, phrase="  Red Probe  BLUE", new_password="new-pass-456").ok)
check("old password refused", sign_in(a, "password-123") is None)
check("new password works", sign_in(a, "new-pass-456") is not None)
for _ in range(5):
    account(action="reset", username=b, phrase="nope", new_password="whatever-789")
check("5 wrong phrases lock resets", account(action="reset", username=b, phrase="red probe blue", new_password="whatever-789").status_code == 429)

print(f"\n{'ALL PASS' if fails == 0 else f'{fails} FAILED'}  (test users {a}, {b}: delete them under Authentication)")
sys.exit(1 if fails else 0)