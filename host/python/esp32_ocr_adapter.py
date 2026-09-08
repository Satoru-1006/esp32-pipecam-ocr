from __future__ import annotations

import csv
import os
import re
import socket
import sys
import time
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Optional
from urllib.parse import urlencode
from urllib.request import Request, urlopen
from urllib.parse import urlparse

os.environ.setdefault("OPENCV_FFMPEG_CAPTURE_OPTIONS", "stimeout;3000000")
os.environ.setdefault("FLAGS_use_mkldnn", "0")
os.environ.setdefault("FLAGS_use_onednn", "0")

import cv2
import numpy as np
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QTextBrowser,
    QVBoxLayout,
    QWidget,
)


APP_DIR = Path(__file__).resolve().parent
MAINWINDOW_DIR = APP_DIR / "mainwindow"
MODEL_DIR = MAINWINDOW_DIR / "models"
CAPTURE_DIR = APP_DIR / "captures"
EXPORT_DIR = APP_DIR / "exports"
DEFAULT_STREAM_URL = "http://192.168.4.1:8080/stream"
DEFAULT_CONTROL_PORT = 8081
FALLBACK_STREAM_URLS = [
    DEFAULT_STREAM_URL,
    "http://192.168.4.1:81/stream",
    "http://192.168.4.1/stream",
    "http://192.168.4.1:8081/stream",
    "http://192.168.4.1:80/stream",
]
DEFAULT_SSID = "PipeCam-2D55"
DEFAULT_PASSWORD = "12345678"


@dataclass
class OcrResult:
    text: str
    confidence: float


class Esp32StreamThread(QThread):
    frame_ready = Signal(object)
    status = Signal(str)

    def __init__(self, url: str) -> None:
        super().__init__()
        self.url = url
        self._running = True

    def _candidate_urls(self) -> list[str]:
        urls = [self.url]
        if self.url == DEFAULT_STREAM_URL:
            urls.extend(FALLBACK_STREAM_URLS)
        deduped: list[str] = []
        for url in urls:
            if url and url not in deduped:
                deduped.append(url)
        return deduped

    def _tcp_probe(self, url: str) -> tuple[bool, str]:
        parsed = urlparse(url)
        host = parsed.hostname
        if not host:
            return False, "地址格式错误"
        port = parsed.port or (443 if parsed.scheme == "https" else 80)
        try:
            with socket.create_connection((host, port), timeout=2):
                return True, f"{host}:{port} 已打开"
        except OSError as exc:
            return False, f"{host}:{port} 不通：{exc}"

    def stop(self) -> None:
        self._running = False
        self.wait(2000)

    def run(self) -> None:
        while self._running:
            capture = None
            active_url = ""
            for candidate_url in self._candidate_urls():
                if not self._running:
                    return
                ok, probe_message = self._tcp_probe(candidate_url)
                self.status.emit(f"端口检测：{probe_message}")
                if not ok:
                    continue
                self.status.emit(f"正在连接 ESP32 视频流：{candidate_url}")
                trial = cv2.VideoCapture(candidate_url)
                if trial.isOpened():
                    capture = trial
                    active_url = candidate_url
                    break
                trial.release()
                self.status.emit(f"连接失败：{candidate_url}")

            if capture is None:
                self.status.emit(
                    "所有常见视频地址均连接失败。请确认电脑已连接 PipeCam-2D55，"
                    "并在浏览器测试 http://192.168.4.1:81/stream 或 http://192.168.4.1:8080/stream。"
                )
                time.sleep(1.5)
                continue

            self.status.emit(f"视频已连接：{active_url}")
            while self._running:
                ok, frame = capture.read()
                if not ok or frame is None or frame.size == 0:
                    self.status.emit("视频中断，正在重连。")
                    break
                self.frame_ready.emit(frame)
                self.msleep(15)

            capture.release()


