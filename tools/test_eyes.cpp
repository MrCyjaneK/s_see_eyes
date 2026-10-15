#include "eyes.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>

static int wait_job(int (*progress)(long long, int), const char* (*progress_str)(long long),
                    void (*free_fn)(void*), long long job) {
  for (int i = 0; i < 6000; ++i) {
    int p = progress(job, 100);
    if (p >= 100) {
      const char* msg = progress_str(job);
      int ok = msg && strcmp(msg, "done") == 0;
      if (msg) free_fn((void*)msg);
      return ok ? 0 : 1;
    }
    usleep(100000);
  }
  return 1;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "usage: %s <image> <output.json> [dataDir]\n", argv[0]);
    return 1;
  }
  const char* lib = getenv("EYES_LIB");
  if (!lib) lib = "./dist/libeyes.so";
  const char* assets = getenv("EYES_ASSETS_DIR");
  if (!assets) {
    fprintf(stderr, "EYES_ASSETS_DIR must point at the ONNX asset directory\n");
    return 1;
  }
  void* h = dlopen(lib, RTLD_NOW);
  if (!h) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  auto load = (int (*)(const char*, const char*))dlsym(h, "SPACESEE_eyes_load");
  auto analyze = (long long (*)(const char*, const char*, const char*))dlsym(h, "SPACESEE_eyes_analyze");
  auto progress = (int (*)(long long, int))dlsym(h, "SPACESEE_eyes_progress");
  auto progress_str = (const char* (*)(long long))dlsym(h, "SPACESEE_eyes_progressString");
  auto free_fn = (void (*)(void*))dlsym(h, "SPACESEE_eyes_free");
  if (!load || !analyze || !progress || !progress_str || !free_fn) {
    fprintf(stderr, "missing symbols\n");
    return 1;
  }
  if (load("/tmp/eyes-work", assets) != 0) {
    fprintf(stderr, "load failed\n");
    return 1;
  }
  const char* data_dir = argc >= 4 ? argv[3] : "";
  long long job = analyze(argv[1], argv[2], data_dir);
  if (wait_job(progress, progress_str, free_fn, job) != 0) {
    const char* msg = progress_str(job);
    fprintf(stderr, "analyze failed: %s\n", msg ? msg : "unknown");
    if (msg) free_fn((void*)msg);
    return 1;
  }
  std::ifstream in(argv[2]);
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  printf("%s\n", content.c_str());
  return 0;
}
