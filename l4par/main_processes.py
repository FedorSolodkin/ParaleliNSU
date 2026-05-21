#!/usr/bin/env python3
"""
Версия с процессами (multiprocessing) вместо потоков (threading).
Запуск: python3 main_processes.py --camera /dev/video0 --resolution 1280x720 --fps 30
"""
import argparse
import logging
import multiprocessing as mp
import sys
import time
from pathlib import Path

import cv2
import numpy as np

_LOG_DIR = Path(__file__).parent / "log"
_LOG_DIR.mkdir(exist_ok=True)

logging.basicConfig(
    level=logging.DEBUG,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[
        logging.FileHandler(_LOG_DIR / "app_proc.log"),
        logging.StreamHandler(sys.stderr),
    ],
)
logger = logging.getLogger(__name__)


# ── Датчики ──────────────────────────────────────────────────────────────────
# Классы Sensor/SensorX не меняются — они не зависят от потоков/процессов

class Sensor:
    def get(self):
        raise NotImplementedError("Subclasses must implement method get()")


class SensorX(Sensor):
    def __init__(self, delay: float):
        self._delay = delay
        self._data = 0

    def get(self) -> int:
        time.sleep(self._delay)
        self._data += 1
        return self._data


class SensorCam(Sensor):
    """Камера — НЕ передаётся в процесс, работает только в главном процессе.

    Почему? cv2.VideoCapture нельзя сериализовать (pickle) для передачи
    в дочерний процесс. Поэтому камеру читаем в главном процессе.
    """
    def __init__(self, device: str, resolution: tuple):
        self._log = logging.getLogger(self.__class__.__name__)
        self._device = device
        index = self._parse_index(device)
        cap = cv2.VideoCapture(index)
        if not cap.isOpened():
            msg = f"Cannot open camera '{device}'"
            self._log.error(msg)
            raise RuntimeError(msg)
        w, h = resolution
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, w)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, h)
        self._cap = cap
        self._log.info("Camera '%s' opened at %dx%d", device, w, h)

    def get(self):
        if self._cap is None or not self._cap.isOpened():
            return None
        ret, frame = self._cap.read()
        if not ret:
            self._log.error("Failed to read frame")
            return None
        return frame

    def __del__(self):
        if hasattr(self, '_cap') and self._cap is not None:
            self._cap.release()
            self._cap = None


    @staticmethod
    def _parse_index(device: str) -> int:
        try:
            return int(device.replace("/dev/video", ""))
        except ValueError:
            return 0


class WindowImage:
    def __init__(self, fps: float):
        self._log = logging.getLogger(self.__class__.__name__)
        self._wait_ms = max(1, int(1000 / fps))
        try:
            cv2.namedWindow("Sensor Dashboard", cv2.WINDOW_NORMAL)
        except Exception as exc:
            self._log.error("Cannot create window: %s", exc)
            raise
        self._log.info("Window created at %.1f fps", fps)

    def show(self, img: np.ndarray) -> int:
        cv2.imshow("Sensor Dashboard", img)
        return cv2.waitKey(self._wait_ms)

    def __del__(self):
        try:
            cv2.destroyWindow("Sensor Dashboard")
        except Exception:
            pass


# ── Функция процесса датчика ──────────────────────────────────────────────────
# КЛЮЧЕВОЕ ОТЛИЧИЕ от потоков:
# Это обычная функция (не метод класса), которую запускает дочерний ПРОЦЕСС.
# У процесса своя память — он не видит переменные главного процесса.
# Общение только через multiprocessing.Queue.
def sensor_process_fn(delay: float, data_queue: mp.Queue, stop_event: mp.Event, name: str):
    """
    Дочерний процесс для SensorX.

    mp.Queue — очередь между процессами (в отличие от queue.Queue между потоками).
    mp.Event — событие между процессами (в отличие от threading.Event).

    Под капотом mp.Queue использует pipe + сериализацию (pickle) данных.
    Это дороже чем threading.Queue где данные просто передаются по ссылке.
    """
    sensor = SensorX(delay)  # создаём датчик ВНУТРИ процесса

    while not stop_event.is_set():
        value = sensor.get()

        # Очищаем очередь если переполнена (хотим только свежие данные)
        while not data_queue.empty():
            try:
                data_queue.get_nowait()
            except Exception:
                break

        try:
            data_queue.put_nowait(value)
        except Exception:
            pass


