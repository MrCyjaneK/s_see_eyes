#include "onnx_session.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>

#if defined(__APPLE__)
#include <dlfcn.h>

#include <filesystem>
#endif

namespace eyes::onnx {

namespace {

Ort::Env* g_env = nullptr;
std::unique_ptr<Ort::SessionOptions> g_opts;
std::string g_models;
#if defined(__APPLE__)
void* g_ort_lib = nullptr;
#endif

std::unique_ptr<Ort::Session> s_yolo;
std::unique_ptr<Ort::Session> s_ocr_det;
std::unique_ptr<Ort::Session> s_ocr_rec;
std::unique_ptr<Ort::Session> s_ocr_cls;
std::unique_ptr<Ort::Session> s_face;
std::unique_ptr<Ort::Session> s_face_rec;
std::unique_ptr<Ort::Session> s_tags;
std::unique_ptr<Ort::Session> s_cap_enc;
std::unique_ptr<Ort::Session> s_cap_dec;
bool caption_loaded = false;
std::mutex caption_mu;
std::mutex infer_mu;

std::vector<std::string> g_ocr_keys;
std::vector<std::string> g_imagenet;

std::unique_ptr<Ort::Session> open(const std::string& file) {
  return std::make_unique<Ort::Session>(*g_env, file.c_str(), *g_opts);
}

#if defined(__APPLE__)
void* open_ort() {
  std::vector<std::string> paths;
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(&init), &info) != 0 && info.dli_fname && info.dli_fname[0]) {
    const std::filesystem::path dir = std::filesystem::path(info.dli_fname).parent_path();
    std::error_code ec;
    for (const auto& ent : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) break;
      const std::string name = ent.path().filename().string();
      if (name.rfind("libonnxruntime", 0) == 0 && name.size() > 6 && name.find(".dylib") != std::string::npos)
        paths.push_back(ent.path().string());
    }
    paths.push_back((dir / "../onnxruntime.framework/onnxruntime").lexically_normal().string());
    paths.push_back((dir / "../../../onnxruntime.framework/onnxruntime").lexically_normal().string());
  }
  const char* fixed[] = {
      "@rpath/onnxruntime.framework/onnxruntime",
      "@loader_path/../onnxruntime.framework/onnxruntime",
      "@loader_path/../../../onnxruntime.framework/onnxruntime",
      "@loader_path/libonnxruntime.dylib",
      "libonnxruntime.dylib",
  };
  for (const char* p : fixed) paths.emplace_back(p);

  std::string tried;
  for (const std::string& path : paths) {
    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle) return handle;
    const char* err = dlerror();
    tried += "\n  " + path + ": " + (err ? err : "dlopen failed");
  }
  throw std::runtime_error("failed to open onnxruntime" + tried);
}

void ensure_runtime() {
  if (g_ort_lib) return;
  g_ort_lib = open_ort();
  auto get_base = reinterpret_cast<const OrtApiBase* (*)()>(dlsym(g_ort_lib, "OrtGetApiBase"));
  if (!get_base) throw std::runtime_error("onnxruntime is missing OrtGetApiBase");
  const OrtApi* api = get_base()->GetApi(ORT_API_VERSION);
  if (!api) throw std::runtime_error("onnxruntime API version mismatch");
  Ort::InitApi(api);
}
#endif

std::unique_ptr<Ort::Session> open_caption(const std::string& file) {
  Ort::SessionOptions opts;
  opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
  return std::make_unique<Ort::Session>(*g_env, file.c_str(), opts);
}

void ensure_caption_loaded() {
  std::lock_guard<std::mutex> lock(caption_mu);
  if (caption_loaded) return;
  s_cap_enc = open_caption(g_models + "/caption_encoder.onnx");
  s_cap_dec = open_caption(g_models + "/caption_decoder.onnx");
  caption_loaded = true;
}

std::vector<std::string> read_lines(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("missing file: " + path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(line);
  }
  return lines;
}

}  // namespace

void init(const std::string& models_dir) {
  if (g_env) return;
#if defined(__APPLE__)
  ensure_runtime();
#endif
  g_models = models_dir;
  g_env = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "eyes");
  g_opts = std::make_unique<Ort::SessionOptions>();
  g_opts->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

  s_yolo = open(g_models + "/yolo26n.onnx");
  s_ocr_det = open(g_models + "/ppocr_det.onnx");
  s_ocr_rec = open(g_models + "/ppocr_rec.onnx");
  s_ocr_cls = open(g_models + "/ppocr_cls.onnx");
  s_face = open(g_models + "/scrfd_det.onnx");
  s_face_rec = open(g_models + "/w600k_mbf.onnx");
  s_tags = open(g_models + "/mobilenet_tags.onnx");

  g_ocr_keys = read_lines(g_models + "/ppocr_keys.txt");
  g_ocr_keys.insert(g_ocr_keys.begin(), "");
  g_ocr_keys.push_back(" ");
  g_imagenet = read_lines(g_models + "/imagenet_labels.txt");
}

void shutdown() {
  // Wait for any in-flight Session::Run before freeing sessions / Env.
  std::lock_guard<std::mutex> infer(infer_mu);
  s_yolo.reset();
  s_ocr_det.reset();
  s_ocr_rec.reset();
  s_ocr_cls.reset();
  s_face.reset();
  s_face_rec.reset();
  s_tags.reset();
  {
    std::lock_guard<std::mutex> lock(caption_mu);
    s_cap_enc.reset();
    s_cap_dec.reset();
    caption_loaded = false;
  }
  g_ocr_keys.clear();
  g_imagenet.clear();
  g_models.clear();
  g_opts.reset();
  delete g_env;
  g_env = nullptr;
}

std::unique_lock<std::mutex> lock_inference() { return std::unique_lock<std::mutex>(infer_mu); }

Ort::Session& yolo() { return *s_yolo; }
Ort::Session& ocr_det() { return *s_ocr_det; }
Ort::Session& ocr_rec() { return *s_ocr_rec; }
Ort::Session& ocr_cls() { return *s_ocr_cls; }
Ort::Session& face_det() { return *s_face; }
Ort::Session& face_rec() { return *s_face_rec; }
Ort::Session& tags() { return *s_tags; }
Ort::Session& caption_encoder() {
  ensure_caption_loaded();
  return *s_cap_enc;
}
Ort::Session& caption_decoder() {
  ensure_caption_loaded();
  return *s_cap_dec;
}
const std::vector<std::string>& ocr_keys() { return g_ocr_keys; }
const std::vector<std::string>& imagenet_labels() { return g_imagenet; }
const std::string& models_dir() { return g_models; }

std::vector<Ort::Value> run(Ort::Session& session,
                              const std::vector<const char*>& input_names,
                              std::vector<Ort::Value> inputs,
                              const std::vector<const char*>& output_names) {
  return session.Run(Ort::RunOptions{nullptr}, input_names.data(), inputs.data(), inputs.size(),
                     output_names.data(), output_names.size());
}

}  // namespace eyes::onnx
