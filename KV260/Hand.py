#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
手掌 / 苹果检测 + 手部关键点检测（KV260 / Linux + Vitis AI）

功能参考 SRC/KV260/06.hand_keypoint_detection.py（原 K230 CanMV 版本）：
  1. 单输出 YOLOv5 手掌检测（hand + apple 两类，输入 640x640，输出 1x25200x7）
  2. 对手掌框裁剪后做 21 关键点检测
  3. 绘制手掌框、苹果框、关键点和手指连线

与 K230 版本的差异（按要求改成 Linux + Vitis AI）：
  - 删除 libs.PipeLine / AIBase / AI2D / nncase_runtime / aicube / aidemo /
    media.media / machine.UART / ulab.numpy 等 K230 专用依赖
  - 摄像头、显示：OpenCV（V4L2 / GStreamer）
  - 推理：Vitis AI DPU（xir + vart，DPU-PYNQ / DPUCZDX8G）
  - 预处理、后处理：NumPy + OpenCV

注意：
  - 需要把 hand_apple_v5_640 / handkp_det 用 Vitis AI 编译成 KV260 DPU 的 .xmodel。
  - 模型输出布局、量化 scale/zero_point、归一化参数与训练/编译参数有关，
    代码里给的是常见默认值，实机使用前按自己的 xmodel 调整。
  - 按要求不保证跑通；本机没有 KV260 环境，未实机验证。
