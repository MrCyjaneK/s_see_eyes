#include "pipeline.h"

#include <stdexcept>

#include "caption.h"
#include "codes.h"
#include "exif.h"
#include "face.h"
#include "features.h"
#include "ocr.h"
#include "onnx_session.h"
#include "tags.h"
#include "yolo.h"

namespace eyes {

AnalysisResult analyze_image(const RGBImage& img, const std::string& path, const std::string& data_dir,
                             const std::function<void(int, const std::string&)>& progress) {
  // Hold for the whole pipeline so unload cannot free Ort sessions between models.
  auto infer_lock = onnx::lock_inference();

  AnalysisResult r;
  r.image = {{"width", img.width},
             {"height", img.height},
             {"format", image_format_from_path(path)}};
  progress(5, "reading exif");
  r.exif = read_exif(path, img);
  progress(15, "computing features");
  r.extras = compute_extras(img);
  progress(25, "detecting objects");
  r.objects = detect_objects(img);
  progress(45, "running ocr");
  r.ocr = run_ocr(img);
  progress(52, "scanning codes");
  r.codes = scan_codes(img);
  progress(60, "detecting faces");
  r.faces = detect_faces(img);
  progress(68, "matching faces");
  if (std::string err = identify_faces(img, r.faces, data_dir); !err.empty()) {
    throw std::runtime_error(err);
  }
  progress(75, "classifying tags");
  r.tags = classify_tags(img);
  progress(85, "generating caption");
  r.caption = generate_caption(img);
  progress(95, "finalizing");
  return r;
}

}  // namespace eyes
