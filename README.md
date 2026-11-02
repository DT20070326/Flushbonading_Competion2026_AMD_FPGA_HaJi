# Flushbonading_FPGA

FPGA + 嵌入式 + 算法综合项目源码仓库。

## 目录结构

| 目录 | 内容 |
| --- | --- |
| `ESP32/` | ESP32 固件（PlatformIO 工程） |
| `STM32/` | STM32F103 固件（Keil MDK 工程） |
| `YOLO/` | YOLOv5 训练 / 验证脚本、数据集与模型权重 |
| `KV260/` | KV260 端侧视觉部署脚本（Linux + Vitis AI） |
| `HTML/` | 前端应用（uni-app 工程） |
| `PCB/` | 原理图与 PCB（嘉立创 EDA `.eprj2`） |
| `3D/` | 结构件三维模型与打印切片 |
| `ASPRO/` | 专用器件工程文件 |

## 说明

- 构建产物（`ESP32/.pio/`、`STM32/Objects/`、`STM32/Listings/`）已被 `.gitignore` 排除。
- `YOLO/finetune_data_v5/weights/` 中为微调后的模型权重（`.onnx` / `.pt`），已随仓库提交。

## 本地凭据配置

仓库不包含任何真实密钥，首次克隆后需要自行创建：

1. **ESP32**：复制 `ESP32/include/secrets.example.h` 为 `ESP32/include/secrets.h`，
   填入 WiFi SSID / 密码 与 OneNET 产品 `ACCESS_KEY`。
2. **前端**：复制 `HTML/utils/secret.example.js` 为 `HTML/utils/secret.js`，
   填入 OneNET 产品 `ACCESS_KEY`。

这两个真实文件均已在 `.gitignore` 中排除，不会被提交。若密钥曾经泄露，请到 OneNET 控制台重置。
