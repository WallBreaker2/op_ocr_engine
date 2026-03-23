# PaddleOCR FastAPI Server

该服务提供与根目录 Tesseract 服务兼容的 HTTP 协议：

- `GET /health`
- `GET /api/v1/version`
- `POST /api/v1/ocr`

OCR 请求体仍为原始像素字节（Base64），不是 PNG/JPG 文件本体。

## 1) 环境准备

- Python 3.10+
- 强烈建议先创建虚拟环境（避免与现有 numpy/matplotlib/numba 冲突）

安装依赖：

```bash
pip install -r py_paddle_server/requirements.txt
```

说明：`paddleocr` 依赖 `paddlepaddle` 运行时，`requirements.txt` 已包含 CPU 版依赖。

如果你当前环境已经被装乱（例如 numpy 被升级到 2.x），建议新建虚拟环境后安装：

```bash
python -m venv .venv-paddle
.venv-paddle\Scripts\activate
python -m pip install --upgrade pip
pip install -r py_paddle_server/requirements.txt
```

确认正在使用虚拟环境解释器：

```bash
python -c "import sys; print(sys.executable)"
```

## 2) 启动服务

```bash
python -m uvicorn py_paddle_server.app:app --host 0.0.0.0 --port 8081
```

或者使用内置启动参数（支持输出识别结果）：

```bash
python -m py_paddle_server.app --host 0.0.0.0 --port 8081 --print-results
```

## 3) 可选环境变量

- `PADDLE_LANG`：识别语言，默认 `ch`
- `PADDLE_USE_GPU`：是否用 GPU，默认 `false`
- `PADDLE_USE_ANGLE_CLS`：是否开启方向分类，默认 `true`
- `PADDLE_SHOW_LOG`：是否打印 Paddle 日志，默认 `false`
- `PADDLE_PRINT_RESULTS`：是否打印每次请求的 OCR 结果，默认 `false`
- `PADDLE_DET_MODEL_DIR`：检测模型目录（可选）
- `PADDLE_REC_MODEL_DIR`：识别模型目录（可选）
- `PADDLE_CLS_MODEL_DIR`：方向分类模型目录（可选）

示例：

```bash
PADDLE_LANG=ch PADDLE_USE_GPU=false python -m uvicorn py_paddle_server.app:app --host 127.0.0.1 --port 8081
```

## 4) 调用示例（Python）

```python
import base64
import json
import urllib.request
from PIL import Image

img = Image.open("input.png").convert("L")
width, height = img.size
raw = img.tobytes()

payload = {
    "image": base64.b64encode(raw).decode("ascii"),
    "width": width,
    "height": height,
    "bpp": 1,
}

req = urllib.request.Request(
    "http://127.0.0.1:8081/api/v1/ocr",
    data=json.dumps(payload).encode("utf-8"),
    headers={"Content-Type": "application/json"},
    method="POST",
)

with urllib.request.urlopen(req, timeout=15) as resp:
    print(resp.read().decode("utf-8"))
```
