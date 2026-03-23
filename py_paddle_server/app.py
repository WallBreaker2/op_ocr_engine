import base64
import argparse
import os
from threading import Lock
from typing import Any, Dict, List

import cv2
import numpy as np
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

try:
    from paddleocr import PaddleOCR, __version__ as paddleocr_version
    _paddle_import_error: Exception | None = None
except Exception as exc:  # pragma: no cover
    PaddleOCR = None  # type: ignore[assignment]
    paddleocr_version = "unknown"
    _paddle_import_error = exc


def _parse_bool(value: str, default: bool) -> bool:
    if value == "":
        return default
    normalized = value.strip().lower()
    return normalized in {"1", "true", "yes", "on"}


def _optional_env(name: str) -> str:
    value = os.getenv(name, "")
    return value.strip()


def _build_engine() -> Any:
    if PaddleOCR is None:
        raise RuntimeError(
            "Failed to import PaddleOCR runtime dependency. "
            "Please install paddlepaddle first (CPU: pip install \"paddlepaddle>=2.6.2,<3.0.0\"). "
            f"Original error: {_paddle_import_error}"
        )

    lang = os.getenv("PADDLE_LANG", "ch")
    use_gpu = _parse_bool(os.getenv("PADDLE_USE_GPU", ""), default=False)
    use_angle_cls = _parse_bool(os.getenv("PADDLE_USE_ANGLE_CLS", ""), default=True)
    show_log = _parse_bool(os.getenv("PADDLE_SHOW_LOG", ""), default=False)

    kwargs: Dict[str, Any] = {
        "lang": lang,
        "use_gpu": use_gpu,
        "use_angle_cls": use_angle_cls,
        "show_log": show_log,
    }

    det_model_dir = _optional_env("PADDLE_DET_MODEL_DIR")
    rec_model_dir = _optional_env("PADDLE_REC_MODEL_DIR")
    cls_model_dir = _optional_env("PADDLE_CLS_MODEL_DIR")

    if det_model_dir:
        kwargs["det_model_dir"] = det_model_dir
    if rec_model_dir:
        kwargs["rec_model_dir"] = rec_model_dir
    if cls_model_dir:
        kwargs["cls_model_dir"] = cls_model_dir

    return PaddleOCR(**kwargs)


def _error_response(message: str, status_code: int = 400) -> JSONResponse:
    return JSONResponse(status_code=status_code, content={"code": -1, "error": message})


def _parse_ocr_items(raw_output: Any) -> List[Dict[str, Any]]:
    if raw_output is None:
        return []

    lines = raw_output
    if isinstance(raw_output, list) and raw_output and isinstance(raw_output[0], list):
        lines = raw_output[0]

    results: List[Dict[str, Any]] = []
    for line in lines:
        if not isinstance(line, list) or len(line) < 2:
            continue

        box = line[0]
        text_info = line[1]

        if not isinstance(box, list) or len(box) < 4:
            continue
        if not isinstance(text_info, (list, tuple)) or len(text_info) < 2:
            continue

        xs: List[int] = []
        ys: List[int] = []
        for point in box:
            if not isinstance(point, (list, tuple)) or len(point) < 2:
                continue
            xs.append(int(round(point[0])))
            ys.append(int(round(point[1])))

        if not xs or not ys:
            continue

        results.append(
            {
                "text": str(text_info[0]),
                "bbox": [min(xs), min(ys), max(xs), max(ys)],
                "confidence": float(text_info[1]),
            }
        )

    return results


def _decode_and_convert_image(image_b64: str, width: int, height: int, bpp: int) -> np.ndarray:
    try:
        image_data = base64.b64decode(image_b64, validate=True)
    except Exception as exc:
        raise ValueError(f"Invalid base64 image: {exc}") from exc

    expected_size = width * height * bpp
    if len(image_data) != expected_size:
        raise ValueError(
            f"Image data size mismatch. Expected {expected_size} bytes, got {len(image_data)}"
        )

    pixels = np.frombuffer(image_data, dtype=np.uint8)
    if bpp == 1:
        gray = pixels.reshape((height, width))
        return cv2.cvtColor(gray, cv2.COLOR_GRAY2BGR)
    if bpp == 3:
        rgb = pixels.reshape((height, width, 3))
        return cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)

    rgba = pixels.reshape((height, width, 4))
    return cv2.cvtColor(rgba, cv2.COLOR_RGBA2BGR)


class ServiceState:
    def __init__(self) -> None:
        self.engine: Any = None
        self.print_results = False
        self.lock = Lock()


state = ServiceState()
app = FastAPI(title="paddle-ocr-service", version="1.0")


@app.on_event("startup")
def _startup() -> None:
    state.print_results = _parse_bool(os.getenv("PADDLE_PRINT_RESULTS", ""), default=False)
    state.engine = _build_engine()


@app.get("/health")
def health() -> Dict[str, str]:
    return {"status": "ok"}


@app.get("/api/v1/version")
def version() -> Dict[str, str]:
    return {"version": f"paddle-ocr-service 1.0 (PaddleOCR {paddleocr_version})"}


@app.post("/api/v1/ocr")
async def ocr_endpoint(request: Request) -> JSONResponse:
    try:
        body = await request.json()
    except Exception as exc:
        return _error_response(f"Invalid JSON: {exc}")

    required_fields = ("image", "width", "height", "bpp")
    if not all(field in body for field in required_fields):
        return _error_response("Missing required fields: image, width, height, bpp")

    try:
        image_b64 = str(body["image"])
        width = int(body["width"])
        height = int(body["height"])
        bpp = int(body["bpp"])
    except Exception:
        return _error_response("Invalid field types for image/width/height/bpp")

    if width <= 0 or height <= 0:
        return _error_response("width and height must be positive integers")
    if bpp not in (1, 3, 4):
        return _error_response("bpp must be 1, 3, or 4")

    try:
        image = _decode_and_convert_image(image_b64, width, height, bpp)
    except ValueError as exc:
        return _error_response(str(exc))

    if state.engine is None:
        return _error_response("OCR engine is not initialized", status_code=500)

    try:
        with state.lock:
            raw_output = state.engine.ocr(image, cls=True)
    except Exception as exc:
        return _error_response(f"Internal OCR error: {exc}", status_code=500)

    results = _parse_ocr_items(raw_output)
    if state.print_results:
        print(f"[OCR] result_count={len(results)}")
        for item in results:
            print(
                f"[OCR] text={item['text']!r} conf={item['confidence']:.4f} bbox={item['bbox']}"
            )
    return JSONResponse(status_code=200, content={"code": 0, "results": results})


def _main() -> None:
    parser = argparse.ArgumentParser(description="PaddleOCR FastAPI server")
    parser.add_argument("--host", default="0.0.0.0", help="HTTP listen host")
    parser.add_argument("--port", type=int, default=8081, help="HTTP listen port")
    parser.add_argument(
        "--print-results",
        action="store_true",
        help="Print OCR results to stdout for each request",
    )
    parser.add_argument(
        "--reload",
        action="store_true",
        help="Enable auto reload (development only)",
    )
    args = parser.parse_args()

    if args.print_results:
        os.environ["PADDLE_PRINT_RESULTS"] = "true"

    import uvicorn

    uvicorn.run("py_paddle_server.app:app", host=args.host, port=args.port, reload=args.reload)


if __name__ == "__main__":
    _main()
