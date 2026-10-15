#pragma once

#include <string>

#include "image.h"
#include <nlohmann/json.hpp>

namespace eyes {

nlohmann::json read_exif(const std::string& path, const RGBImage& img);
std::string image_format_from_path(const std::string& path);

}  // namespace eyes
