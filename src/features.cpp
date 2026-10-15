#include "features.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <tuple>
#include <vector>

namespace eyes {

namespace {

std::string phash_hex(const RGBImage& img) {
  const int size = 8;
  std::vector<float> gray(static_cast<size_t>(size * size));
  for (int y = 0; y < size; ++y) {
    int sy = y * img.height / size;
    if (sy >= img.height) sy = img.height - 1;
    for (int x = 0; x < size; ++x) {
      int sx = x * img.width / size;
      if (sx >= img.width) sx = img.width - 1;
      const size_t off = static_cast<size_t>((sy * img.width + sx) * 3);
      gray[static_cast<size_t>(y * size + x)] =
          0.299f * img.pixels[off] + 0.587f * img.pixels[off + 1] + 0.114f * img.pixels[off + 2];
    }
  }
  float avg = 0;
  for (float v : gray) avg += v;
  avg /= gray.size();
  uint64_t h = 0;
  for (size_t i = 0; i < gray.size(); ++i) {
    if (gray[i] > avg) h |= (1ULL << i);
  }
  char buf[17];
  snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
  return buf;
}

double blur_score(const RGBImage& img) {
  if (img.width < 3 || img.height < 3) return 0;
  double sum = 0;
  int n = 0;
  for (int y = 1; y + 1 < img.height; ++y) {
    for (int x = 1; x + 1 < img.width; ++x) {
      auto lum = [&](int px, int py) {
        size_t off = static_cast<size_t>((py * img.width + px) * 3);
        return 0.299 * img.pixels[off] + 0.587 * img.pixels[off + 1] + 0.114 * img.pixels[off + 2];
      };
      double c = lum(x, y);
      double lap = -4 * c + lum(x - 1, y) + lum(x + 1, y) + lum(x, y - 1) + lum(x, y + 1);
      sum += lap * lap;
      ++n;
    }
  }
  return n ? sum / n / 10000.0 : 0;
}

nlohmann::json dominant_colors(const RGBImage& img, int buckets = 8) {
  std::map<std::tuple<int, int, int>, int> hist;
  const int step = std::max(1, (img.width * img.height) / 20000);
  for (int i = 0; i < img.width * img.height; i += step) {
    const size_t off = static_cast<size_t>(i) * 3;
    int r = (img.pixels[off] / 32) * 32;
    int g = (img.pixels[off + 1] / 32) * 32;
    int b = (img.pixels[off + 2] / 32) * 32;
    hist[{r, g, b}]++;
  }
  std::vector<std::pair<int, std::tuple<int, int, int>>> ranked;
  for (const auto& kv : hist) ranked.push_back({kv.second, kv.first});
  std::sort(ranked.begin(), ranked.end(), std::greater<>());
  nlohmann::json arr = nlohmann::json::array();
  for (int i = 0; i < std::min(buckets, static_cast<int>(ranked.size())); ++i) {
    auto [r, g, b] = ranked[static_cast<size_t>(i)].second;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02x%02x%02x", r, g, b);
    arr.push_back({{"hex", hex}, {"weight", ranked[static_cast<size_t>(i)].first}});
  }
  return arr;
}

}  // namespace

nlohmann::json compute_extras(const RGBImage& img) {
  return {{"phash", phash_hex(img)},
          {"blurScore", blur_score(img)},
          {"dominantColors", dominant_colors(img)}};
}

}  // namespace eyes
