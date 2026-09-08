# This Python file uses the following encoding: utf-8

from pathlib import Path
import logging
import os
import re
import sys
import tempfile
import time
import traceback

logging.disable(logging.DEBUG)
logging.disable(logging.WARNING)

MODULE_DIR = Path(__file__).resolve().parent
MODULE_LOG_PATH = MODULE_DIR / "ocr_debug.log"


def module_log(message):
    try:
        with MODULE_LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(message + "\n")
    except Exception:
        pass


module_log(f"module imported from {__file__}")


def add_runtime_paths():
    candidates = [
        MODULE_DIR,
        MODULE_DIR / "DLLs",
        MODULE_DIR / "Lib" / "site-packages",
        MODULE_DIR / "hunhe",
        MODULE_DIR / "hunhe" / "DLLs",
        MODULE_DIR / "hunhe" / "Lib" / "site-packages",
        MODULE_DIR.parent,
        MODULE_DIR.parent / "DLLs",
        MODULE_DIR.parent / "Lib" / "site-packages",
        MODULE_DIR.parent / "hunhe",
        MODULE_DIR.parent / "hunhe" / "DLLs",
        MODULE_DIR.parent / "hunhe" / "Lib" / "site-packages",
    ]
    for path in candidates:
        if path.exists():
            path_text = str(path)
            if path_text not in sys.path:
                sys.path.insert(0, path_text)
            if hasattr(os, "add_dll_directory"):
                try:
                    os.add_dll_directory(path_text)
                except OSError:
                    pass


add_runtime_paths()


