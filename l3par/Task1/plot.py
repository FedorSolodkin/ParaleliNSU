#!/usr/bin/env python3
"""
Читает results.csv и строит график ускорения S_p vs p
для обеих матриц. CSV формат: size,threads,t_min,t_avg,s_min,s_avg
"""
import csv
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    HAS_MPL = True
except ImportError:
    HAS_MPL = False

CSV_FILE = Path(__file__).parent / "results.csv"


def load(path):
    data = {}  # {size: [(threads, s_min, s_avg), ...]}
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            sz = int(row["size"])
            p  = int(row["threads"])
            s_min = float(row["s_min"])
            s_avg = float(row["s_avg"])
            data.setdefault(sz, []).append((p, s_min, s_avg))
    return data


def print_table(path):
    print("\n" + "=" * 68)
    print(f"{'Size':>10} | {'Threads':>7} | {'T_min (s)':>10} | "
          f"{'T_avg (s)':>10} | {'S_min':>7} | {'S_avg':>7}")
    print("-" * 68)
    with open(path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            print(f"{row['size']:>10} | {row['threads']:>7} | "
                  f"{float(row['t_min']):>10.4f} | {float(row['t_avg']):>10.4f} | "
                  f"{float(row['s_min']):>7.3f} | {float(row['s_avg']):>7.3f}")
    print("=" * 68)


def plot(data):
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))
    colors  = ["#e05c5c", "#4c7de0"]
    markers = ["o", "s"]
    sizes   = sorted(data.keys())
    all_p   = sorted({p for pts in data.values() for p, *_ in pts})

    opt_info = []   # [(sz, p_opt, q_opt)] для вывода в консоль

    for i, sz in enumerate(sizes):
        pts   = sorted(data[sz])
        xs    = [p           for p, *_ in pts]
        s_arr = [s_min       for _, s_min, _ in pts]
        e_arr = [s_min / p   for p, s_min, _ in pts]   # E_p = S_p / p
        q_arr = [s * e       for s, e in zip(s_arr, e_arr)]  # Q_p = S_p * E_p = S_p²/p

        # ── 1. Ускорение ─────────────────────────────────────────────────────
        axes[0].plot(xs, s_arr, color=colors[i], marker=markers[i],
                     linewidth=2, markersize=7, label=f"M = {sz:,}")

        # ── 2. Эффективность ─────────────────────────────────────────────────
        axes[1].plot(xs, e_arr, color=colors[i], marker=markers[i],
                     linewidth=2, markersize=7, label=f"M = {sz:,}")

        # ── 3. Q_p = S_p * E_p  (максимум = оптимальное число потоков) ───────
        axes[2].plot(xs, q_arr, color=colors[i], marker=markers[i],
                     linewidth=2, markersize=7, label=f"M = {sz:,}")

        # Отметить максимум Q_p
        p_opt  = xs[q_arr.index(max(q_arr))]
        q_opt  = max(q_arr)
        axes[2].axvline(x=p_opt, color=colors[i], linestyle=":", linewidth=1.5, alpha=0.8)
        axes[2].annotate(f"opt p={p_opt}",
                         xy=(p_opt, q_opt),
                         xytext=(p_opt + 0.5, q_opt - 0.4),
                         fontsize=9, color=colors[i],
                         arrowprops=dict(arrowstyle="->", color=colors[i], lw=1.2))
        opt_info.append((sz, p_opt, q_opt))

    # ── Настройки осей ────────────────────────────────────────────────────────
    axes[0].plot(all_p, all_p, "k--", linewidth=1.2, label="Linear (ideal)", zorder=0)
    axes[0].set_title("Ускорение  $S_p = T_1/T_p$", fontsize=12)
    axes[0].set_ylabel("$S_p$", fontsize=11)
    axes[0].set_ylim(bottom=0)

    axes[1].axhline(y=1.0, color="k", linestyle="--", linewidth=1.2,
                    label="Ideal (E=1.0)", zorder=0)
    axes[1].set_title("Эффективность  $E_p = S_p / p$", fontsize=12)
    axes[1].set_ylabel("$E_p$  (идеал = 1.0)", fontsize=11)
    axes[1].set_ylim(0, 1.15)

    axes[2].set_title("$Q_p = S_p \\cdot E_p = S_p^2 / p$\n"
                      "максимум → оптимальное число потоков", fontsize=11)
    axes[2].set_ylabel("$Q_p$", fontsize=11)
    axes[2].set_ylim(bottom=0)

    for ax in axes:
        ax.set_xlabel("Количество потоков  p", fontsize=11)
        ax.set_xticks(all_p)
        ax.xaxis.set_minor_locator(ticker.NullLocator())
        ax.grid(True, linestyle="--", alpha=0.5)
        ax.legend(fontsize=10)
        ax.set_xlim(left=0)

    fig.suptitle("Масштабируемость: умножение матрицы на вектор  (100 запусков/точка)",
                 fontsize=13)
    fig.tight_layout()
    out = Path(__file__).parent / "speedup_plot.png"
    fig.savefig(out, dpi=150)
    print(f"\nГрафик сохранён: {out}")

    print("\n--- Оптимальное число потоков (max Q_p = S_p * E_p) ---")
    for sz, p_opt, q_opt in opt_info:
        print(f"  M={sz:>6}: p_opt = {p_opt:>3}  (Q = {q_opt:.3f})")


if __name__ == "__main__":
    if not CSV_FILE.exists():
        print(f"Файл не найден: {CSV_FILE}\nСначала запустите: ./build/matvec")
        sys.exit(1)

    data = load(CSV_FILE)
    if not data:
        print("results.csv пуст")
        sys.exit(1)

    print_table(CSV_FILE)
    if HAS_MPL:
        plot(data)
    else:
        print("matplotlib не найден. Установите: pip3 install matplotlib --user")
