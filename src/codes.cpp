#include "codes.h"

#include "BarcodeFormat.h"
#include "Error.h"
#include "ReadBarcode.h"

#include <algorithm>
#include <climits>

namespace eyes {

namespace {

Bounds bounds_from_position(const ZXing::Position& pos, int width, int height) {
  if (width <= 0 || height <= 0) return {};
  int min_x = INT_MAX, min_y = INT_MAX, max_x = 0, max_y = 0;
  for (const auto& p : pos) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }
  if (min_x == INT_MAX || max_x <= min_x || max_y <= min_y) return {};
  return {min_x / float(width), min_y / float(height), (max_x - min_x) / float(width),
          (max_y - min_y) / float(height)};
}

void append_codes(const ZXing::Barcodes& codes, int width, int height, std::vector<CodeRegion>& regions) {
  for (const auto& code : codes) {
    if (!code.isValid()) continue;
    std::string text = code.text();
    if (text.empty()) continue;
    CodeRegion region;
    region.text = std::move(text);
    region.format = ZXing::ToString(code.format());
    region.bounds = bounds_from_position(code.position(), width, height);
    region.inverted = code.isInverted();
    regions.push_back(std::move(region));
  }
}

// zxing-cpp 2.3 throws ZXing::Error (not std::exception) from DataBarLimitedReader
// when a false-positive pattern does not fit a GTIN. ReadBarcodes does not catch
// it, and an uncaught throw on the analysis thread aborts the process.
void read_codes(const ZXing::ImageView& view, ZXing::ReaderOptions options, int width, int height,
                std::vector<CodeRegion>& regions) {
  try {
    append_codes(ZXing::ReadBarcodes(view, options), width, height, regions);
  } catch (const ZXing::Error&) {
    // formats() returns a copy; clear DataBar Limited on that copy and scan again.
    auto formats = options.formats();
    if (!formats.testFlag(ZXing::BarcodeFormat::DataBarLimited)) return;
    formats.setFlag(ZXing::BarcodeFormat::DataBarLimited, false);
    options.setFormats(formats);
    try {
      append_codes(ZXing::ReadBarcodes(view, options), width, height, regions);
    } catch (const ZXing::Error&) {
    }
  }
}

}  // namespace

std::vector<CodeRegion> scan_codes(const RGBImage& img) {
  std::vector<CodeRegion> regions;
  if (img.width <= 0 || img.height <= 0 || img.pixels.empty()) return regions;

  ZXing::ImageView view(img.pixels.data(), img.width, img.height, ZXing::ImageFormat::RGB);
  // Scan normal and inverted (white-on-black) reflectance; ReadBarcodes returns every match.
  auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::Any).setTryInvert(true);
  read_codes(view, options, img.width, img.height, regions);
  return regions;
}

}  // namespace eyes
