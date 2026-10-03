// Oscil account function: signup and secret-phrase password reset.
// The service-role key is injected by Supabase; it never leaves this function.
import { createClient } from "npm:@supabase/supabase-js@2";

const admin = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_SERVICE_ROLE_KEY")!,
  { auth: { persistSession: false } });

const USER_RE = /^[a-z0-9_]{3,20}$/;
const ITER = 100_000;
const enc = new TextEncoder();
const emailOf = (u: string) => `${u}@oscil.local`;
const norm = (p: string) => p.trim().toLowerCase().replace(/\s+/g, " ");   // "Blue  Dog" == "blue dog"
const hex = (b: Uint8Array) => Array.from(b, (x) => x.toString(16).padStart(2, "0")).join("");
const unhex = (s: string) => new Uint8Array(s.match(/../g)!.map((h) => parseInt(h, 16)));
const json = (status: number, body: unknown) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });

async function pbkdf2(phrase: string, salt: Uint8Array, iter: number): Promise<string> {
  const key = await crypto.subtle.importKey("raw", enc.encode(norm(phrase)), "PBKDF2", false, ["deriveBits"]);
  const bits = await crypto.subtle.deriveBits({ name: "PBKDF2", hash: "SHA-256", salt, iterations: iter }, key, 256);
  return hex(new Uint8Array(bits));
}

async function hashPhrase(phrase: string): Promise<string> {
  const salt = crypto.getRandomValues(new Uint8Array(16));
  return `pbkdf2$${ITER}$${hex(salt)}$${await pbkdf2(phrase, salt, ITER)}`;
}

async function phraseMatches(phrase: string, stored: string): Promise<boolean> {
  const [, iter, salt, want] = stored.split("$");
  const got = await pbkdf2(phrase, unhex(salt), Number(iter));
  let diff = got.length ^ want.length;                          // constant time
  for (let i = 0; i < Math.min(got.length, want.length); i++) diff |= got.charCodeAt(i) ^ want.charCodeAt(i);
  return diff === 0;
}

async function signup(username: string, password: string, phrase: string) {
  if (password.length < 8) return json(400, { error: "Password: 8 or more characters" });
  if (norm(phrase).length < 8) return json(400, { error: "Secret phrase: 8 or more characters" });
  const { data, error } = await admin.auth.admin.createUser({ email: emailOf(username), password, email_confirm: true });
  if (error) {
    const taken = /already/i.test(error.message);
    return json(taken ? 409 : 400, { error: taken ? "Username taken" : error.message });
  }
  const id = data.user.id;
  const p = await admin.from("profiles").insert({ id, username });
  const r = await admin.from("recovery").insert({ user_id: id, phrase_hash: await hashPhrase(phrase) });
  if (p.error || r.error) {                                     // all or nothing
    await admin.auth.admin.deleteUser(id);
    return json(500, { error: "Could not create account" });
  }
  return json(200, { ok: true });
}

async function reset(username: string, phrase: string, password: string) {
  if (password.length < 8) return json(400, { error: "Password: 8 or more characters" });
  const wrong = json(401, { error: "Username or secret phrase is wrong" });   // same answer for both
  const { data: prof } = await admin.from("profiles").select("id").eq("username", username).maybeSingle();
  if (!prof) return wrong;
  const { data: rec } = await admin.from("recovery").select("*").eq("user_id", prof.id).single();
  if (!rec) return wrong;
  if (rec.locked_until && new Date(rec.locked_until) > new Date())
    return json(429, { error: "Too many tries. Wait 15 minutes" });
  if (!(await phraseMatches(phrase, rec.phrase_hash))) {
    const failed = rec.failed + 1;
    await admin.from("recovery").update(failed >= 5
      ? { failed: 0, locked_until: new Date(Date.now() + 15 * 60_000).toISOString() }
      : { failed }).eq("user_id", prof.id);
    return wrong;
  }
  const { error } = await admin.auth.admin.updateUserById(prof.id, { password });
  if (error) return json(400, { error: error.message });
  await admin.from("recovery").update({ failed: 0, locked_until: null }).eq("user_id", prof.id);
  return json(200, { ok: true });
}

Deno.serve(async (req) => {
  if (req.method !== "POST") return json(405, { error: "POST only" });
  let b: Record<string, unknown>;
  try { b = await req.json(); } catch { return json(400, { error: "Body must be JSON" }); }
  const username = String(b.username ?? "").trim().toLowerCase();
  if (!USER_RE.test(username)) return json(400, { error: "Username: 3 to 20 of a-z, 0-9, _" });
  if (b.action === "signup") return signup(username, String(b.password ?? ""), String(b.phrase ?? ""));
  if (b.action === "reset") return reset(username, String(b.phrase ?? ""), String(b.new_password ?? ""));
  return json(400, { error: "action must be signup or reset" });
});