"""

import os
import time
import math
import numpy as np
import cv2

try:
    import xir             # Vitis AI 图接口
    import vart            # Vitis AI 运行时
except ImportError:          # 非 KV260 / 未装 Vitis AI 时，导入阶段不报错
    xir = None
    vart = None


# ============================================================
#  模型与运行参数
# ============================================================

MODEL_DIR = os.environ.get("HAND_MODEL_DIR", "/home/ubuntu/models")
HAND_DET_XMODEL = os.path.join(MODEL_DIR, "hand_apple_v5_640.xmodel")
HAND_KP_XMODEL = os.path.join(MODEL_DIR, "handkp_det.xmodel")

# DPU bitstream：DPU-PYNQ 镜像通常已加载，可留空；
# 纯 Vitis AI 环境可填 /home/ubuntu/dpu.bit 之类。
DPU_BITSTREAM = os.environ.get("DPU_BITSTREAM", "")

# 与原 K230 版本一致的模型输入 (宽, 高)
DET_INPUT_SIZE = (640, 640)
KP_INPUT_SIZE = (256, 256)

# 归一化参数（按训练时的 mean/std 改）
DET_MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
DET_STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)
KP_MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
KP_STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)

# 手掌检测参数
CONF_THRESHOLD = 0.2
NMS_THRESHOLD = 0.5
LABELS = ["hand", "apple"]
STRIDES = (8, 16, 32)
ANCHORS = [26, 27, 53, 52, 75, 71,
           80, 99, 106, 82, 99, 134,
           140, 113, 161, 172, 245, 276]
MAX_DET = 50

# 是否显示 OpenCV 窗口（无桌面环境时设为 False）
SHOW_WINDOW = True


# ============================================================
#  Vitis AI DPU 推理封装
# ============================================================

class DpuRunner:
    """加载一个 xmodel，并在 DPU 上执行异步推理。"""

    def __init__(self, xmodel_path, name=""):
        if xir is None or vart is None:
            raise RuntimeError("未找到 Vitis AI 运行时（xir/vart），请在 KV260 的 Vitis AI 环境中运行")
        if not os.path.exists(xmodel_path):
            raise FileNotFoundError("找不到模型：" + xmodel_path)

        self.name = name
        self.graph = xir.Graph.deserialize(xmodel_path)

        subgraphs = self.graph.get_root_subgraph().toposort_child_subgraph()
        dpu_subgraphs = [g for g in subgraphs
                         if g.has_attr("device") and g.get_attr("device") == "DPU"]
        if not dpu_subgraphs:
            raise RuntimeError("xmodel 中没有 DPU 子图：" + xmodel_path)

        self.runner = vart.Runner.create_runner(dpu_subgraphs[0], "run")
        self.inputs = self.runner.get_input_tensors()
        self.outputs = self.runner.get_output_tensors()

        self.input_scale, self.input_zp = self._quant_params(self.inputs[0])
        self.output_quant = [self._quant_params(t) for t in self.outputs]

    @staticmethod
    def _quant_params(tensor):
        """读取 xmodel 里的量化参数；读不到时返回占位值。"""
        try:
            scale = tensor.get_attr("scale")
            zero_point = tensor.get_attr("zero_point")
            scale = float(scale[0]) if isinstance(scale, (list, tuple)) else float(scale)
            zero_point = int(zero_point[0]) if isinstance(zero_point, (list, tuple)) else int(zero_point)
            return scale, zero_point
        except Exception:
            return 1.0, 0

    def infer(self, input_hwc):
        """input_hwc: [1,H,W,3] 的 int8 张量；自动适配 NCHW/NHWC。"""
        dims = self.inputs[0].dims
        x = input_hwc
        if len(dims) == 4 and dims[1] == 3 and input_hwc.shape[-1] == 3:
            x = np.transpose(input_hwc, (0, 3, 1, 2))

        in_data = [np.empty(t.dims, dtype=np.int8) for t in self.inputs]
        in_data[0][:] = x.reshape(dims)

        out_data = [np.empty(t.dims, dtype=np.int8) for t in self.outputs]
        job_id = self.runner.execute_async(in_data, out_data)
        self.runner.wait(job_id)

        return [d.copy() for d in out_data]

    def dequantize(self, index, data):
        scale, zero_point = self.output_quant[index]
        if scale <= 0:
            scale = 1.0
        return (data.astype(np.float32) - zero_point) * scale


# ============================================================
#  预处理
# ============================================================

def letterbox(img, new_size, color=114):
    """等比缩放 + 居中填充，返回 (图像, 缩放比例, (pad_x, pad_y))。"""
    h, w = img.shape[:2]
    new_w, new_h = new_size
    ratio = min(new_w / float(w), new_h / float(h))
    resized_w, resized_h = int(round(w * ratio)), int(round(h * ratio))
    resized = cv2.resize(img, (resized_w, resized_h), interpolation=cv2.INTER_LINEAR)

    pad_x = (new_w - resized_w) // 2
    pad_y = (new_h - resized_h) // 2
    padded = cv2.copyMakeBorder(resized, pad_y, new_h - resized_h - pad_y,
                                pad_x, new_w - resized_w - pad_x,
                                cv2.BORDER_CONSTANT, value=(color, color, color))
    return padded, ratio, (pad_x, pad_y)


def to_int8_input(img_bgr, size, scale, zero_point, mean, std):
    """BGR -> RGB -> resize -> 归一化 -> 量化成 int8 的 [1,H,W,3]。"""
    rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    if size is not None:
        rgb = cv2.resize(rgb, size, interpolation=cv2.INTER_LINEAR)

    x = rgb.astype(np.float32) / 255.0
    x = (x - mean) / std
    scale = scale if scale > 0 else 1.0
    q = np.clip(np.round(x / scale + zero_point), -128, 127).astype(np.int8)
    return q[None, ...]


def sigmoid(x):
    return 1.0 / (1.0 + np.exp(-np.clip(x, -50.0, 50.0)))


# ============================================================
#  后处理：单输出 YOLOv5
# ============================================================

def yolov5_det_postprocess(output, model_size, strides, anchors,
                           num_classes, ratio, pad, conf_threshold, nms_threshold,
                           max_det=50):
    """
    output: (25200, 7) 的 raw 输出，未过 sigmoid，布局为
            [cx, cy, w, h, obj, cls_prob...]
    model_size: (w, h)
    ratio/pad: letterbox 的缩放比例和填充量

    返回:
      boxes_xywh: (N,4) 原图坐标 [x, y, w, h]
      cls_ids:    (N,)
      scores:     (N,)
    """
    pred = np.asarray(output, dtype=np.float32).reshape(-1, 5 + num_classes)
    model_w, model_h = model_size

    boxes_all = []
    scores_all = []
    cls_all = []

    row_offset = 0
    for head, stride in enumerate(strides):
        grid_h = model_h // stride
        grid_w = model_w // stride
        num_anchors = 3
        count = grid_h * grid_w * num_anchors

        head_pred = pred[row_offset:row_offset + count]
        row_offset += count
        if head_pred.shape[0] != count:
            continue

        # YOLOv5 Detect 输出顺序：anchor -> y -> x -> [cx,cy,w,h,obj,cls...]
        head_pred = head_pred.reshape(num_anchors, grid_h, grid_w, 5 + num_classes)

        anchor_base = head * 6
        anchor_w = np.array([anchors[anchor_base + 2 * i] for i in range(num_anchors)],
                            dtype=np.float32)[:, None, None]
        anchor_h = np.array([anchors[anchor_base + 2 * i + 1] for i in range(num_anchors)],
                            dtype=np.float32)[:, None, None]

        grid_x, grid_y = np.meshgrid(np.arange(grid_w, dtype=np.float32),
                                     np.arange(grid_h, dtype=np.float32))

        tx = head_pred[..., 0]
        ty = head_pred[..., 1]
        tw = head_pred[..., 2]
        th = head_pred[..., 3]

        bx = (sigmoid(tx) * 2.0 - 0.5 + grid_x[None]) * stride
        by = (sigmoid(ty) * 2.0 - 0.5 + grid_y[None]) * stride
        bw = (sigmoid(tw) * 2.0) ** 2 * anchor_w
        bh = (sigmoid(th) * 2.0) ** 2 * anchor_h

        obj = sigmoid(head_pred[..., 4])                       # (A,gh,gw)
        cls_prob = sigmoid(head_pred[..., 5:])                  # (A,gh,gw,C)
        conf = obj[..., None] * cls_prob
        score = np.max(conf, axis=-1)
        cls_id = np.argmax(conf, axis=-1)

        mask = score >= conf_threshold
        if not np.any(mask):
            continue

        # 模型输入坐标 -> 原图坐标（先去 padding，再除以缩放比例）
        cx = (bx[mask] - pad[0]) / ratio
        cy = (by[mask] - pad[1]) / ratio
        ww = bw[mask] / ratio
        hh = bh[mask] / ratio

        boxes_all.append(np.stack([cx - ww / 2.0, cy - hh / 2.0, ww, hh], axis=1))
        scores_all.append(score[mask])
        cls_all.append(cls_id[mask])

    if not boxes_all:
        return np.zeros((0, 4), dtype=np.float32), np.zeros((0,), dtype=np.int32), \
            np.zeros((0,), dtype=np.float32)

    boxes = np.concatenate(boxes_all, axis=0)
    scores = np.concatenate(scores_all, axis=0)
    cls_ids = np.concatenate(cls_all, axis=0)

    # 按类别分别做 NMS
    keep = []
    for c in range(num_classes):
        idx = np.where(cls_ids == c)[0]
        if idx.size == 0:
            continue
        sub_boxes = boxes[idx].tolist()
        sub_scores = scores[idx].tolist()
        picked = cv2.dnn.NMSBoxes(sub_boxes, sub_scores, conf_threshold, nms_threshold)
        if len(picked) > 0:
            keep.extend(idx[np.array(picked).reshape(-1)].tolist())

    if not keep:
        return np.zeros((0, 4), dtype=np.float32), np.zeros((0,), dtype=np.int32), \
            np.zeros((0,), dtype=np.float32)

    keep = keep[:max_det]
    return boxes[keep], cls_ids[keep], scores[keep]


# ============================================================
#  手掌 / 苹果检测
# ============================================================

class HandDetApp:
    def __init__(self, xmodel_path, labels, model_input_size, anchors,
                 confidence_threshold=0.2, nms_threshold=0.5,
                 nms_option=False, strides=(8, 16, 32),
                 mean=DET_MEAN, std=DET_STD, debug_mode=0):
        self.model = DpuRunner(xmodel_path, "hand_det")
        self.labels = labels
        self.model_input_size = model_input_size
        self.anchors = anchors
        self.strides = strides
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.nms_option = nms_option        # 单输出两类，保留参数兼容
        self.mean = mean
        self.std = std
        self.debug_mode = debug_mode

    def run(self, frame):
        """
        返回:
          boxes_xywh: 原图坐标 [x, y, w, h]
          cls_ids:    0=hand, 1=apple
          scores:     置信度
        """
        padded, ratio, pad = letterbox(frame, self.model_input_size)
        tensor = to_int8_input(padded, None,
                               self.model.input_scale, self.model.input_zp,
                               self.mean, self.std)
        raw = self.model.infer(tensor)[0]
        output = self.model.dequantize(0, raw).reshape(-1, 5 + len(self.labels))

        return yolov5_det_postprocess(
            output, self.model_input_size, self.strides, self.anchors,
            len(self.labels), ratio, pad,
            self.confidence_threshold, self.nms_threshold, MAX_DET)


# ============================================================
#  手部关键点检测
# ============================================================

class HandKPDetApp:
    def __init__(self, xmodel_path, model_input_size,
                 mean=KP_MEAN, std=KP_STD, debug_mode=0):
        self.model = DpuRunner(xmodel_path, "hand_kp")
        self.model_input_size = model_input_size
        self.mean = mean
        self.std = std
        self.debug_mode = debug_mode

    @staticmethod
    def get_crop_param(det_box, frame_shape):
        """det_box = [cls, conf, x1, y1, x2, y2]，按 1.26 倍边长取正方形裁剪区。"""
        x1, y1, x2, y2 = det_box[2], det_box[3], det_box[4], det_box[5]
        w, h = x2 - x1, y2 - y1
        length = max(w, h) / 2.0
        cx, cy = (x1 + x2) / 2.0, (y1 + y2) / 2.0
        ratio_num = 1.26 * length

        frame_h, frame_w = frame_shape[:2]
        crop_x = int(max(0, cx - ratio_num))
        crop_y = int(max(0, cy - ratio_num))
        crop_w = int(min(frame_w - 1, cx + ratio_num)) - crop_x + 1
        crop_h = int(min(frame_h - 1, cy + ratio_num)) - crop_y + 1
        return crop_x, crop_y, max(1, crop_w), max(1, crop_h)

    def run(self, frame, det_box):
        """返回 21 个关键点的像素坐标（原图坐标系），失败返回 None。"""
        crop_x, crop_y, crop_w, crop_h = self.get_crop_param(det_box, frame.shape)
        roi = frame[crop_y:crop_y + crop_h, crop_x:crop_x + crop_w]
        if roi.size == 0:
            return None

        tensor = to_int8_input(roi, self.model_input_size,
                               self.model.input_scale, self.model.input_zp,
                               self.mean, self.std)
        raw = self.model.infer(tensor)[0]
        coords = self.model.dequantize(0, raw).reshape(-1).astype(np.float32)

        if coords.size < 21 * 2:
            return None

        # 关键点模型一般输出 0~1 归一化坐标；
        # 若模型直接输出像素坐标，删掉这段即可。
        finite = coords[np.isfinite(coords)]
        if finite.size and finite.max() > 2.0:
            coords[0::2] /= float(self.model_input_size[0])
            coords[1::2] /= float(self.model_input_size[1])

        points = np.zeros_like(coords)
        points[0::2] = coords[0::2] * crop_w + crop_x
        points[1::2] = coords[1::2] * crop_h + crop_y
        return points


# ============================================================
#  检测 + 关键点 + 绘制
# ============================================================

FINGER_COLORS = [(0, 0, 255), (255, 0, 255), (0, 255, 255), (0, 255, 0), (255, 0, 0)]


class HandKeyPointDet:
    def __init__(self, hand_det_xmodel, hand_kp_xmodel,
                 det_input_size, kp_input_size, labels, anchors,
                 confidence_threshold=0.2, nms_threshold=0.5,
                 nms_option=False, strides=(8, 16, 32), debug_mode=0):
        self.hand_det = HandDetApp(hand_det_xmodel, labels,
                                   model_input_size=det_input_size,
                                   anchors=anchors,
                                   confidence_threshold=confidence_threshold,
                                   nms_threshold=nms_threshold,
                                   nms_option=nms_option,
                                   strides=strides,
                                   debug_mode=debug_mode)
        self.hand_kp = HandKPDetApp(hand_kp_xmodel,
                                    model_input_size=kp_input_size,
                                    debug_mode=debug_mode)

    def run(self, frame):
        """
        返回:
          boxes:       手掌框列表，每项 [cls, conf, x1, y1, x2, y2]
          hand_res:    对应的 21 关键点数组列表
          apple_boxes: 苹果框列表
        """
        xywhs, cls_ids, scores = self.hand_det.run(frame)
        frame_h, frame_w = frame.shape[:2]

        boxes = []
        hand_res = []
        apple_boxes = []
        best_hand = None
        best_apple = None

        for i in range(len(cls_ids)):
            cls = int(cls_ids[i])
            conf = float(scores[i])
            x, y, bw, bh = xywhs[i]
            det_box = [cls, conf, x, y, x + bw, y + bh]
            if cls == 0:
                if best_hand is None or conf > best_hand[1]:
                    best_hand = det_box
            elif cls == 1:
                if best_apple is None or conf > best_apple[1]:
                    best_apple = det_box

        # 每类只保留置信度最高的一个（与原版一致）
        if best_apple is not None:
            apple_boxes.append(best_apple)

        if best_hand is not None:
            det_box = best_hand
            x1, y1, x2, y2 = det_box[2], det_box[3], det_box[4], det_box[5]
            w, h = x2 - x1, y2 - y1

            # 与原版一致：丢弃贴边、过小的框
            ok = True
            if h < 0.1 * frame_h:
                ok = False
            if w < 0.25 * frame_w and (x1 < 0.03 * frame_w or x2 > 0.97 * frame_w):
                ok = False
            if w < 0.15 * frame_w and (x1 < 0.01 * frame_w or x2 > 0.99 * frame_w):
                ok = False

            if ok:
                points = self.hand_kp.run(frame, det_box)
                if points is not None:
                    boxes.append(det_box)
                    hand_res.append(points)

        return boxes, hand_res, apple_boxes

    @staticmethod
    def draw_result(frame, dets, hand_res, apple_boxes=None):
        if apple_boxes:
            for det_box in apple_boxes:
                x1, y1, x2, y2 = [int(v) for v in det_box[2:6]]
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 0, 255), 2)
                cv2.putText(frame, "apple %.2f" % det_box[1],
                            (x1, max(20, y1 - 8)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

        for k, det_box in enumerate(dets):
            x1, y1, x2, y2 = [int(v) for v in det_box[2:6]]
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)

            points = hand_res[k]
            for i in range(len(points) // 2):
                cv2.circle(frame, (int(points[2 * i]), int(points[2 * i + 1])),
                           3, (0, 255, 0), -1)

            for i in range(5):
                j = i * 8
                color = FINGER_COLORS[i]
                cv2.line(frame, (int(points[0]), int(points[1])),
                         (int(points[j + 2]), int(points[j + 3])), color, 2)
                cv2.line(frame, (int(points[j + 2]), int(points[j + 3])),
                         (int(points[j + 4]), int(points[j + 5])), color, 2)
                cv2.line(frame, (int(points[j + 4]), int(points[j + 5])),
                         (int(points[j + 6]), int(points[j + 7])), color, 2)
                cv2.line(frame, (int(points[j + 6]), int(points[j + 7])),
                         (int(points[j + 8]), int(points[j + 9])), color, 2)
        return frame


# ============================================================
#  摄像头 / DPU 初始化
# ============================================================

def open_camera():
    pipeline = ("v4l2src device=/dev/video0 ! "
                "video/x-raw,width=640,height=480,framerate=30/1 ! "
                "videoconvert ! appsink")
    cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
    if not cap.isOpened():
        cap = cv2.VideoCapture(0)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    return cap


def load_dpu_bitstream():
    if not DPU_BITSTREAM:
        return
    try:
        from pynq import Overlay
        Overlay(DPU_BITSTREAM)
    except Exception as exc:
        print("[WARN] 加载 DPU bitstream 失败:", exc)


# ============================================================
#  主程序
# ============================================================

def main():
    load_dpu_bitstream()

    hkd = HandKeyPointDet(
        HAND_DET_XMODEL, HAND_KP_XMODEL,
        det_input_size=DET_INPUT_SIZE,
        kp_input_size=KP_INPUT_SIZE,
        labels=LABELS,
        anchors=ANCHORS,
        confidence_threshold=CONF_THRESHOLD,
        nms_threshold=NMS_THRESHOLD,
        nms_option=False,
        strides=STRIDES,
        debug_mode=0)

    cap = open_camera()

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                print("[WARN] 摄像头读取失败")
                break

            det_boxes, hand_res, apple_boxes = hkd.run(frame)
            hkd.draw_result(frame, det_boxes, hand_res, apple_boxes)

            if SHOW_WINDOW:
                cv2.imshow("KV260 Hand", frame)
                if cv2.waitKey(1) & 0xFF in (ord("q"), 27):
                    break
    finally:
        cap.release()
        if SHOW_WINDOW:
            cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
