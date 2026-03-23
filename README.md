# op_ocr_engine

基于 Tesseract 5 的 C++ OCR HTTP 服务，提供简单的健康检查、版本查询和文本识别接口。

## 1) 环境要求

- CMake >= 3.14
- C++17 编译器（Windows 下可用 VS2022）
- 已安装 Tesseract 开发库（例如通过 vcpkg）
- OCR 语言模型（`*.traineddata`）

推荐中文模型：

- `chi_sim.traineddata`
- 下载地址：`https://raw.githubusercontent.com/tesseract-ocr/tessdata/main/chi_sim.traineddata`

## 2) 构建与启动

### 2.1 构建

```bash
cmake -S . -B build_service
cmake --build build_service --config Release
```

构建后可执行文件通常位于：

- `build_service/Release/ocr_server.exe`

### 2.2 准备模型目录

创建 `tessdata` 目录，并放入模型文件，例如：

- `tessdata/chi_sim.traineddata`

### 2.3 启动服务

```bash
build_service/Release/ocr_server.exe --datapath tessdata --lang chi_sim --host 0.0.0.0 --port 8080
```

参数说明：

- `--datapath`：`tessdata` 目录路径（必填）
- `--lang`：语言模型名（默认 `eng`，中文可用 `chi_sim`）
- `--host`：监听地址（默认 `0.0.0.0`）
- `--port`：监听端口（默认 `8080`）

## 3) HTTP 协议

### 3.1 健康检查

- 方法：`GET`
- 路径：`/health`

响应示例：

```json
{"status":"ok"}
```

### 3.2 版本信息

- 方法：`GET`
- 路径：`/api/v1/version`

响应示例：

```json
{"version":"tesseract-ocr-service 1.0 (Tesseract 5.x)"}
```

### 3.3 OCR 识别

- 方法：`POST`
- 路径：`/api/v1/ocr`
- `Content-Type`：`application/json`

请求体字段：

- `image`：图像原始像素字节的 Base64 字符串
- `width`：图像宽度（像素）
- `height`：图像高度（像素）
- `bpp`：每像素字节数，仅支持 `1`（灰度）/`3`（RGB）/`4`（RGBA）

注意：接口接收的是原始像素字节，不是 PNG/JPG 文件本身。

请求示例：

```json
{
  "image": "<base64_of_raw_pixels>",
  "width": 640,
  "height": 480,
  "bpp": 1
}
```

成功响应示例：

```json
{
  "code": 0,
  "results": [
    {
      "text": "你好",
      "bbox": [10, 20, 120, 60],
      "confidence": 91.2
    }
  ]
}
```

失败响应示例：

```json
{
  "code": -1,
  "error": "Missing required fields: image, width, height, bpp"
}
```

常见错误场景（HTTP 400）：

- JSON 非法
- 缺少必要字段
- `bpp` 不在 `1/3/4`
- Base64 解码后字节数与 `width*height*bpp` 不一致

## 4) 调用示例（Python）

以下示例使用 Pillow 把图片转为灰度原始字节并调用接口：

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
    "http://127.0.0.1:8080/api/v1/ocr",
    data=json.dumps(payload).encode("utf-8"),
    headers={"Content-Type": "application/json"},
    method="POST",
)

with urllib.request.urlopen(req, timeout=10) as resp:
    print(resp.read().decode("utf-8"))
```

## 5) 测试

项目已集成 gtest/ctest，可执行：

```bash
ctest --test-dir build_service -C Release --output-on-failure
```

如果要运行集成测试并指定模型路径，可设置：

- `OCR_TESSDATA_PATH`
- `OCR_LANG`

## 6) 说明

- `cpp_infer/` 目录为 PaddleOCR 相关代码，和根目录 Tesseract HTTP 服务是两条链路。

## 7) PaddleOCR Python 服务（FastAPI）

项目新增了一个基于 FastAPI 的 PaddleOCR HTTP 服务，位于：

- `py_paddle_server/app.py`

该服务接口与现有 Tesseract 服务保持兼容：

- `GET /health`
- `GET /api/v1/version`
- `POST /api/v1/ocr`

请求体字段相同：

- `image`：图像原始像素字节的 Base64 字符串
- `width`：图像宽度
- `height`：图像高度
- `bpp`：每像素字节数（支持 `1/3/4`）

注意：这里同样接收原始像素字节，不是 PNG/JPG 文件本体。

快速启动：

```bash
pip install -r py_paddle_server/requirements.txt
uvicorn py_paddle_server.app:app --host 0.0.0.0 --port 8081
```

更多配置和示例见：

- `py_paddle_server/README.md`
