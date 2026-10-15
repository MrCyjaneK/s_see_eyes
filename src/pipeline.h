#pragma once

#include <functional>
#include <string>
#include <vector>

#include "image.h"
#include <nlohmann/json.hpp>

namespace eyes {

struct Detection {
  std::string label;
  float confidence = 0;
  Bounds bounds;
};

struct TextRegion {
  std::string text;
  float confidence = 0;
  Bounds bounds;
};

struct FacePoint {
  float x = 0;
  float y = 0;
};

struct FaceBox {
  Bounds bounds;
  float confidence = 0;
  FacePoint landmark[5]{};
  bool has_landmarks = false;
  std::string id;
  std::string person_id;
};

struct CodeRegion {
  std::string text;
  std::string format;
  Bounds bounds;  // normalized rect for this code; one entry per QR/barcode in the image
  bool inverted = false;
};

struct AnalysisResult {
  nlohmann::json image;
  nlohmann::json exif;
  std::vector<Detection> objects;
  std::vector<TextRegion> ocr;
  std::vector<CodeRegion> codes;
  std::vector<FaceBox> faces;
  std::vector<std::pair<std::string, float>> tags;
  std::string caption;
  nlohmann::json extras;
};

AnalysisResult analyze_image(const RGBImage& img, const std::string& path, const std::string& data_dir,
                             const std::function<void(int, const std::string&)>& progress);

}  // namespace eyes