def _compose(frame, sensor_values: dict) -> np.ndarray:
    canvas = frame.copy() if frame is not None else np.zeros((480, 640, 3), dtype=np.uint8)
    h, w = canvas.shape[:2]
    lines = [f"{k}: {v}" for k, v in sensor_values.items()]
    panel_w, line_h, pad = 220, 22, 8
    panel_h = pad + line_h * len(lines) + pad
    x0, y0 = w - panel_w - 8, h - panel_h - 8
    overlay = canvas.copy()
    cv2.rectangle(overlay, (x0, y0), (x0 + panel_w, y0 + panel_h), (0, 0, 0), cv2.FILLED)
    cv2.addWeighted(overlay, 0.55, canvas, 0.45, 0, canvas)
    for i, line in enumerate(lines):
        y = y0 + pad + line_h * i + line_h - 4
        cv2.putText(canvas, line, (x0 + 6, y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1, cv2.LINE_AA)
    return canvas


def _parse_args():
    p = argparse.ArgumentParser(description="Sensor display (multiprocessing version)")
    p.add_argument("--camera", default="/dev/video0")
    p.add_argument("--resolution", default="1280x720")
    p.add_argument("--fps", type=float, default=30.0)
    return p.parse_args()


def main():
    args = _parse_args()
    try:
        w_str, h_str = args.resolution.lower().split("x")
        resolution = (int(w_str), int(h_str))
    except ValueError:
        logger.error("Invalid resolution '%s'", args.resolution)
        sys.exit(1)

    # mp.Event вместо threading.Event — работает между процессами
    stop_event = mp.Event()

    # Создаём очереди и процессы для трёх датчиков SensorX
    # mp.Queue(maxsize=1) — очередь между процессами, только 1 элемент
    sensor_entries = []
    for name, delay in [("Sensor0", 0.01), ("Sensor1", 0.1), ("Sensor2", 1.0)]:
        q = mp.Queue(maxsize=1)
        proc = mp.Process(
            target=sensor_process_fn,
            args=(delay, q, stop_event, name),
            name=f"proc-{name}",
            daemon=True,  # умрёт вместе с главным процессом
        )
        sensor_entries.append((name, q, proc))

    # Камера остаётся в главном процессе (нельзя передать в дочерний)
    cam_sensor = None
    cam_queue = mp.Queue(maxsize=1)
    try:
        cam_sensor = SensorCam(args.camera, resolution)
    except RuntimeError:
        logger.warning("Running without camera")

    try:
        window = WindowImage(args.fps)
    except Exception:
        logger.critical("Cannot initialise display — aborting")
        sys.exit(1)

    # Запускаем дочерние процессы
    processes = []
    for name, q, proc in sensor_entries:
        proc.start()
        processes.append(proc)

    latest_frame = None
    latest = {name: "—" for name, _, _ in sensor_entries}

    logger.info("Running (multiprocessing). Press 'q' or ESC to quit.")

    try:
        while True:
            # Читаем камеру прямо здесь (в главном процессе)
            if cam_sensor is not None:
                frame = cam_sensor.get()
                if frame is not None:
                    latest_frame = frame

            # Читаем данные из очередей дочерних процессов
            for name, q, _ in sensor_entries:
                value = None
                while True:
                    try:
                        value = q.get_nowait()
                    except Exception:
                        break
                if value is not None:
                    latest[name] = value

            img = _compose(latest_frame, latest)
            key = window.show(img)
            if key in (ord("q"), ord("Q"), 27):
                break

    except KeyboardInterrupt:
        logger.info("Interrupted")
    finally:
        stop_event.set()
        for proc in processes:
            proc.join(timeout=3.0)
            if proc.is_alive():
                proc.terminate()  # принудительно убиваем если не остановился
        del window
        if cam_sensor is not None:
            del cam_sensor
        logger.info("Shutdown complete")


if __name__ == "__main__":
    # ОБЯЗАТЕЛЬНО для multiprocessing на Windows и macOS
    # Без этого при создании дочерних процессов main() запустится снова
    mp.freeze_support()
    main()
