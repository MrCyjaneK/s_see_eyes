#include "ocr.h"

#include "onnx_session.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace eyes {

namespace {

std::vector<uint8_t> crop_rgb(const RGBImage& img, int x0, int y0, int x1, int y1) {
  x0 = std::max(0, x0);
  y0 = std::max(0, y0);
  x1 = std::min(img.width, x1);
  y1 = std::min(img.height, y1);
  if (x1 <= x0 || y1 <= y0) return {};
  int w = x1 - x0, h = y1 - y0;
  std::vector<uint8_t> out(static_cast<size_t>(w * h * 3));
  for (int y = 0; y < h; ++y) {
    size_t src = static_cast<size_t>(((y0 + y) * img.width + x0) * 3);
    size_t dst = static_cast<size_t>(y * w * 3);
    memcpy(out.data() + dst, img.pixels.data() + src, static_cast<size_t>(w * 3));
  }
  return out;
}

std::string ctc_decode(const float* data, size_t len, int vocab, const std::vector<std::string>& keys) {
  std::string out;
  int prev = -1;
  for (size_t t = 0; t < len; ++t) {
    int best = 0;
    float best_v = data[t * vocab];
    for (int i = 1; i < vocab; ++i) {
      float v = data[t * vocab + i];
      if (v > best_v) {
        best_v = v;
        best = i;
      }
    }
    if (best != 0 && best != prev && best < static_cast<int>(keys.size())) {
      out += keys[static_cast<size_t>(best)];
    }
    prev = best;
  }
  return out;
}

std::vector<float> prep_rec_tensor(const RGBImage& patch) {
  const int img_h = 48;
  const int img_w = 320;
  float wh_ratio = patch.width / float(patch.height);
  float max_wh_ratio = std::max(img_w / float(img_h), wh_ratio);
  int padded_w = static_cast<int>(img_h * max_wh_ratio);
  int resized_w = static_cast<int>(std::ceil(img_h * wh_ratio));
  if (resized_w > padded_w) resized_w = padded_w;

  auto chw = resize_rgb_chw(patch, resized_w, img_h);
  std::vector<float> out(static_cast<size_t>(3 * img_h * padded_w), 0.f);
  for (int c = 0; c < 3; ++c) {
    int src_c = 2 - c;  // RGB -> BGR
    for (int y = 0; y < img_h; ++y) {
      for (int x = 0; x < resized_w; ++x) {
        float v = chw[static_cast<size_t>(src_c * img_h * resized_w + y * resized_w + x)];
        v = (v - 0.5f) / 0.5f;
        out[static_cast<size_t>(c * img_h * padded_w + y * padded_w + x)] = v;
      }
    }
  }
  return out;
}

}  // namespace

std::vector<TextRegion> run_ocr(const RGBImage& img) {
  std::vector<TextRegion> regions;
  if (img.width <= 0 || img.height <= 0) return regions;

  const int max_side = 960;
  float scale = 1.f;
  int rw = img.width, rh = img.height;
  if (std::max(img.width, img.height) > max_side) {
    scale = static_cast<float>(max_side) / std::max(img.width, img.height);
    rw = static_cast<int>(img.width * scale);
    rh = static_cast<int>(img.height * scale);
  }
  rw = (rw + 31) / 32 * 32;
  rh = (rh + 31) / 32 * 32;
  auto chw = resize_rgb_chw(img, rw, rh);
  std::array<int64_t, 4> shape{1, 3, rh, rw};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value in = Ort::Value::CreateTensor<float>(mem, chw.data(), chw.size(), shape.data(), shape.size());
  const char* in_name = "x";
  const char* out_name = "fetch_name_0";
  auto outs = onnx::run(onnx::ocr_det(), {in_name}, {std::move(in)}, {out_name});
  float* map = outs[0].GetTensorMutableData<float>();
  auto dims = outs[0].GetTensorTypeAndShapeInfo().GetShape();
  if (dims.size() != 4) return regions;
  int mh = static_cast<int>(dims[2]);
  int mw = static_cast<int>(dims[3]);
  float thr = 0.3f;
  int min_x = mw, min_y = mh, max_x = 0, max_y = 0;
  bool any = false;
  for (int y = 0; y < mh; ++y) {
    for (int x = 0; x < mw; ++x) {
      if (map[y * mw + x] < thr) continue;
      any = true;
      min_x = std::min(min_x, x);
      min_y = std::min(min_y, y);
      max_x = std::max(max_x, x);
      max_y = std::max(max_y, y);
    }
  }
  std::vector<std::array<int, 4>> boxes;
  if (any && max_x > min_x && max_y > min_y) {
    int px0 = static_cast<int>(min_x / float(mw) * img.width);
    int py0 = static_cast<int>(min_y / float(mh) * img.height);
    int px1 = static_cast<int>((max_x + 1) / float(mw) * img.width);
    int py1 = static_cast<int>((max_y + 1) / float(mh) * img.height);
    int mx = std::max(4, (px1 - px0) / 8);
    int my = std::max(4, (py1 - py0) / 2);
    px0 = std::max(0, px0 - mx);
    py0 = std::max(0, py0 - my);
    px1 = std::min(img.width, px1 + mx);
    py1 = std::min(img.height, py1 + my);
    boxes.push_back({px0, py0, px1, py1});
  }

  const auto& keys = onnx::ocr_keys();
  for (const auto& b : boxes) {
    auto crop = crop_rgb(img, b[0], b[1], b[2], b[3]);
    if (crop.empty()) continue;
    int cw = b[2] - b[0], ch = b[3] - b[1];
    RGBImage patch{cw, ch, 3, crop};
    auto rec_in = prep_rec_tensor(patch);
    const int img_h = 48;
    const int img_w = 320;
    float wh_ratio = patch.width / float(patch.height);
    int padded_w = static_cast<int>(img_h * std::max(img_w / float(img_h), wh_ratio));
    std::array<int64_t, 4> rshape{1, 3, img_h, padded_w};
    Ort::Value rt = Ort::Value::CreateTensor<float>(mem, rec_in.data(), rec_in.size(), rshape.data(), rshape.size());
    const char* rin = "x";
    const char* rout = "fetch_name_0";
    auto ro = onnx::run(onnx::ocr_rec(), {rin}, {std::move(rt)}, {rout});
    float* logits = ro[0].GetTensorMutableData<float>();
    auto rd = ro[0].GetTensorTypeAndShapeInfo().GetShape();
    if (rd.size() != 3) continue;
    int tlen = static_cast<int>(rd[1]);
    int vocab = static_cast<int>(rd[2]);
    std::string text = ctc_decode(logits, static_cast<size_t>(tlen), vocab, keys);
    if (text.empty()) continue;
    TextRegion tr;
    tr.text = text;
    tr.confidence = 0.8f;
    tr.bounds = {b[0] / float(img.width), b[1] / float(img.height), (b[2] - b[0]) / float(img.width),
                 (b[3] - b[1]) / float(img.height)};
    regions.push_back(tr);
  }
  return regions;
}

}  // namespace eyes
