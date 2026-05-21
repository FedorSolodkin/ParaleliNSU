#!/usr/bin/env python3
"""
Обработка видеопотока с камеры в реальном времени через YOLOv8s-pose.

Запуск:
  python3 camera_stream.py
  python3 camera_stream.py --camera 0 --workers 2 --model yolov8s-pose.pt
"""
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
        logging.FileHandler(_LOG_DIR / "camera.log"),
        logging.StreamHandler(sys.stderr),
    ],
)
logger = logging.getLogger(__name__)


_CAM_MAX_RETRIES = 3
_CAM_RETRY_DELAY = 1.0


class CameraReader:
    # RAII обёртка над камерой — открываем в конструкторе, закрываем в деструкторе
    def __init__(self, index: int):
        self._cap = cv2.VideoCapture(index)
        if not self._cap.isOpened():
            msg = f"Cannot open camera {index}"
            logger.error(msg)
            raise RuntimeError(msg)
        self._index = index
        # запоминаем разрешение чтобы при переподключении не схватить другую камеру
        self._expected_w = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self._expected_h = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        logger.info("Camera %d opened at %dx%d", index, self._expected_w, self._expected_h)

    def read(self):
        if self._cap is None or not self._cap.isOpened():
            return False, None
        return self._cap.read()

    def reopen(self) -> bool:
        if self._cap is not None:
            self._cap.release()
            self._cap = None
        cap = cv2.VideoCapture(self._index)
        if not cap.isOpened():
            logger.error("Reconnect failed for camera %d", self._index)
            return False
        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        if actual_w != self._expected_w or actual_h != self._expected_h:
            logger.warning("Wrong camera at index %d: got %dx%d, expected %dx%d — skipping",
                           self._index, actual_w, actual_h, self._expected_w, self._expected_h)
            cap.release()
            return False
        self._cap = cap
        logger.info("Reconnected to camera %d at %dx%d", self._index, actual_w, actual_h)
        return True

    def __del__(self):
        if hasattr(self, "_cap") and self._cap is not None:
            self._cap.release()
            logger.info("Camera %d released", self._index)


def _worker(input_queue: queue.Queue,
            output_queue: queue.Queue,
            stop: threading.Event,
            model_path: str) -> None:
    # каждый поток создаёт свою копию модели — YOLO не потокобезопасен
    model = YOLO(model_path)
    logger.info("Worker started in thread %s", threading.current_thread().name)

    while not stop.is_set():
        try:
            frame = input_queue.get(timeout=1.0)
        except queue.Empty:
            continue

        # инференс отпускает GIL — потоки работают параллельно
        results = model(frame, verbose=False)
        annotated = results[0].plot()

        # если выходная очередь полна — выбрасываем старый кадр и кладём новый
        # нам важен самый свежий результат, а не очередь из старых
        if output_queue.full():
            try:
                output_queue.get_nowait()
            except queue.Empty:
                pass
        try:
            output_queue.put_nowait(annotated)
        except queue.Full:
            pass

        input_queue.task_done()

    logger.info("Worker stopped in thread %s", threading.current_thread().name)


def run(camera_index: int, n_workers: int, model_path: str) -> None:
    cam = CameraReader(camera_index)
    stop = threading.Event()

    # input_queue: камера → воркеры. maxsize=n_workers — не копим старые кадры
    input_queue: queue.Queue = queue.Queue(maxsize=n_workers)
    # output_queue: воркеры → дисплей. maxsize=1 — показываем только последний
    output_queue: queue.Queue = queue.Queue(maxsize=1)

    workers = []
    for i in range(n_workers):
        t = threading.Thread(
            target=_worker,
            args=(input_queue, output_queue, stop, model_path),
            name=f"worker-{i}",
            daemon=True,
        )
        t.start()
        workers.append(t)
    logger.info("Started %d worker threads", n_workers)

    cv2.namedWindow("YOLOv8 Camera", cv2.WINDOW_NORMAL)

    latest_frame = None
    fps_counter = 0
    fps_timer = time.perf_counter()
    fail_count = 0

    logger.info("Streaming started. Press 'q' or ESC to quit.")

    try:
        while True:
            ret, frame = cam.read()
            if not ret:
                fail_count += 1
                logger.warning("Camera lost (fail %d/%d)", fail_count, _CAM_MAX_RETRIES)
                if fail_count <= _CAM_MAX_RETRIES:
                    logger.info("Reconnect attempt %d/%d in %.1f s...",
                                fail_count, _CAM_MAX_RETRIES, _CAM_RETRY_DELAY)
                    time.sleep(_CAM_RETRY_DELAY)
                    if cam.reopen():
                        logger.info("Camera restored on attempt %d", fail_count)
                    continue
                else:
                    logger.error("Camera unavailable after %d attempts — shutting down", _CAM_MAX_RETRIES)
                    break

            fail_count = 0  # успешный кадр — сбрасываем счётчик

            # кладём кадр в очередь для воркеров
            # если очередь полна — пропускаем кадр (воркеры не успевают)
            if not input_queue.full():
                try:
                    input_queue.put_nowait(frame)
                except queue.Full:
                    pass

            # берём последний обработанный кадр если есть
            try:
                latest_frame = output_queue.get_nowait()
            except queue.Empty:
                pass

            # показываем последний обработанный кадр или оригинал пока воркер считает
            display = latest_frame if latest_frame is not None else frame

            # считаем и показываем fps
            fps_counter += 1
            elapsed = time.perf_counter() - fps_timer
            if elapsed >= 1.0:
                fps = fps_counter / elapsed
                fps_counter = 0
                fps_timer = time.perf_counter()
                cv2.setWindowTitle("YOLOv8 Camera", f"YOLOv8 Camera — {fps:.1f} fps")

            cv2.imshow("YOLOv8 Camera", display)
            if cv2.waitKey(1) in (ord("q"), ord("Q"), 27):
                logger.info("Quit key pressed")
                break

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        stop.set()
        for t in workers:
            t.join(timeout=3.0)
        cv2.destroyAllWindows()
        del cam
        logger.info("Shutdown complete")


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="YOLOv8s-pose camera stream")
    p.add_argument("--camera",  type=int, default=0,                  help="Индекс камеры (0, 1, ...)")
    p.add_argument("--workers", type=int, default=1,                  help="Число рабочих потоков")
    p.add_argument("--model",   default="yolov8s-pose.pt",            help="Путь к модели")
    return p.parse_args()


if __name__ == "__main__":
    args = _parse_args()
    try:
        run(args.camera, args.workers, args.model)
    except RuntimeError:
        sys.exit(1)