class Rec_Text:
    def __init__(self):
        base_dir = MODULE_DIR
        self.log_path = base_dir / "ocr_debug.log"
        self._log(f"loading module from {__file__}")
        model_dir = base_dir / "models"
        self.init_error = ""
        try:
            from paddleocr import PaddleOCR

            self.ocr = PaddleOCR(
                det_model_dir=str(model_dir / "det"),
                rec_model_dir=str(model_dir / "rec"),
                rec_char_dict_path=str(model_dir / "dict.txt"),
                cls_model_dir=str(model_dir / "cls"),
                use_angle_cls=True,
                det_db_unclip_ratio=2.4,
                show_log=False,
            )
            self.api = "legacy"
            self._log("PaddleOCR initialized with legacy local models")
        except ValueError:
            self.ocr = PaddleOCR(
                use_doc_orientation_classify=False,
                use_doc_unwarping=False,
                use_textline_orientation=True,
                text_det_unclip_ratio=2.4,
                lang="ch",
                ocr_version="PP-OCRv4",
                device="cpu",
                enable_mkldnn=False,
            )
            self.api = "v3"
            self._log("PaddleOCR initialized with v3 compatible API")
        except Exception:
            self.ocr = None
            self.api = "error"
            self.init_error = traceback.format_exc()
            self._log(self.init_error)

    def recognize_text(self, pic):
        self._log(f"recognize_text input={pic}")
        if self.ocr is None:
            self._log("OCR unavailable")
            return ""

        try:
            import cv2
            import numpy as np

            image = cv2.imdecode(np.fromfile(pic, dtype=np.uint8), cv2.IMREAD_COLOR)
            if image is None:
                image = cv2.imread(pic)
            if image is None:
                self._log("image load failed")
                return ""
        except Exception:
            self._log(traceback.format_exc())
            return ""

        start_time = time.time()
        best_text = ""
        candidates = {}
        try:
            for index, (name, candidate, base_score) in enumerate(self._candidate_images(image)):
                result = self._ocr_image(candidate)
                items = self._parse_result_items(result)
                texts = [item["text"] for item in items]
                text = self._pick_code(texts)
                if len(text) == 15:
                    text_roi = self._crop_text_roi(candidate, items, text)
                    text = self._correct_1_to_7_by_glyph(text, text_roi)
                if text:
                    self._log(f"variant={index} name={name} texts={texts} picked={text}")
                if len(text) == 15:
                    score = base_score + self._candidate_score(text, index)
                    candidates[text] = candidates.get(text, 0) + score
                    if name in ("tight_enhanced", "tight_binary") and score >= 10.0:
                        self._log(f"fast_success={text} name={name} elapsed={time.time() - start_time:.2f}s")
                        return text
                if len(text) > len(best_text):
                    best_text = text
        except Exception:
            self._log(traceback.format_exc())

        if candidates:
            winner = max(candidates.items(), key=lambda item: item[1])[0]
            self._log(f"success={winner} candidates={candidates} elapsed={time.time() - start_time:.2f}s")
            return winner

        self._log(f"fallback={best_text}")
        return best_text

    def _log(self, message):
        try:
            with self.log_path.open("a", encoding="utf-8") as f:
                f.write(message + "\n")
        except Exception:
            pass

    def _ocr_image(self, image):
        import cv2

        if self.api == "v3" and hasattr(self.ocr, "predict"):
            return self.ocr.predict(image)

        with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as tmp:
            tmp_path = Path(tmp.name)
        try:
            cv2.imencode(".png", image)[1].tofile(str(tmp_path))
            return self.ocr.ocr(str(tmp_path), cls=True)
        finally:
            try:
                tmp_path.unlink()
            except OSError:
                pass

    def _candidate_images(self, image):
        import cv2

        variants = []

        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        h, w = gray.shape[:2]

        crops = []
        tight = self._digit_band_crop(gray)
        if tight is not None:
            crops.append(("tight", tight, 8.0))

        # Do not run OCR on the full frame. Background bottles, desktop texture
        # and shadows slow recognition and can outvote the real stamped code.
        for y0, y1 in (
            (int(h * 0.30), int(h * 0.78)),
            (int(h * 0.38), int(h * 0.70)),
        ):
            if y1 - y0 > 24:
                crops.append(("band", gray[y0:y1, :], 3.0))

        if not crops:
            self._log("no code-region crop available")
            return []

        for prefix, crop, crop_score in crops:
            for name, processed, extra_score in self._preprocess_number_roi(crop):
                variants.append((f"{prefix}_{name}", cv2.cvtColor(processed, cv2.COLOR_GRAY2BGR), crop_score + extra_score))
        return variants

    @staticmethod
    def _preprocess_number_roi(roi_gray):
        import cv2

        if roi_gray is None or roi_gray.size == 0:
            return []

        scale = 2.0
        h, w = roi_gray.shape[:2]
        if w < 700:
            scale = max(scale, min(3.0, 1000 / max(w, 1)))
        gray = cv2.resize(roi_gray, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC)

        clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))
        gray = clahe.apply(gray)
        gray = cv2.GaussianBlur(gray, (3, 3), 0)

        bin_img = cv2.adaptiveThreshold(
            gray,
            255,
            cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
            cv2.THRESH_BINARY,
            31,
            5,
        )

        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (2, 1))
        connected = cv2.morphologyEx(bin_img, cv2.MORPH_CLOSE, kernel, iterations=1)

        return [
            ("clahe", gray, 4.0),
            ("adaptive", bin_img, 3.0),
            ("adaptive_inv", 255 - bin_img, 2.4),
            ("connected", connected, 2.0),
        ]

    def _correct_1_to_7_by_glyph(self, code, image):
        if "1" not in code or len(code) != 15:
            return code

        import cv2
        import numpy as np

        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY) if len(image.shape) == 3 else image.copy()
        enhanced = self._enhance_gray(gray)

        # Bright embossed digits are easier to segment by local contrast than by
        # global thresholding. Keep both bright strokes and dark grooves.
        blur = cv2.GaussianBlur(enhanced, (0, 0), 2.0)
        contrast = cv2.absdiff(enhanced, blur)
        contrast = cv2.normalize(contrast, None, 0, 255, cv2.NORM_MINMAX)
        _, mask = cv2.threshold(contrast, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        mask = cv2.morphologyEx(
            mask,
            cv2.MORPH_CLOSE,
            cv2.getStructuringElement(cv2.MORPH_RECT, (2, 2)),
            iterations=1,
        )

        edges = cv2.Canny(enhanced, 35, 120)
        foreground = cv2.bitwise_or(mask, edges)
        ys, xs = np.where(foreground > 0)
        if len(xs) < 40:
            return code

        x0 = max(int(xs.min()) - 4, 0)
        x1 = min(int(xs.max()) + 5, gray.shape[1])
        y0 = max(int(ys.min()) - 4, 0)
        y1 = min(int(ys.max()) + 5, gray.shape[0])
        if x1 - x0 < 90 or y1 - y0 < 18:
            return code

        roi_mask = foreground[y0:y1, x0:x1]
        char_w = roi_mask.shape[1] / 15.0
        corrected = list(code)
        for i, ch in enumerate(corrected):
            if ch != "1":
                continue

            cx0 = max(int(i * char_w), 0)
            cx1 = min(int((i + 1) * char_w), roi_mask.shape[1])
            cell = roi_mask[:, cx0:cx1]
            if cell.shape[0] < 18 or cell.shape[1] < 4:
                continue

            top = cell[:max(4, int(cell.shape[0] * 0.36)), :]
            top_cols = np.count_nonzero(top, axis=0)
            top_span = np.count_nonzero(top_cols > 0) / max(float(cell.shape[1]), 1.0)
            top_density = np.count_nonzero(top) / max(float(top.size), 1.0)

            mid = cell[int(cell.shape[0] * 0.25):int(cell.shape[0] * 0.90), :]
            mid_cols = np.count_nonzero(mid, axis=0)
            vertical_span = np.count_nonzero(mid_cols > 0) / max(float(cell.shape[1]), 1.0)

            left_top = top[:, :max(1, int(top.shape[1] * 0.45))]
            right_top = top[:, int(top.shape[1] * 0.45):]
            diagonal_bias = (
                np.count_nonzero(left_top) - np.count_nonzero(right_top)
            ) / max(float(np.count_nonzero(top)), 1.0)

            if (
                top_span >= 0.34
                and top_density >= 0.055
                and (top_span > vertical_span * 1.08 or diagonal_bias >= 0.18)
            ):
                corrected[i] = "7"
                self._log(
                    f"glyph_correct index={i} 1->7 top_span={top_span:.2f} "
                    f"top_density={top_density:.2f} vertical_span={vertical_span:.2f} "
                    f"diagonal_bias={diagonal_bias:.2f}"
                )

        return "".join(corrected)

    def _crop_text_roi(self, image, items, code):
        import cv2
        import numpy as np

        for item in items:
            digits = re.sub(r"\D", "", item.get("text", ""))
            box = item.get("box")
            if code not in digits or box is None:
                continue
            try:
                points = np.array(box, dtype=np.float32)
                x0 = max(int(points[:, 0].min()) - 8, 0)
                y0 = max(int(points[:, 1].min()) - 8, 0)
                x1 = min(int(points[:, 0].max()) + 9, image.shape[1])
                y1 = min(int(points[:, 1].max()) + 9, image.shape[0])
                if x1 - x0 >= 90 and y1 - y0 >= 18:
                    return image[y0:y1, x0:x1]
            except Exception:
                pass

        gray = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY) if len(image.shape) == 3 else image.copy()
        enhanced = self._enhance_gray(gray)
        edges = cv2.Canny(enhanced, 35, 120)
        ys, xs = np.where(edges > 0)
        if len(xs) < 40:
            return image
        x0 = max(int(xs.min()) - 8, 0)
        y0 = max(int(ys.min()) - 8, 0)
        x1 = min(int(xs.max()) + 9, image.shape[1])
        y1 = min(int(ys.max()) + 9, image.shape[0])
        return image[y0:y1, x0:x1]

    @staticmethod
    def _digit_band_crop(gray):
        import cv2

        h, w = gray.shape[:2]
        band = gray[int(h * 0.34):int(h * 0.72), :]
        if band.size == 0:
            return None

        enhanced = cv2.createCLAHE(clipLimit=2.5, tileGridSize=(8, 8)).apply(band)
        edges = cv2.Canny(enhanced, 40, 120)
        kernel = cv2.getStructuringElement(cv2.MORPH_RECT, (5, 3))
        edges = cv2.dilate(edges, kernel, iterations=1)
        contours, _ = cv2.findContours(edges, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        boxes = []
        for contour in contours:
            x, y, bw, bh = cv2.boundingRect(contour)
            area = bw * bh
            if 6 <= bw <= w * 0.12 and 18 <= bh <= band.shape[0] * 0.75 and area >= 120:
                boxes.append((x, y, bw, bh))
        if len(boxes) < 6:
            return None

        xs = [x for x, _, _, _ in boxes]
        ys = [y for _, y, _, _ in boxes]
        x2s = [x + bw for x, _, bw, _ in boxes]
        y2s = [y + bh for _, y, _, bh in boxes]
        x0 = max(min(xs) - 18, 0)
        y0 = max(min(ys) - 12, 0)
        x1 = min(max(x2s) + 18, band.shape[1])
        y1 = min(max(y2s) + 12, band.shape[0])
        if x1 - x0 < 120 or y1 - y0 < 24:
            return None
        return band[y0:y1, x0:x1]

    @staticmethod
    def _enhance_gray(gray):
        import cv2

        if gray.shape[1] < 700:
            scale = min(3.0, 1000 / max(gray.shape[1], 1))
            gray = cv2.resize(gray, None, fx=scale, fy=scale, interpolation=cv2.INTER_CUBIC)
        clahe = cv2.createCLAHE(clipLimit=3.0, tileGridSize=(8, 8))
        enhanced = clahe.apply(gray)
        blur = cv2.GaussianBlur(enhanced, (0, 0), 1.2)
        sharp = cv2.addWeighted(enhanced, 1.8, blur, -0.8, 0)
        return cv2.normalize(sharp, None, 0, 255, cv2.NORM_MINMAX)

    def _parse_result(self, result):
        return [item["text"] for item in self._parse_result_items(result)]

    def _parse_result_items(self, result):
        items = []

        def visit(item):
            if item is None:
                return
            if isinstance(item, dict):
                if "res" in item:
                    visit(item["res"])
                    return
                rec_texts = item.get("rec_texts")
                if rec_texts is not None:
                    items.extend({"text": str(text), "box": None} for text in rec_texts)
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
                items.append({"text": item[0], "box": None})
                return
            if isinstance(item, list):
                if (
                    len(item) >= 2
                    and isinstance(item[1], tuple)
                    and len(item[1]) >= 1
                    and isinstance(item[1][0], str)
                ):
                    items.append({"text": item[1][0], "box": item[0]})
                    return
                for child in item:
                    visit(child)

        visit(result)
        return items

    @staticmethod
    def _pick_code(texts):
        per_line_digits = [re.sub(r"\D", "", str(text)) for text in texts]
        for digits in per_line_digits:
            match = re.search(r"\d{15}", digits)
            if match:
                return match.group(0)

        digits = "".join(per_line_digits)
        match = re.search(r"\d{15}", digits)
        if match:
            return match.group(0)
        return max(per_line_digits, key=len, default=digits)

    @staticmethod
    def _candidate_score(raw, variant_index):
        score = 4.0 if len(raw) == 15 else 2.0
        if variant_index == 0:
            score += 0.5
        return score


if __name__ == "__main__":
    recognizer = Rec_Text()
    print(recognizer.recognize_text(str(Path(__file__).resolve().parent / "temp_image.png")))
