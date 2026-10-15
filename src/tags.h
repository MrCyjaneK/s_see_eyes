#pragma once

#include <utility>
#include <vector>

#include "image.h"

namespace eyes {

std::vector<std::pair<std::string, float>> classify_tags(const RGBImage& img);

}  // namespace eyes
