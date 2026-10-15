#pragma once

#include <vector>

#include "image.h"
#include "pipeline.h"

namespace eyes {

std::vector<TextRegion> run_ocr(const RGBImage& img);

}  // namespace eyes
