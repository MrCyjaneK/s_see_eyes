#pragma once

#include <string>

#include "pipeline.h"
#include <nlohmann/json.hpp>

namespace eyes {

std::string to_json(const AnalysisResult& r);

}  // namespace eyes
