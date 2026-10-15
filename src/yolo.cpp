#include "yolo.h"

#include "onnx_session.h"

#include <array>

namespace eyes {

namespace {

constexpr int kInput = 640;
constexpr std::array<const char*, 80> kCoco = {
    "person",        "bicycle",      "car",
    "motorcycle",    "airplane",     "bus",
    "train",         "truck",        "boat",
    "traffic light", "fire hydrant", "stop sign",
    "parking meter", "bench",        "bird",
    "cat",           "dog",          "horse",
    "sheep",         "cow",          "elephant",
    "bear",          "zebra",        "giraffe",
    "backpack",      "umbrella",     "handbag",
    "tie",           "suitcase",     "frisbee",
    "skis",          "snowboard",    "sports ball",
    "kite",          "baseball bat", "baseball glove",
    "skateboard",    "surfboard",    "tennis racket",
    "bottle",        "wine glass",   "cup",
    "fork",          "knife",        "spoon",
    "bowl",          "banana",       "apple",
    "sandwich",      "orange",       "broccoli",
    "carrot",        "hot dog",      "pizza",
    "donut",         "cake",         "chair",
    "couch",         "potted plant", "bed",
    "dining table",  "toilet",       "tv",
    "laptop",        "mouse",        "remote",
    "keyboard",      "cell phone",   "microwave",
    "oven",          "toaster",      "sink",
    "refrigerator",  "book",         "clock",
    "vase",          "scissors",     "teddy bear",
    "hair drier",    "toothbrush"};

}  // namespace

std::vector<Detection> detect_objects(const RGBImage& img) {
  std::vector<Detection> out;
  if (img.width <= 0 || img.height <= 0) return out;

  auto input = resize_rgb_chw(img, kInput, kInput);
  std::array<int64_t, 4> shape{1, 3, kInput, kInput};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value tensor = Ort::Value::CreateTensor<float>(mem, input.data(), input.size(), shape.data(), shape.size());

  const char* in_name = "images";
  const char* out_name = "output0";
  auto outputs = onnx::run(onnx::yolo(), {in_name}, {std::move(tensor)}, {out_name});
  float* data = outputs[0].GetTensorMutableData<float>();
  auto dims = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
  if (dims.size() != 3 || dims[2] < 6) return out;

  int64_t n = dims[1];
  for (int64_t i = 0; i < n; ++i) {
    float x1 = data[i * 6 + 0];
    float y1 = data[i * 6 + 1];
    float x2 = data[i * 6 + 2];
    float y2 = data[i * 6 + 3];
    float conf = data[i * 6 + 4];
    int cls = static_cast<int>(data[i * 6 + 5]);
    if (conf < 0.5f) continue;

    Detection d;
    d.confidence = conf;
    d.label = cls >= 0 && cls < static_cast<int>(kCoco.size()) ? kCoco[static_cast<size_t>(cls)] : "object";
    d.bounds = {x1 / kInput, y1 / kInput, (x2 - x1) / kInput, (y2 - y1) / kInput};
    out.push_back(d);
  }
  return out;
}

}  // namespace eyes