class ScanOcr:
    def __init__(self) -> None:
        self._ocr = None
        self._api = "legacy"

    def available(self) -> bool:
        try:
            import paddle  # noqa: F401
            import paddleocr  # noqa: F401
        except Exception:
            return False
        return True

    def _load(self):
        if self._ocr is not None:
            return self._ocr
        if not MODEL_DIR.exists():
            raise FileNotFoundError(f"没有找到模型目录：{MODEL_DIR}")

        try:
            import paddle

            paddle.set_flags({"FLAGS_use_mkldnn": False})
        except Exception:
            pass

        from paddleocr import PaddleOCR

        has_v3_local_models = all(
            (MODEL_DIR / name / "inference.yml").exists()
            for name in ("det", "rec")
        )
        if has_v3_local_models:
            self._ocr = PaddleOCR(
                text_detection_model_dir=str(MODEL_DIR / "det"),
                text_recognition_model_dir=str(MODEL_DIR / "rec"),
                use_doc_orientation_classify=False,
                use_doc_unwarping=False,
                use_textline_orientation=False,
                text_det_unclip_ratio=1.8,
                device="cpu",
                enable_mkldnn=False,
            )
            self._api = "v3"
        else:
            # PaddleOCR 3.x requires inference.yml in local model directories.
            # The bundled models here are old pdmodel/pdiparams format, so use
            # the official PP-OCRv4 runtime model cached by PaddleOCR instead.
            self._ocr = PaddleOCR(
                use_doc_orientation_classify=False,
                use_doc_unwarping=False,
                use_textline_orientation=False,
                text_det_unclip_ratio=1.8,
                device="cpu",
                enable_mkldnn=False,
                lang="ch",
                ocr_version="PP-OCRv4",
            )
            self._api = "v3"
        return self._ocr

    def recognize(self, frame: np.ndarray) -> OcrResult:
        ocr = self._load()
        if self._api == "v3" and hasattr(ocr, "predict"):
            result = ocr.predict(frame)
        else:
            result = ocr.ocr(frame, cls=False)
        return self._parse(result)

    def _parse(self, result) -> OcrResult:
        texts: list[str] = []
        scores: list[float] = []

        def visit(item) -> None:
            if item is None:
                return
            if isinstance(item, dict):
                if "res" in item:
                    visit(item["res"])
                    return
                rec_texts = item.get("rec_texts")
                rec_scores = item.get("rec_scores")
                if rec_texts is not None:
                    texts.extend(str(text) for text in rec_texts)
                    if rec_scores is not None:
                        scores.extend(float(score) for score in rec_scores)
                    return
                for child in item.values():
                    visit(child)
                return
            if hasattr(item, "json"):
                try:
                    visit(item.json)
                    return
                except Exception:
                    pass
            if isinstance(item, tuple) and len(item) >= 2 and isinstance(item[0], str):
                texts.append(item[0])
                try:
                    scores.append(float(item[1]))
                except (TypeError, ValueError):
                    pass
                return
            if isinstance(item, list):
                if len(item) >= 2 and isinstance(item[1], tuple) and isinstance(item[1][0], str):
                    texts.append(item[1][0])
                    try:
                        scores.append(float(item[1][1]))
                    except (TypeError, ValueError):
                        pass
                    return
                for child in item:
                    visit(child)

        visit(result)
        text = re.sub(r"\W", "", "".join(texts))
        confidence = sum(scores) / len(scores) if scores else 0.0
        return OcrResult(text=text, confidence=confidence)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        CAPTURE_DIR.mkdir(exist_ok=True)
        EXPORT_DIR.mkdir(exist_ok=True)

        self.setWindowTitle("ESP32 字符识别上位机")
        self.resize(1080, 720)

        self.stream_thread: Optional[Esp32StreamThread] = None
        self.current_frame: Optional[np.ndarray] = None
        self.ocr = ScanOcr()

        root = QWidget()
        self.setCentralWidget(root)
        layout = QVBoxLayout(root)

        top = QHBoxLayout()
        self.url_input = QLineEdit(DEFAULT_STREAM_URL)
        self.connect_btn = QPushButton("连接ESP32视频")
        self.ocr_btn = QPushButton("字符识别")
        self.save_btn = QPushButton("保存当前画面")
        top.addWidget(QLabel("视频地址"))
        top.addWidget(self.url_input, 1)
        top.addWidget(self.connect_btn)
        top.addWidget(self.ocr_btn)
        top.addWidget(self.save_btn)
        layout.addLayout(top)

        body = QHBoxLayout()
        self.video_label = QLabel("请先连接电脑 Wi-Fi 到 ESP32 热点 PipeCam-2D55")
        self.video_label.setAlignment(Qt.AlignCenter)
        self.video_label.setMinimumSize(720, 480)
        self.video_label.setStyleSheet("background:#101418;color:#e5e7eb;border:1px solid #333;")
        body.addWidget(self.video_label, 1)

        side = QVBoxLayout()
        self.result_input = QLineEdit()
        self.result_input.setReadOnly(True)
        self.log = QTextBrowser()
        side.addWidget(QLabel("识别结果"))
        side.addWidget(self.result_input)
        side.addWidget(QLabel("运行日志"))
        side.addWidget(self.log, 1)
        body.addLayout(side)
        layout.addLayout(body, 1)

        self.connect_btn.clicked.connect(self.toggle_stream)
        self.ocr_btn.clicked.connect(self.run_ocr)
        self.save_btn.clicked.connect(self.save_frame)
        self.log.append(f"设备逻辑：ESP32 开热点，电脑连接 {DEFAULT_SSID} 后运行本软件。")
        self.log.append(f"默认热点密码：{DEFAULT_PASSWORD}")
        self.log.append(f"OCR 模型目录：{MODEL_DIR}")

    def toggle_stream(self) -> None:
        if self.stream_thread and self.stream_thread.isRunning():
            self.stream_thread.stop()
            self.stream_thread = None
            self.connect_btn.setText("连接ESP32视频")
            self.log.append("视频已断开。")
            return

        self.stream_thread = Esp32StreamThread(self.url_input.text().strip() or DEFAULT_STREAM_URL)
        self.stream_thread.frame_ready.connect(self.update_frame)
        self.stream_thread.status.connect(self.log.append)
        self.stream_thread.start()
        self.connect_btn.setText("断开ESP32视频")

    def update_frame(self, frame: np.ndarray) -> None:
        self.current_frame = frame
        rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = rgb.shape
        image = QImage(rgb.data, w, h, ch * w, QImage.Format_RGB888)
        pixmap = QPixmap.fromImage(image).scaled(
            self.video_label.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation
        )
        self.video_label.setPixmap(pixmap)

    def run_ocr(self) -> None:
        if self.current_frame is None:
            QMessageBox.warning(self, "字符识别", "当前没有视频画面。")
            return
        if not self.ocr.available():
            QMessageBox.warning(
                self,
                "缺少OCR依赖",
                "当前电脑还没有安装 paddleocr / paddlepaddle。\n"
                "请先运行 C:\\Users\\86198\\Desktop\\scan\\安装OCR依赖.bat",
            )
            return

        self.post_esp32_result("processing")
        self.log.append("正在识别当前画面...")
        QApplication.processEvents()
        try:
            result = self.ocr.recognize(self.current_frame.copy())
        except Exception as exc:
            QMessageBox.warning(self, "字符识别失败", str(exc))
            self.log.append(f"字符识别失败：{exc}")
            self.post_esp32_result("ng", reason="OCR ERROR")
            return

        self.result_input.setText(result.text)
        self.log.append(f"识别完成：{result.text or '未识别到字符'}，置信度：{result.confidence:.4f}")
        if result.text:
            self.post_esp32_result(
                "ok",
                text=result.text,
                confidence=f"{result.confidence * 100:.1f}%",
            )
        else:
            self.post_esp32_result("ng", reason="NO VALID TEXT")
        self.export_result(result)

    def post_esp32_result(
        self,
        state: str,
        *,
        text: str = "",
        confidence: str = "",
        reason: str = "",
    ) -> None:
        stream_url = self.url_input.text().strip() or DEFAULT_STREAM_URL
        parsed = urlparse(stream_url)
        if not parsed.hostname:
            return
        port = DEFAULT_CONTROL_PORT
        result_url = f"{parsed.scheme or 'http'}://{parsed.hostname}:{port}/result"
        payload = urlencode(
            {
                "state": state,
                "text": text,
                "confidence": confidence,
                "reason": reason,
            }
        ).encode("utf-8")
        try:
            req = Request(result_url, data=payload, method="POST")
            req.add_header("Content-Type", "application/x-www-form-urlencoded")
            with urlopen(req, timeout=1.5):
                pass
        except Exception as exc:
            self.log.append(f"结果回传到 ESP32 失败：{exc}")

    def save_frame(self) -> None:
        if self.current_frame is None:
            QMessageBox.warning(self, "保存画面", "当前没有视频画面。")
            return
        path = CAPTURE_DIR / f"frame_{datetime.now():%Y%m%d_%H%M%S}.jpg"
        cv2.imwrite(str(path), self.current_frame)
        self.log.append(f"已保存：{path}")

    def export_result(self, result: OcrResult) -> None:
        path = EXPORT_DIR / "scan_results.csv"
        exists = path.exists()
        with path.open("a", newline="", encoding="utf-8-sig") as f:
            writer = csv.writer(f)
            if not exists:
                writer.writerow(["时间", "识别结果", "置信度"])
            writer.writerow([datetime.now().strftime("%Y-%m-%d %H:%M:%S"), result.text, f"{result.confidence:.4f}"])

    def closeEvent(self, event) -> None:  # type: ignore[override]
        if self.stream_thread and self.stream_thread.isRunning():
            self.stream_thread.stop()
        event.accept()


def main() -> int:
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
