#!/usr/bin/env python3
import argparse
import logging
import queue
import sys
import threading
import time
from pathlib import Path

import cv2
from ultralytics import YOLO

_LOG_DIR = Path(__file__).parent / "log"
_LOG_DIR.mkdir(exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[
        logging.FileHandler(_LOG_DIR / "app.log"),
        logging.StreamHandler(sys.stderr),
    ],
)
logger = logging.getLogger(__name__)


class VideoReader:
    # RAII обёртка над VideoCapture — открываем в конструкторе, закрываем в деструкторе
    def __init__(self, path: str):
        self._path = path
        self._cap = cv2.VideoCapture(path)
        if not self._cap.isOpened():
            msg = f"Cannot open video: '{path}'"
            logger.error(msg)
            raise RuntimeError(msg)

        self.fps          = self._cap.get(cv2.CAP_PROP_FPS) or 30.0
        self.width        = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.height       = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.total_frames = int(self._cap.get(cv2.CAP_PROP_FRAME_COUNT))
        logger.info("Video opened: %s | %dx%d @ %.1f fps | %d frames",
                    path, self.width, self.height, self.fps, self.total_frames)

    def read(self):
        return self._cap.read()

    def __del__(self):
        if hasattr(self, "_cap") and self._cap is not None:
            self._cap.release()
            logger.info("VideoReader released: %s", self._path)


class VideoWriter:
    # RAII обёртка над VideoWriter
    def __init__(self, path: str, fps: float, width: int, height: int):
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        self._writer = cv2.VideoWriter(path, fourcc, fps, (width, height))
        if not self._writer.isOpened():
            msg = f"Cannot create output video: '{path}'"
            logger.error(msg)
            raise RuntimeError(msg)
        self._path = path
        logger.info("VideoWriter created: %s", path)

    def write(self, frame) -> None:
        self._writer.write(frame)

    def __del__(self):
        if hasattr(self, "_writer") and self._writer is not None:
            self._writer.release()
            logger.info("VideoWriter released: %s", self._path)


def _worker(input_queue: queue.Queue,
            output_dict: dict,
            lock: threading.Lock,
            model_path: str) -> None:
    # каждый поток создаёт свою копию модели — YOLO не потокобезопасен
    # при общем экземпляре потоки перезаписывают внутреннее состояние друг друга
    model = YOLO(model_path)
    logger.info("Worker started, model loaded in thread %s", threading.current_thread().name)

    while True:
        try:
            item = input_queue.get(timeout=2.0)
        except queue.Empty:
            continue

        if item is None:
            input_queue.task_done()
            break

        frame_idx, frame = item

        # инференс отпускает GIL — потоки реально работают параллельно
        results = model(frame, verbose=False)
        annotated_frame = results[0].plot()

        # пишем в словарь по индексу кадра чтобы потом восстановить порядок
        with lock:
            output_dict[frame_idx] = annotated_frame

        input_queue.task_done()


def process_single(reader: VideoReader, writer: VideoWriter, model_path: str) -> float:
    model = YOLO(model_path)
    logger.info("Single-thread processing started")

    t_start = time.perf_counter()
    processed = 0

    while True:
        ret, frame = reader.read()
        if not ret:
            break

        results = model(frame, verbose=False)
        annotated = results[0].plot()
        writer.write(annotated)

        processed += 1
        if processed % 30 == 0:
            elapsed = time.perf_counter() - t_start
            logger.info("Single: %d / %d frames (%.1f fps)",
                        processed, reader.total_frames, processed / elapsed)

    elapsed = time.perf_counter() - t_start
    logger.info("Single-thread done: %d frames in %.2f s", processed, elapsed)
    return elapsed


def process_multi(reader: VideoReader, writer: VideoWriter,
                  model_path: str, n_workers: int) -> float:
    # maxsize ограничивает очередь чтобы не загружать всё видео в память сразу
    input_queue: queue.Queue = queue.Queue(maxsize=n_workers * 4)
    output_dict: dict = {}
    lock = threading.Lock()

    workers = []
    for i in range(n_workers):
        t = threading.Thread(
            target=_worker,
            args=(input_queue, output_dict, lock, model_path),
            name=f"worker-{i}",
            daemon=True,
        )
        t.start()
        workers.append(t)
    logger.info("Started %d worker threads", n_workers)

    t_start = time.perf_counter()

    # читаем кадры и нумеруем их — номер это и есть позиция в оригинальном видео
    total_frames = 0
    while True:
        ret, frame = reader.read()
        if not ret:
            break
        input_queue.put((total_frames, frame))
        total_frames += 1

    # шлём None каждому воркеру как сигнал что кадры кончились
    for _ in workers:
        input_queue.put(None)

    input_queue.join()
    for t in workers:
        t.join()

    t_elapsed = time.perf_counter() - t_start
    logger.info("All workers done: %d frames processed in %.2f s", total_frames, t_elapsed)

    # воркеры писали в словарь вразнобой, здесь восстанавливаем правильный порядок
    for idx in range(total_frames):
        writer.write(output_dict[idx])

    return t_elapsed


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="YOLOv8s-pose inference — parallel frame processing")
    p.add_argument("--video",    required=True, metavar="PATH")
    p.add_argument("--mode",     choices=["single", "multi"], default="single")
    p.add_argument("--output",   default="output.mp4", metavar="PATH")
    p.add_argument("--workers",  type=int, default=4, metavar="N")
    p.add_argument("--model",    default="yolov8s-pose.pt", metavar="PATH")
    return p.parse_args()


def main() -> None:
    args = _parse_args()

    try:
        reader = VideoReader(args.video)
    except RuntimeError:
        sys.exit(1)

    try:
        writer = VideoWriter(args.output, reader.fps, reader.width, reader.height)
    except RuntimeError:
        sys.exit(1)

    n_workers = args.workers if args.mode == "multi" else 1
    logger.info("Mode: %s | Workers: %d | Model: %s", args.mode, n_workers, args.model)

    if args.mode == "single":
        elapsed = process_single(reader, writer, args.model)
    else:
        elapsed = process_multi(reader, writer, args.model, args.workers)

    speed = reader.total_frames / elapsed if elapsed > 0 else 0
    print(f"\n{'='*50}")
    print(f"  Режим:          {args.mode}")
    print(f"  Потоков:        {n_workers}")
    print(f"  Кадров:         {reader.total_frames}")
    print(f"  Время:          {elapsed:.2f} с")
    print(f"  Скорость:       {speed:.1f} fps")
    print(f"  Выходной файл:  {args.output}")
    print(f"{'='*50}\n")

    del writer
    del reader


if __name__ == "__main__":
    main()
