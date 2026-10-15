#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eyes {

struct Bounds {
  float x = 0, y = 0, w = 0, h = 0;  // normalized 0-1
};

struct RGBImage {
  int width = 0;
  int height = 0;
  int channels = 0;
  std::vector<uint8_t> pixels;  // RGB row-major
};

RGBImage load_image(const std::string& path, std::string& err);

#if defined(__APPLE__)
RGBImage load_image_imageio(const std::string& path, std::string& err);
#endif

std::vector<float> resize_rgb_chw(const RGBImage& img, int out_w, int out_h);

}  // namespace eyes
