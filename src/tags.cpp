#include "tags.h"

#include "onnx_session.h"

#include <algorithm>

namespace eyes {

std::vector<std::pair<std::string, float>> classify_tags(const RGBImage& img) {
  std::vector<std::pair<std::string, float>> tags;
  if (img.width <= 0 || img.height <= 0) return tags;

  const int k = 224;
  auto chw = resize_rgb_chw(img, k, k);
  for (size_t i = 0; i < chw.size(); i += 3) {
    chw[i] = (chw[i] - 0.485f) / 0.229f;
    chw[i + 1] = (chw[i + 1] - 0.456f) / 0.224f;
    chw[i + 2] = (chw[i + 2] - 0.406f) / 0.225f;
  }
  std::array<int64_t, 4> shape{1, 3, k, k};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value in = Ort::Value::CreateTensor<float>(mem, chw.data(), chw.size(), shape.data(), shape.size());
  const char* in_name = "input";
  const char* out_name = "output";
  auto outs = onnx::run(onnx::tags(), {in_name}, {std::move(in)}, {out_name});
  float* logits = outs[0].GetTensorMutableData<float>();
  int n = static_cast<int>(outs[0].GetTensorTypeAndShapeInfo().GetShape().back());
  const auto& labels = onnx::imagenet_labels();
  std::vector<std::pair<float, int>> ranked;
  for (int i = 0; i < n; ++i) ranked.push_back({logits[i], i});
  std::partial_sort(ranked.begin(), ranked.begin() + std::min(10, n), ranked.end(),
                    [](auto& a, auto& b) { return a.first > b.first; });
  for (int i = 0; i < std::min(10, n); ++i) {
    int idx = ranked[static_cast<size_t>(i)].second;
  float score = ranked[static_cast<size_t>(i)].first;
    std::string label = idx < static_cast<int>(labels.size()) ? labels[static_cast<size_t>(idx)] : "class";
    tags.push_back({label, score});
  }
  return tags;
}

}  // namespace eyes
