#pragma once

#include "image.h"
#include <nlohmann/json.hpp>

namespace eyes {

nlohmann::json compute_extras(const RGBImage& img);

}  // namespace eyes
