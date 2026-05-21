#!/usr/bin/env python3
"""
Генерирует PDF-отчёт по результатам benchmark.py.
Читает results.json, строит 4 графика и выводы.

Запуск:
  python3 report.py
  python3 report.py --input results.json --output report.pdf
"""
import argparse
import json
import io
from pathlib import Path
from datetime import date

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
from matplotlib.backends.backend_pdf import PdfPages

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import cm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_LEFT, TA_JUSTIFY
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
    Image, PageBreak, HRFlowable,
)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfbase.pdfmetrics import registerFontFamily

# Регистрируем Arial — поддерживает кириллицу, всегда есть на Windows
_FONTS = {
    "Arial":           "C:/Windows/Fonts/arial.ttf",
    "Arial-Bold":      "C:/Windows/Fonts/arialbd.ttf",
    "Arial-Italic":    "C:/Windows/Fonts/ariali.ttf",
    "Arial-BoldItalic":"C:/Windows/Fonts/arialbi.ttf",
}
for name, path in _FONTS.items():
    pdfmetrics.registerFont(TTFont(name, path))
registerFontFamily("Arial",
    normal="Arial", bold="Arial-Bold",
    italic="Arial-Italic", boldItalic="Arial-BoldItalic"
)
FONT      = "Arial"
FONT_BOLD = "Arial-Bold"


# ── Графики ──────────────────────────────────────────────────────────────────

def make_graphs(single_time: float, rows: list[dict]) -> list[io.BytesIO]:
    """
    Возвращает список BytesIO с PNG-изображениями графиков.
    rows — список dict: workers, time, speedup, efficiency, utility
    """
    workers    = [r["workers"]    for r in rows]
    times      = [r["time"]       for r in rows]
    speedups   = [r["speedup"]    for r in rows]
    efficiency = [r["efficiency"] for r in rows]
    utility    = [r["utility"]    for r in rows]

    BLUE   = "#2563EB"
    GREEN  = "#16A34A"
    ORANGE = "#EA580C"
    RED    = "#DC2626"
    GRAY   = "#9CA3AF"

    style = {
        "figure.facecolor": "white",
        "axes.facecolor":   "#F9FAFB",
        "axes.grid":        True,
        "grid.color":       "#E5E7EB",
        "grid.linewidth":   0.8,
        "axes.spines.top":  False,
        "axes.spines.right":False,
        "font.family":      "DejaVu Sans",
        "font.size":        10,
    }

    images = []

    # ── График 1: Время выполнения ────────────────────────────────────────────
    with plt.style.context(style):
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.plot(workers, times, "o-", color=BLUE, linewidth=2, markersize=6, label="Многопоточный")
        ax.axhline(single_time, color=GRAY, linestyle="--", linewidth=1.5, label=f"Однопоточный ({single_time:.1f} с)")
        ax.set_xlabel("Число потоков")
        ax.set_ylabel("Время (с)")
        ax.set_title("Время обработки видео")
        ax.legend()
        ax.xaxis.set_major_locator(ticker.FixedLocator(workers))
        fig.tight_layout()
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=150)
        plt.close(fig)
        buf.seek(0)
        images.append(buf)

    # ── График 2: Ускорение ───────────────────────────────────────────────────
    with plt.style.context(style):
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.plot(workers, speedups, "o-", color=GREEN, linewidth=2, markersize=6, label="Реальное ускорение")
        ax.plot(workers, workers, "--", color=GRAY, linewidth=1.2, label="Идеальное (линейное)")
        ax.set_xlabel("Число потоков")
        ax.set_ylabel("Ускорение S(n) = T(1) / T(n)")
        ax.set_title("Ускорение относительно однопоточного режима")
        ax.legend()
        ax.xaxis.set_major_locator(ticker.FixedLocator(workers))
        fig.tight_layout()
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=150)
        plt.close(fig)
        buf.seek(0)
        images.append(buf)

    # ── График 3: Эффективность ───────────────────────────────────────────────
    with plt.style.context(style):
        fig, ax = plt.subplots(figsize=(7, 4))
        ax.plot(workers, efficiency, "o-", color=ORANGE, linewidth=2, markersize=6)
        ax.axhline(1.0, color=GRAY, linestyle="--", linewidth=1.2, label="Идеальная (E=1)")
        ax.set_xlabel("Число потоков")
        ax.set_ylabel("Эффективность E(n) = S(n) / n")
        ax.set_title("Эффективность использования потоков")
        ax.set_ylim(0, max(efficiency) * 1.2)
        ax.legend()
        ax.xaxis.set_major_locator(ticker.FixedLocator(workers))
        fig.tight_layout()
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=150)
        plt.close(fig)
        buf.seek(0)
        images.append(buf)

    # ── График 4: Полезность ──────────────────────────────────────────────────
    best_idx = utility.index(max(utility))
    with plt.style.context(style):
        fig, ax = plt.subplots(figsize=(7, 4))
        bar_colors = [RED if i == best_idx else BLUE for i in range(len(workers))]
        bars = ax.bar([str(w) for w in workers], utility, color=bar_colors, edgecolor="white", linewidth=0.5)
        ax.bar_label(bars, fmt="%.3f", padding=3, fontsize=9)
        ax.set_xlabel("Число потоков")
        ax.set_ylabel("Полезность P(n) = S(n) * E(n)")
        ax.set_title(f"Полезность потоков (оптимум: {workers[best_idx]} потока/ов)")
        fig.tight_layout()
        buf = io.BytesIO()
        fig.savefig(buf, format="png", dpi=150)
        plt.close(fig)
        buf.seek(0)
        images.append(buf)

    return images


