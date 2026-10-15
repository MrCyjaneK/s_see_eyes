#include "assets.h"
#include "ctx.h"
#include "image.h"
#include "jobs.h"
#include "output_json.h"
#include "pipeline.h"

#include "eyes.h"

#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>

namespace {

std::mutex load_mu;
std::string load_error;

char* dup_cstr(const std::string& s) { return strdup(s.c_str()); }

}  // namespace

extern "C" int SPACESEE_eyes_load(const char* workDir, const char* assetsDir) {
  if (!workDir || !workDir[0]) {
    load_error = "work dir is empty";
    return 1;
  }
  if (!assetsDir || !assetsDir[0]) {
    load_error = "assets dir is empty";
    return 1;
  }
  std::string err;
  if (!eyes::ensure_loaded(workDir, assetsDir, err)) {
    load_error = err;
    return 1;
  }
  load_error.clear();
  return 0;
}

extern "C" const char* SPACESEE_eyes_loadError(void) {
  if (load_error.empty()) return nullptr;
  return dup_cstr(load_error);
}

extern "C" void SPACESEE_eyes_unload(void) { eyes::unload(); }

extern "C" int SPACESEE_eyes_isHeavy(void) { return 1; }

extern "C" int SPACESEE_eyes_assetCount(void) { return eyes::assets::count(); }

extern "C" const char* SPACESEE_eyes_asset(int index) {
  std::string s = eyes::assets::describe(index);
  if (s.empty()) return nullptr;
  return dup_cstr(s);
}

extern "C" const char* SPACESEE_eyes_version(void) { return dup_cstr("1.0.0"); }

extern "C" const char* SPACESEE_eyes_extension(void) { return dup_cstr("json"); }

extern "C" const char* SPACESEE_eyes_filetypes(void) {
#if defined(__APPLE__)
  return dup_cstr(".jpg,.jpeg,.png,.webp,.gif,.bmp,.tiff,.tif,.heic,.heif");
#else
  return dup_cstr(".jpg,.jpeg,.png,.webp,.gif,.bmp,.tiff,.tif");
#endif
}

extern "C" long long SPACESEE_eyes_maxInputSize(void) {
  return 16LL * 1024 * 1024 * 1024;
}

namespace eyes::jobs {
void run_analyze(JobId id, std::string input, std::string output, std::string data_dir) {
  try {
    update_progress(id, 5, "opening image");
    std::string err;
    auto img = load_image(input, err);
    if (img.width <= 0) {
      fail_job(id, err.empty() ? "failed to decode image" : err);
      return;
    }
  auto result = analyze_image(img, input, data_dir, [&](int p, const std::string& msg) { update_progress(id, p, msg); });
    update_progress(id, 98, "writing output");
    std::string json = to_json(result);
    std::ofstream out(output);
    if (!out) {
      fail_job(id, "failed to write output");
      return;
    }
    out << json;
    finish_job(id);
  } catch (const std::exception& ex) {
    fail_job(id, ex.what());
  } catch (...) {
    // ZXing::Error does not derive from std::exception. An uncaught exception
    // on this detached thread calls std::terminate.
    fail_job(id, "analysis failed");
  }
}
}  // namespace eyes::jobs

extern "C" long long SPACESEE_eyes_analyze(const char* inputPath, const char* outputPath, const char* dataDir) {
  auto id = eyes::jobs::new_job_id();
  eyes::jobs::start_analyze(id, inputPath ? inputPath : "", outputPath ? outputPath : "",
                            dataDir ? dataDir : "");
  return static_cast<long long>(id);
}

extern "C" int SPACESEE_eyes_progress(long long jobId, int scale) {
  return eyes::jobs::progress(jobId, scale);
}

extern "C" const char* SPACESEE_eyes_progressString(long long jobId) {
  return dup_cstr(eyes::jobs::progress_string(jobId));
}

extern "C" void SPACESEE_eyes_free(void* ptr) { free(ptr); }
