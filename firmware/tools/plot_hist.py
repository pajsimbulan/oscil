"""Histogram of ADC codes. Usage: python plot_hist.py noise.csv noise_hist.png"""
import sys, statistics
import matplotlib.pyplot as plt

src, out = sys.argv[1], sys.argv[2]
codes = [int(v) for v in open(src).read().split()[1:]]
mean, rms = statistics.fmean(codes), statistics.pstdev(codes)
mv = rms * 4 * 3314 / 4096

plt.figure(figsize=(7, 4))
plt.hist(codes, bins=range(min(codes), max(codes) + 2), color="#2a7ab0")
plt.title(f"CH1 noise, input grounded, 26.67 MHz, {len(codes)} samples")
plt.xlabel("ADC code")
plt.ylabel("count")
plt.figtext(0.99, 0.01, f"mean {mean:.1f}   rms {rms:.2f} codes ({mv:.1f} mV at BNC)",
            ha="right", fontsize=9)
plt.tight_layout()
plt.savefig(out, dpi=150)
print(f"saved {out}")