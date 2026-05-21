#!/usr/bin/env python3
import argparse
import logging
import queue
import sys
import threading
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
        logging.FileHandler(_LOG_DIR / "app.log"),
        logging.StreamHandler(sys.stderr),
    ],
)
logger = logging.getLogger(__name__)


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
    def __init__(self, device: str, resolution: tuple[int, int]):
        self._log = logging.getLogger(self.__class__.__name__)
        self._device = device
        self._cap: cv2.VideoCapture | None = None

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
        # Запоминаем фактическое разрешение для проверки при переподключении
        self._expected_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self._expected_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self._log.info("Camera '%s' opened at %dx%d", device, self._expected_w, self._expected_h)

    def get(self) -> np.ndarray | None:
        if self._cap is None or not self._cap.isOpened():
            self._log.error("Camera '%s' is not open", self._device)
            return None
        ret, frame = self._cap.read()
        if not ret:
            self._log.error("Failed to read frame from camera '%s'", self._device)
            return None
        return frame

    def reopen(self) -> bool:
        """Переподключается к камере. Проверяет что это та же камера по разрешению."""
        if self._cap is not None:
            self._cap.release()
            self._cap = None
        index = self._parse_index(self._device)
        cap = cv2.VideoCapture(index)
        if not cap.isOpened():
            self._log.error("Reconnect failed for camera '%s'", self._device)
            return False

        # Проверяем что это та же камера — сравниваем разрешение
        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        if actual_w != self._expected_w or actual_h != self._expected_h:
            self._log.warning(
                "Wrong camera at index %d: got %dx%d, expected %dx%d — skipping",
                index, actual_w, actual_h, self._expected_w, self._expected_h,
            )
            cap.release()
            return False

        self._cap = cap
        self._log.info("Reconnected to camera '%s' at %dx%d", self._device, actual_w, actual_h)
        return True

    def __del__(self):
        if self._cap is not None:
            self._cap.release()
            self._log.info("Camera '%s' released", self._device)
            self._cap = None

    @staticmethod
    def _parse_index(device: str) -> int:
        stripped = device.replace("/dev/video", "")
        try:
            return int(stripped)
        except ValueError:
            return 0


class WindowImage:
    _WINDOW = "Sensor Dashboard"

    def __init__(self, fps: float):
        self._log = logging.getLogger(self.__class__.__name__)
        self._wait_ms = max(1, int(1000 / fps))
        try:
            cv2.namedWindow(self._WINDOW, cv2.WINDOW_NORMAL)
        except Exception as exc:
            self._log.error("Cannot create window: %s", exc)
            raise
        self._log.info("Window created at %.1f fps", fps)

    def show(self, img: np.ndarray) -> int:
        try:
            cv2.imshow(self._WINDOW, img)
            return cv2.waitKey(self._wait_ms)
        except Exception as exc:
            self._log.error("imshow failed: %s", exc)
            return -1

    def __del__(self):
        try:
            cv2.destroyWindow(self._WINDOW)
            self._log.info("Window destroyed")
        except Exception:
            pass


_CAM_MAX_RETRIES = 3   # сколько раз пытаемся переподключить камеру
_CAM_RETRY_DELAY = 1.0  # пауза между попытками (секунды)


def _sensor_worker(sensor: Sensor, data_queue: queue.Queue, stop: threading.Event, name: str) -> None:
    log = logging.getLogger(f"worker.{name}")
    log.info("Thread started")

    # Счётчик подряд идущих неудачных кадров (только для камеры)
    fail_count = 0

    while not stop.is_set():
        try:
            value = sensor.get()
        except Exception as exc:
            log.error("sensor.get() raised: %s", exc)
            break

        if value is None:
            if hasattr(sensor, "reopen"):
                fail_count += 1
                log.warning("Camera lost (fail %d/%d)", fail_count, _CAM_MAX_RETRIES)

                if fail_count <= _CAM_MAX_RETRIES:
                    log.info("Reconnect attempt %d/%d in %.1f s...",
                             fail_count, _CAM_MAX_RETRIES, _CAM_RETRY_DELAY)
                    time.sleep(_CAM_RETRY_DELAY)
                    if sensor.reopen():  
                        log.info("Camera restored on attempt %d", fail_count)
                        fail_count = 0   
                else:
                    log.error("Camera unavailable after %d attempts — shutting down", _CAM_MAX_RETRIES)
                    stop.set()
                    
                    break
            else:
                time.sleep(0.01)
            continue

        # Удачный кадр — сбрасываем счётчик ошибок (актуально после успешного reopen)
        fail_count = 0

        if data_queue.full():
            try:
                data_queue.get_nowait()
            except queue.Empty:
                pass
        try:
            data_queue.put_nowait(value)
        except queue.Full:
            pass

    log.info("Thread stopped")


def _compose(frame: np.ndarray | None, sensor_values: dict) -> np.ndarray:
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


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Sensor data acquisition and display")
    p.add_argument("--camera", default="/dev/video0", metavar="DEVICE",
                   help="Camera device (e.g. /dev/video0)")
    p.add_argument("--resolution", default="1280x720", metavar="WxH",
                   help="Camera resolution, e.g. 1280x720")
    p.add_argument("--fps", type=float, default=30.0, metavar="HZ",
                   help="Display refresh rate in Hz")
    return p.parse_args()


def main() -> None:
    args = _parse_args()

    try:
        w_str, h_str = args.resolution.lower().split("x")
        resolution = (int(w_str), int(h_str))
    except ValueError:
        logger.error("Invalid resolution '%s', expected WxH", args.resolution)
        sys.exit(1)

    stop = threading.Event()

    sensor_entries: list[tuple[str, Sensor, queue.Queue]] = []

    for name, delay in [("Sensor0", 0.01), ("Sensor1", 0.1), ("Sensor2", 1.0)]:
        sensor_entries.append((name, SensorX(delay), queue.Queue(maxsize=1)))

    cam_sensor: SensorCam | None = None
    try:
        cam_sensor = SensorCam(args.camera, resolution)
        sensor_entries.append(("Camera", cam_sensor, queue.Queue(maxsize=1)))
    except RuntimeError:
        logger.warning("Running without camera")

    try:
        window = WindowImage(args.fps)
    except Exception:
        logger.critical("Cannot initialise display — aborting")
        sys.exit(1)

    threads: list[threading.Thread] = []
    for name, sensor, q in sensor_entries:
        t = threading.Thread(
            target=_sensor_worker,
            args=(sensor, q, stop, name),
            name=f"sensor-{name}",
            daemon=True,
        )
        t.start()
        threads.append(t)

    latest_frame: np.ndarray | None = None
    latest = {name: "—" for name, _, _ in sensor_entries if name != "Camera"}

    logger.info("Running. Press 'q' or ESC to quit.")

    try:
        while not stop.is_set():
            for name, _sensor, q in sensor_entries:
                value = None
                while True:
                    try:
                        value = q.get_nowait()
                    except queue.Empty:
                        break
                if value is not None:
                    if name == "Camera":
                        latest_frame = value
                    else:
                        latest[name] = value

            img = _compose(latest_frame, latest)
            key = window.show(img)

            if key in (ord("q"), ord("Q"), 27):
                logger.info("Quit key pressed")
                break

    except KeyboardInterrupt:
        logger.info("Interrupted by user")
    finally:
        stop.set()
        for t in threads:
            t.join(timeout=3.0)
        del window
        if cam_sensor is not None:
            del cam_sensor
        logger.info("Shutdown complete")


if __name__ == "__main__":
    main()
