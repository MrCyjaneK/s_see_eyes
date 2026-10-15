#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>

namespace eyes {

RGBImage load_image(const std::string& path, std::string& err) {
  RGBImage out;
  err.clear();
  int w = 0, h = 0, c = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, 3);
  if (data) {
    out.width = w;
    out.height = h;
    out.channels = 3;
    out.pixels.assign(data, data + w * h * 3);
    stbi_image_free(data);
    return out;
  }

  const char* stb_err = stbi_failure_reason();
  std::string stb_msg = stb_err ? stb_err : "unknown error";

#if defined(__APPLE__)
  out = load_image_imageio(path, err);
  if (out.width > 0) return out;
  err = std::string("decode failed: ") + stb_msg + "; ImageIO: " + err;
#else
  err = std::string("decode failed: ") + stb_msg;
#endif
  return out;
}

std::vector<float> resize_rgb_chw(const RGBImage& img, int out_w, int out_h) {
  std::vector<float> out(static_cast<size_t>(3 * out_w * out_h));
  if (img.width <= 0 || img.height <= 0) return out;
  for (int oy = 0; oy < out_h; ++oy) {
    int sy = static_cast<int>(static_cast<float>(oy) * img.height / out_h);
    if (sy >= img.height) sy = img.height - 1;
    for (int ox = 0; ox < out_w; ++ox) {
      int sx = static_cast<int>(static_cast<float>(ox) * img.width / out_w);
      if (sx >= img.width) sx = img.width - 1;
      const size_t src = static_cast<size_t>((sy * img.width + sx) * 3);
      for (int ch = 0; ch < 3; ++ch) {
        out[static_cast<size_t>(ch * out_w * out_h + oy * out_w + ox)] =
            img.pixels[src + ch] / 255.0f;
      }
    }
  }
  return out;
}

}  // namespace eyes
