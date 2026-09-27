"""Save the next BEGIN..END block a board prints. Usage: python capture.py COM5 out.csv"""
import sys, serial

port, out = sys.argv[1], sys.argv[2]
vals,inside = [], False
with serial.Serial(port, 115200, timeout=15) as s:
    while True:
        line = s.readline().decode(errors="ignore").strip()
        if not line:
            sys.exit("timeout: no BEGIN seen")
        if line == "BEGIN":
            inside, vals = True, []
        elif line == "END" and inside:
            break
        elif inside:
            vals.append(line)

if len(vals) != 16384 or not all(v.isdigit() and len(v) == 4 for v in vals):
    sys.exit(f"bad capture: {len(vals)} lines, some corrupted; run again")
with open(out, "w") as f:
    f.write("value\n" + "\n".join(vals) + "\n")
print(f"{len(vals)} values -> {out}")