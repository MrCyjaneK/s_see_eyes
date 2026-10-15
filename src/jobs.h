#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace eyes::jobs {

using JobId = int64_t;

JobId new_job_id();
void start_analyze(JobId id, std::string input, std::string output, std::string data_dir);
int progress(JobId id, int scale);
std::string progress_string(JobId id);
void update_progress(JobId id, int p, const std::string& msg);
void finish_job(JobId id);
void fail_job(JobId id, const std::string& err);

}  // namespace eyes::jobs