# ── Reportlab PDF ─────────────────────────────────────────────────────────────

def build_pdf(data: dict, output_path: str) -> None:
    single_time = data["single_time"]
    rows        = data["multi_results"]
    video       = data.get("video", "—")
    model       = data.get("model", "yolov8s-pose.pt")

    # Находим оптимум по полезности
    best = max(rows, key=lambda r: r["utility"])

    # Генерируем графики
    graph_bufs = make_graphs(single_time, rows)

    doc = SimpleDocTemplate(
        output_path,
        pagesize=A4,
        leftMargin=2.5*cm, rightMargin=2.5*cm,
        topMargin=2.5*cm,  bottomMargin=2.5*cm,
    )

    title_style = ParagraphStyle("MyTitle",
        fontName=FONT_BOLD, fontSize=16, spaceAfter=6, alignment=TA_CENTER,
        leading=20)
    subtitle_style = ParagraphStyle("MySubtitle",
        fontName=FONT, fontSize=11, spaceAfter=4, alignment=TA_CENTER,
        leading=16, textColor=colors.HexColor("#6B7280"))
    h1_style = ParagraphStyle("H1",
        fontName=FONT_BOLD, fontSize=13, spaceBefore=14, spaceAfter=6,
        leading=18, textColor=colors.HexColor("#1E3A5F"))
    h2_style = ParagraphStyle("H2",
        fontName=FONT_BOLD, fontSize=11, spaceBefore=10, spaceAfter=4,
        leading=16, textColor=colors.HexColor("#374151"))
    body_style = ParagraphStyle("Body",
        fontName=FONT, fontSize=10, leading=15, spaceAfter=6,
        alignment=TA_JUSTIFY)
    formula_style = ParagraphStyle("Formula",
        fontName=FONT, fontSize=10, leading=16, spaceAfter=4,
        leftIndent=1*cm, textColor=colors.HexColor("#1E40AF"))

    story = []

    # ── Заголовок ─────────────────────────────────────────────────────────────
    story.append(Spacer(1, 0.5*cm))
    story.append(Paragraph("Лабораторная работа №5", title_style))
    story.append(Paragraph("Параллельный инференс YOLOv8s-pose на CPU", subtitle_style))
    story.append(Paragraph(f"Дата: {date.today().strftime('%d.%m.%Y')} &nbsp;|&nbsp; Видео: {video} &nbsp;|&nbsp; Модель: {model}", subtitle_style))
    story.append(HRFlowable(width="100%", thickness=1, color=colors.HexColor("#D1D5DB"), spaceAfter=12))

    # ── Описание задачи ───────────────────────────────────────────────────────
    story.append(Paragraph("1. Описание задачи", h1_style))
    story.append(Paragraph(
        "Задача — обработать видеофайл моделью YOLOv8s-pose (детекция поз людей) "
        "с использованием параллельных потоков Python. Каждый кадр независимо "
        "проходит инференс, что позволяет распределить нагрузку между несколькими "
        "рабочими потоками. Несмотря на GIL (Global Interpreter Lock), реальный "
        "параллелизм достигается за счёт того, что инференс выполняется в нативном "
        "C++ коде библиотеки ultralytics, который GIL отпускает.",
        body_style))
    story.append(Paragraph(
        "Каждый рабочий поток создаёт собственный экземпляр модели YOLO — это "
        "обязательно, так как модель содержит изменяемое внутреннее состояние "
        "и не является потокобезопасной при совместном использовании.",
        body_style))

    # ── Метрики ───────────────────────────────────────────────────────────────
    story.append(Paragraph("2. Метрики оценки", h1_style))
    story.append(Paragraph("Используются три метрики для оценки параллелизма:", body_style))

    story.append(Paragraph("<b>Ускорение</b> — во сколько раз быстрее N потоков по сравнению с 1:", h2_style))
    story.append(Paragraph("S(n) = T(1) / T(n)", formula_style))

    story.append(Paragraph("<b>Эффективность</b> — насколько каждый поток загружен полезной работой:", h2_style))
    story.append(Paragraph("E(n) = S(n) / n", formula_style))
    story.append(Paragraph(
        "При идеальном параллелизме E=1. На практике из-за накладных расходов "
        "(создание потоков, синхронизация очереди, GIL-паузы) E &lt; 1.",
        body_style))

    story.append(Paragraph("<b>Полезность</b> — комплексная метрика, учитывающая оба показателя:", h2_style))
    story.append(Paragraph("P(n) = S(n) × E(n) = S(n)<super>2</super> / n", formula_style))
    story.append(Paragraph(
        "Полезность максимальна при оптимальном числе потоков. Если добавлять "
        "потоки сверх оптимума — ускорение растёт незначительно, а эффективность "
        "резко падает, и полезность снижается.",
        body_style))

    # ── Таблица результатов ───────────────────────────────────────────────────
    story.append(Paragraph("3. Результаты измерений", h1_style))

    table_data = [["Режим", "Потоки", "Время (с)", "Ускорение S(n)", "Эффективность E(n)", "Полезность P(n)"]]
    table_data.append(["Single", "1", f"{single_time:.2f}", "1.000", "1.000", "1.000"])
    for r in rows:
        table_data.append([
            "Multi",
            str(r["workers"]),
            f"{r['time']:.2f}",
            f"{r['speedup']:.3f}",
            f"{r['efficiency']:.3f}",
            f"{r['utility']:.3f}",
        ])

    col_widths = [2.8*cm, 2*cm, 2.5*cm, 3.5*cm, 4*cm, 3.2*cm]
    t = Table(table_data, colWidths=col_widths, repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND",   (0, 0), (-1, 0),  colors.HexColor("#1E3A5F")),
        ("TEXTCOLOR",    (0, 0), (-1, 0),  colors.white),
        ("FONTNAME",     (0, 0), (-1, 0),  FONT_BOLD),
        ("FONTSIZE",     (0, 0), (-1, 0),  9),
        ("ALIGN",        (0, 0), (-1, -1), "CENTER"),
        ("VALIGN",       (0, 0), (-1, -1), "MIDDLE"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F3F4F6")]),
        ("FONTNAME",     (0, 1), (-1, -1), FONT),
        ("FONTSIZE",     (0, 1), (-1, -1), 9),
        ("GRID",         (0, 0), (-1, -1), 0.4, colors.HexColor("#D1D5DB")),
        ("TOPPADDING",   (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING",(0, 0), (-1, -1), 5),
        # Подсвечиваем строку с оптимумом
        ("BACKGROUND",   (0, rows.index(best) + 2), (-1, rows.index(best) + 2),
         colors.HexColor("#DCFCE7")),
        ("FONTNAME",     (0, rows.index(best) + 2), (-1, rows.index(best) + 2),
         FONT_BOLD),
    ]))
    story.append(t)
    story.append(Spacer(1, 0.3*cm))
    story.append(Paragraph(
        f"* Зелёным выделен оптимальный вариант: <b>{best['workers']} потоков</b> "
        f"(максимальная полезность P = {best['utility']:.3f})",
        ParagraphStyle("note", fontName=FONT, fontSize=9,
                       textColor=colors.HexColor("#16A34A"), spaceAfter=8)))

    # ── Графики ───────────────────────────────────────────────────────────────
    story.append(PageBreak())
    story.append(Paragraph("4. Графики", h1_style))

    graph_labels = [
        "Рис. 1 — Время обработки видео (меньше = лучше)",
        "Рис. 2 — Ускорение S(n) = T(1)/T(n) и идеальная линейная зависимость",
        "Рис. 3 — Эффективность E(n) = S(n)/n использования потоков",
        "Рис. 4 — Полезность P(n) = S(n)×E(n) — красным выделен оптимум",
    ]

    page_w = A4[0] - 5*cm   # ширина контента
    for buf, label in zip(graph_bufs, graph_labels):
        img = Image(buf, width=page_w, height=page_w * 4/7)
        story.append(img)
        story.append(Paragraph(label,
            ParagraphStyle("cap", fontName=FONT, fontSize=9,
                           textColor=colors.HexColor("#6B7280"),
                           alignment=TA_CENTER, spaceAfter=14)))

    # ── Выводы ────────────────────────────────────────────────────────────────
    story.append(PageBreak())
    story.append(Paragraph("5. Выводы", h1_style))

    # Динамические выводы по данным
    max_speedup = max(r["speedup"] for r in rows)
    max_speedup_w = max(rows, key=lambda r: r["speedup"])["workers"]
    eff_at_best = best["efficiency"]

    story.append(Paragraph(
        f"<b>1. Параллелизм работает.</b> Многопоточный режим ускоряет обработку "
        f"видео: максимальное ускорение S = {max_speedup:.2f}x достигается при "
        f"{max_speedup_w} потоках. Это подтверждает, что инференс YOLO на CPU "
        f"высвобождает GIL и потоки работают реально параллельно.",
        body_style))

    story.append(Paragraph(
        f"<b>2. Оптимальное число потоков — {best['workers']}.</b> "
        f"При этом значении полезность P = {best['utility']:.3f} максимальна. "
        f"Ускорение S = {best['speedup']:.2f}x, эффективность E = {eff_at_best:.3f} "
        f"({eff_at_best*100:.1f}% от идеала). Каждый поток приносит реальный прирост "
        f"производительности без значительных накладных расходов.",
        body_style))

    story.append(Paragraph(
        f"<b>3. Закон Амдала в действии.</b> С ростом числа потоков сверх "
        f"{best['workers']} ускорение растёт всё медленнее — последовательные "
        f"части (чтение кадров, восстановление порядка, запись видео) ограничивают "
        f"параллелизм. Эффективность при {rows[-1]['workers']} потоках падает до "
        f"{rows[-1]['efficiency']:.3f}, то есть каждый поток загружен лишь "
        f"на {rows[-1]['efficiency']*100:.1f}%.",
        body_style))

    story.append(Paragraph(
        "<b>4. Память.</b> Каждый рабочий поток загружает собственную копию модели "
        "YOLOv8s-pose (~80 МБ). При 8 потоках суммарно потребляется ~640 МБ RAM "
        "только под модели — это ограничивает масштабирование на машинах с малым "
        "объёмом памяти.",
        body_style))

    story.append(Paragraph(
        "<b>5. Восстановление порядка кадров.</b> Потоки обрабатывают кадры в "
        "произвольном порядке — быстрые кадры могут обогнать медленные. "
        "Порядок восстанавливается через словарь output_dict[frame_idx]: "
        "после завершения всех потоков главный поток читает кадры строго "
        "по индексу (0, 1, 2, ...) и записывает в видеофайл. "
        "Без этой техники видео имело бы перепутанные кадры.",
        body_style))

    doc.build(story)
    print(f"\nОтчёт сохранён: {Path(output_path).resolve()}")


# ── Точка входа ───────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description="Генерация PDF-отчёта по результатам benchmark.py")
    p.add_argument("--input",  default="results.json", help="JSON с результатами бенчмарка")
    p.add_argument("--output", default="report.pdf",   help="Выходной PDF-файл")
    args = p.parse_args()

    if not Path(args.input).exists():
        print(f"Файл {args.input} не найден. Сначала запустите benchmark.py")
        raise SystemExit(1)

    data = json.loads(Path(args.input).read_text(encoding="utf-8"))
    build_pdf(data, args.output)


if __name__ == "__main__":
    main()
