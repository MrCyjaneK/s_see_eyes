#if defined(__APPLE__)

#include "image.h"

#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <vector>

namespace eyes {

RGBImage load_image_imageio(const std::string& path, std::string& err) {
  RGBImage out;
  err.clear();

  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.c_str()),
      path.size(), false);
  if (!url) {
    err = "invalid path";
    return out;
  }

  CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
  CFRelease(url);
  if (!source) {
    err = "cannot open image";
    return out;
  }

  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
  CFRelease(source);
  if (!image) {
    err = "cannot decode image";
    return out;
  }

  const size_t w = CGImageGetWidth(image);
  const size_t h = CGImageGetHeight(image);
  if (w == 0 || h == 0) {
    err = "empty image";
    CGImageRelease(image);
    return out;
  }

  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  if (!color_space) {
    err = "color space unavailable";
    CGImageRelease(image);
    return out;
  }

  std::vector<uint8_t> rgba(w * h * 4);
  CGContextRef ctx = CGBitmapContextCreate(
      rgba.data(), w, h, 8, w * 4, color_space,
      kCGImageAlphaNoneSkipLast | kCGBitmapByteOrderDefault);
  CGColorSpaceRelease(color_space);
  if (!ctx) {
    err = "bitmap context failed";
    CGImageRelease(image);
    return out;
  }

  CGContextTranslateCTM(ctx, 0, static_cast<CGFloat>(h));
  CGContextScaleCTM(ctx, 1, -1);
  CGContextDrawImage(ctx, CGRectMake(0, 0, static_cast<CGFloat>(w), static_cast<CGFloat>(h)), image);
  CGContextRelease(ctx);
  CGImageRelease(image);

  out.width = static_cast<int>(w);
  out.height = static_cast<int>(h);
  out.channels = 3;
  out.pixels.resize(w * h * 3);
  for (size_t i = 0; i < w * h; ++i) {
    out.pixels[i * 3 + 0] = rgba[i * 4 + 0];
    out.pixels[i * 3 + 1] = rgba[i * 4 + 1];
    out.pixels[i * 3 + 2] = rgba[i * 4 + 2];
  }
  return out;
}

}  // namespace eyes

#endif
