#pragma once

#include <vector>

#include "image.h"
#include "pipeline.h"

namespace eyes {

std::vector<FaceBox> detect_faces(const RGBImage& img);

// Fills id and person_id from the synced gallery in data_dir.
// An empty data_dir leaves faces as detections only.
// Returns an error string, or empty on success.
std::string identify_faces(const RGBImage& img, std::vector<FaceBox>& faces, const std::string& data_dir);

}  // namespace eyes
