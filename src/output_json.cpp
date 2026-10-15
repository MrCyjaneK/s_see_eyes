#include "output_json.h"

namespace eyes {

static nlohmann::json bounds_json(const Bounds& b) {
  return {{"x", b.x}, {"y", b.y}, {"w", b.w}, {"h", b.h}};
}

std::string to_json(const AnalysisResult& r) {
  nlohmann::json j;
  j["image"] = r.image;
  j["exif"] = r.exif;
  j["objects"] = nlohmann::json::array();
  for (const auto& o : r.objects) {
    j["objects"].push_back(
        {{"label", o.label}, {"confidence", o.confidence}, {"bounds", bounds_json(o.bounds)}});
  }
  j["ocr"] = nlohmann::json::array();
  for (const auto& t : r.ocr) {
    j["ocr"].push_back(
        {{"text", t.text}, {"confidence", t.confidence}, {"bounds", bounds_json(t.bounds)}});
  }
  j["codes"] = nlohmann::json::array();
  for (const auto& c : r.codes) {
    j["codes"].push_back({{"text", c.text},
                          {"format", c.format},
                          {"bounds", bounds_json(c.bounds)},
                          {"inverted", c.inverted}});
  }
  j["faces"] = nlohmann::json::array();
  for (const auto& f : r.faces) {
    nlohmann::json face = {{"confidence", f.confidence}, {"bounds", bounds_json(f.bounds)}};
    if (!f.id.empty()) face["id"] = f.id;
    if (!f.person_id.empty()) face["personId"] = f.person_id;
    j["faces"].push_back(std::move(face));
  }
  j["description"] = {{"tags", nlohmann::json::array()}, {"caption", r.caption}};
  for (const auto& [label, score] : r.tags) {
    j["description"]["tags"].push_back({{"label", label}, {"score", score}});
  }
  j["extras"] = r.extras;
  return j.dump(2);
}

}  // namespace eyes
