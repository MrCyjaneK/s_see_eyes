#include "face.h"

#include "onnx_session.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace eyes {

namespace {

struct Anchor {
  float cx;
  float cy;
};

std::vector<Anchor> scrfd_anchors(int stride) {
  int n = 640 / stride;
  std::vector<Anchor> out;
  out.reserve(static_cast<size_t>(n * n * 2));
  for (int y = 0; y < n; ++y) {
    for (int x = 0; x < n; ++x) {
      float cx = (x + 0.5f) * static_cast<float>(stride);
      float cy = (y + 0.5f) * static_cast<float>(stride);
      out.push_back({cx, cy});
      out.push_back({cx, cy});
    }
  }
  return out;
}

float iou(const FaceBox& a, const FaceBox& b) {
  float x1 = std::max(a.bounds.x, b.bounds.x);
  float y1 = std::max(a.bounds.y, b.bounds.y);
  float x2 = std::min(a.bounds.x + a.bounds.w, b.bounds.x + b.bounds.w);
  float y2 = std::min(a.bounds.y + a.bounds.h, b.bounds.y + b.bounds.h);
  float inter = std::max(0.f, x2 - x1) * std::max(0.f, y2 - y1);
  float uni = a.bounds.w * a.bounds.h + b.bounds.w * b.bounds.h - inter;
  return uni > 0 ? inter / uni : 0;
}

}  // namespace

std::vector<FaceBox> detect_faces(const RGBImage& img) {
  std::vector<FaceBox> faces;
  if (img.width <= 0 || img.height <= 0) return faces;

  const int k = 640;
  auto chw = resize_rgb_chw(img, k, k);
  for (float& v : chw) v = (v - 0.5f) / 0.5f;
  std::array<int64_t, 4> shape{1, 3, k, k};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value in = Ort::Value::CreateTensor<float>(mem, chw.data(), chw.size(), shape.data(), shape.size());

  auto& session = onnx::face_det();
  const char* in_name = "input.1";
  // scores, boxes, then 5-point landmarks, one pair of tensors per stride 8/16/32
  const char* out_names[] = {"443", "468", "493", "446", "471", "496", "449", "474", "499"};
  auto outs = onnx::run(session, {in_name}, {std::move(in)},
                        {out_names[0], out_names[1], out_names[2], out_names[3], out_names[4],
                         out_names[5], out_names[6], out_names[7], out_names[8]});

  const int strides[] = {8, 16, 32};
  for (int si = 0; si < 3; ++si) {
    float* scores = outs[static_cast<size_t>(si)].GetTensorMutableData<float>();
    float* boxes = outs[static_cast<size_t>(si + 3)].GetTensorMutableData<float>();
    float* kps = outs[static_cast<size_t>(si + 6)].GetTensorMutableData<float>();
    auto sd = outs[static_cast<size_t>(si)].GetTensorTypeAndShapeInfo().GetShape();
    int n = static_cast<int>(sd[0]);
    auto anchors = scrfd_anchors(strides[si]);
    if (static_cast<int>(anchors.size()) != n) continue;
    float stride = static_cast<float>(strides[si]);
    float scale = static_cast<float>(k);
    for (int i = 0; i < n; ++i) {
      float score = scores[i];
      if (score < 0.5f) continue;
      float x1 = anchors[static_cast<size_t>(i)].cx - boxes[i * 4 + 0] * stride;
      float y1 = anchors[static_cast<size_t>(i)].cy - boxes[i * 4 + 1] * stride;
      float x2 = anchors[static_cast<size_t>(i)].cx + boxes[i * 4 + 2] * stride;
      float y2 = anchors[static_cast<size_t>(i)].cy + boxes[i * 4 + 3] * stride;
      FaceBox f;
      f.confidence = score;
      f.bounds = {x1 / scale, y1 / scale, (x2 - x1) / scale, (y2 - y1) / scale};
      f.has_landmarks = true;
      float cx = anchors[static_cast<size_t>(i)].cx;
      float cy = anchors[static_cast<size_t>(i)].cy;
      for (int p = 0; p < 5; ++p) {
        f.landmark[p].x = (cx + kps[i * 10 + p * 2] * stride) / scale;
        f.landmark[p].y = (cy + kps[i * 10 + p * 2 + 1] * stride) / scale;
      }
      faces.push_back(f);
    }
  }

  std::sort(faces.begin(), faces.end(),
            [](const FaceBox& a, const FaceBox& b) { return a.confidence > b.confidence; });
  std::vector<FaceBox> kept;
  for (const auto& f : faces) {
    bool dup = false;
    for (const auto& k : kept) {
      if (iou(f, k) > 0.4f) {
        dup = true;
        break;
      }
    }
    if (!dup) kept.push_back(f);
    if (kept.size() >= 20) break;
  }
  return kept;
}

}  // namespace eyes
