#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "image.h"

// Apple dlopens ONNX Runtime. The C++ API must not call OrtGetApiBase at load.
#if defined(__APPLE__)
#define ORT_API_MANUAL_INIT
#endif
#include <onnxruntime_cxx_api.h>

namespace eyes::onnx {

void init(const std::string& models_dir);
void shutdown();

// Serialize Session::Run / session teardown against concurrent analyze jobs and
// host unload (same pattern as ears::whisper::lock_inference).
std::unique_lock<std::mutex> lock_inference();

Ort::Session& yolo();
Ort::Session& ocr_det();
Ort::Session& ocr_rec();
Ort::Session& ocr_cls();
Ort::Session& face_det();
Ort::Session& face_rec();
Ort::Session& tags();
Ort::Session& caption_encoder();
Ort::Session& caption_decoder();
const std::vector<std::string>& ocr_keys();
const std::vector<std::string>& imagenet_labels();

const std::string& models_dir();

std::vector<Ort::Value> run(Ort::Session& session,
                              const std::vector<const char*>& input_names,
                              std::vector<Ort::Value> inputs,
                              const std::vector<const char*>& output_names);

inline std::vector<Ort::Value> run(Ort::Session& session,
                                   const std::vector<const char*>& input_names,
                                   Ort::Value input,
                                   const std::vector<const char*>& output_names) {
  std::vector<Ort::Value> inputs;
  inputs.push_back(std::move(input));
  return run(session, input_names, std::move(inputs), output_names);
}

}  // namespace eyes::onnx
