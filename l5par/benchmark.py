#!/usr/bin/env python3
"""
Скрипт для подбора оптимального числа потоков.
Запускает main.py с разным числом потоков и выводит сравнительную таблицу.
Результаты сохраняются в results.json для последующей генерации отчёта.

Запуск:
  python3 benchmark.py --video input.mp4
  python3 benchmark.py --video input.mp4 --max-workers 8
"""
import argparse
import json
import subprocess
import sys
import re
from pathlib import Path


def run_once(video: str, mode: str, workers: int, model: str) -> float:
    """Запускает main.py и парсит время из вывода."""
    cmd = [
        sys.executable, "main.py",
        "--video", video,
        "--mode", mode,
        "--output", f"_bench_{mode}_{workers}.mp4",
        "--workers", str(workers),
        "--model", model,
    ]
    # stderr=STDOUT — мержим stderr в stdout, чтобы поймать вывод при любом исходе
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    combined = result.stdout
    # Ищем строку "Время: X.XX с"
    match = re.search(r"Время:\s+([\d.]+)", combined)
    if match:
        return float(match.group(1))
    # Не нашли — показываем понятную ошибку
    print(f"\n  [ОШИБКА] main.py завершился с кодом {result.returncode}")
    # Показываем только последние строки (самое важное)
    tail = "\n".join(combined.strip().splitlines()[-15:])
    print(f"  Вывод (последние строки):\n{tail}\n")
    return -1.0


def main():
    p = argparse.ArgumentParser(description="Benchmark different worker counts")
    p.add_argument("--video",        required=True)
    p.add_argument("--model",        default="yolov8s-pose.pt")
    p.add_argument("--max-workers",  type=int, default=8)
    p.add_argument("--output-json",  default="results.json",
                   help="Файл для сохранения результатов (для report.py)")
    args = p.parse_args()

    worker_counts = [1, 2, 4, 6, 8, 12, 16]
    worker_counts = [w for w in worker_counts if w <= args.max_workers]

    print("\nЗамер однопоточного режима...")
    t_single = run_once(args.video, "single", 1, args.model)
    print(f"  Single: {t_single:.2f} с\n")

    print(f"{'Потоки':>8} {'Время (с)':>12} {'Ускорение':>12} {'Эффективность':>15} {'Полезность':>12}")
    print("-" * 65)
    print(f"{'1 (single)':>8} {t_single:>12.2f} {'1.00':>12} {'1.000':>15} {'1.000':>12}")

    multi_results = []
    for w in worker_counts:
        t = run_once(args.video, "multi", w, args.model)
        if t > 0:
            speedup    = t_single / t
            efficiency = speedup / w
            utility    = speedup * efficiency   # ускорение × эффективность
        else:
            speedup = efficiency = utility = 0.0
        multi_results.append({
            "workers":    w,
            "time":       round(t, 3),
            "speedup":    round(speedup, 4),
            "efficiency": round(efficiency, 4),
            "utility":    round(utility, 4),
        })
        print(f"{w:>8} {t:>12.2f} {speedup:>12.2f} {efficiency:>15.3f} {utility:>12.3f}")

    print()

    # Сохраняем результаты в JSON для report.py
    data = {
        "video":         args.video,
        "model":         args.model,
        "single_time":   round(t_single, 3),
        "multi_results": multi_results,
    }
    out_path = Path(args.output_json)
    out_path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Результаты сохранены в {out_path.resolve()}")
    print("Запустите:  python3 report.py  — для генерации PDF-отчёта\n")


if __name__ == "__main__":
    main()
