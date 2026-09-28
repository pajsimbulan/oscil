"""Plot the first 3 ms of a capture.py CSV.
Usage: python plot_wave.py burst.csv RATE_HZ burst_waveform.png
"""
import sys
import matplotlib.pyplot as plt

src, rate, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
codes = [int(v) for v in open(src).read().split()[1:]]

# First 3 ms. Use the measured sample rate, not just the requested one.
n = min(len(codes), int(0.003 * rate) + 1)
ms = [i * 1000.0 / rate for i in range(n)]

plt.figure(figsize=(9, 4))
plt.plot(ms, codes[:n], ".-", color="#2a7ab0", markersize=3, linewidth=1)
plt.title(f"CH1 burst, {rate:.0f} Sa/s, first 3 ms")
plt.xlabel("time (ms)")
plt.ylabel("ADC code")
plt.xlim(0, 3)
plt.grid(True, alpha=0.3)
plt.tight_layout()
plt.savefig(out, dpi=150)
print(f"saved {out}")
plt.show()