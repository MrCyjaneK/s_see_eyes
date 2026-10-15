#pragma once

#include <vector>

#include "image.h"
#include "pipeline.h"

namespace eyes {

std::vector<Detection> detect_objects(const RGBImage& img);

}  // namespace eyes
