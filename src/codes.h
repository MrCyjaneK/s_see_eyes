#pragma once

#include <vector>

#include "image.h"
#include "pipeline.h"

namespace eyes {

// Detect all QR codes and barcodes in img. Each match gets its own bounds entry.
std::vector<CodeRegion> scan_codes(const RGBImage& img);

}  // namespace eyes
