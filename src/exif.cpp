#include "exif.h"

#include "easyexif.h"

#include <cctype>
#include <fstream>

namespace eyes {

std::string image_format_from_path(const std::string& path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return "unknown";
  std::string ext = path.substr(dot + 1);
  for (auto& c : ext) c = static_cast<char>(tolower(c));
  return ext;
}

nlohmann::json read_exif(const std::string& path, const RGBImage& img) {
  nlohmann::json j = nlohmann::json::object();
  std::ifstream in(path, std::ios::binary);
  if (!in) return j;
  std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  easyexif::EXIFInfo info;
  if (info.parseFrom(bytes) != 0) {
    j["parsed"] = false;
    return j;
  }
  j["parsed"] = true;
  j["ByteAlign"] = info.ByteAlign;
  j["Make"] = info.Make;
  j["Model"] = info.Model;
  j["Software"] = info.Software;
  j["ImageDescription"] = info.ImageDescription;
  j["Orientation"] = info.Orientation;
  j["BitsPerSample"] = info.BitsPerSample;
  j["DateTime"] = info.DateTime;
  j["DateTimeOriginal"] = info.DateTimeOriginal;
  j["DateTimeDigitized"] = info.DateTimeDigitized;
  j["SubSecTimeOriginal"] = info.SubSecTimeOriginal;
  j["Copyright"] = info.Copyright;
  j["ExposureTime"] = info.ExposureTime;
  j["FNumber"] = info.FNumber;
  j["ExposureProgram"] = info.ExposureProgram;
  j["ISOSpeedRatings"] = info.ISOSpeedRatings;
  j["ShutterSpeedValue"] = info.ShutterSpeedValue;
  j["ExposureBiasValue"] = info.ExposureBiasValue;
  j["SubjectDistance"] = info.SubjectDistance;
  j["FocalLength"] = info.FocalLength;
  j["FocalLengthIn35mm"] = info.FocalLengthIn35mm;
  j["Flash"] = static_cast<int>(info.Flash);
  j["FlashReturnedLight"] = info.FlashReturnedLight;
  j["FlashMode"] = info.FlashMode;
  j["MeteringMode"] = info.MeteringMode;
  j["ImageWidth"] = info.ImageWidth ? info.ImageWidth : static_cast<unsigned>(img.width);
  j["ImageHeight"] = info.ImageHeight ? info.ImageHeight : static_cast<unsigned>(img.height);
  j["GPS"] = {{"Latitude", info.GeoLocation.Latitude},
              {"Longitude", info.GeoLocation.Longitude},
              {"Altitude", info.GeoLocation.Altitude},
              {"AltitudeRef", static_cast<int>(info.GeoLocation.AltitudeRef)},
              {"DOP", info.GeoLocation.DOP},
              {"LatDegrees", info.GeoLocation.LatComponents.degrees},
              {"LatMinutes", info.GeoLocation.LatComponents.minutes},
              {"LatSeconds", info.GeoLocation.LatComponents.seconds},
              {"LatDirection", std::string(1, info.GeoLocation.LatComponents.direction)},
              {"LonDegrees", info.GeoLocation.LonComponents.degrees},
              {"LonMinutes", info.GeoLocation.LonComponents.minutes},
              {"LonSeconds", info.GeoLocation.LonComponents.seconds},
              {"LonDirection", std::string(1, info.GeoLocation.LonComponents.direction)}};
  j["Lens"] = {{"Make", info.LensInfo.Make},
               {"Model", info.LensInfo.Model},
               {"FStopMin", info.LensInfo.FStopMin},
               {"FStopMax", info.LensInfo.FStopMax},
               {"FocalLengthMin", info.LensInfo.FocalLengthMin},
               {"FocalLengthMax", info.LensInfo.FocalLengthMax},
               {"FocalPlaneXResolution", info.LensInfo.FocalPlaneXResolution},
               {"FocalPlaneYResolution", info.LensInfo.FocalPlaneYResolution},
               {"FocalPlaneResolutionUnit", info.LensInfo.FocalPlaneResolutionUnit}};
  return j;
}

}  // namespace eyes
