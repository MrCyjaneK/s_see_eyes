#include "face.h"

#include "onnx_session.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace eyes {

namespace {

constexpr int kEmbed = 512;
constexpr int kAlign = 112;
constexpr float kMatch = 0.45f;
constexpr int kBytes = kEmbed * static_cast<int>(sizeof(float));

// ArcFace 112 template: left eye, right eye, nose, left mouth, right mouth.
constexpr float kDst[5][2] = {
    {38.2946f, 51.6963f}, {73.5318f, 51.5014f}, {56.0252f, 71.7366f},
    {41.5493f, 92.3655f}, {70.7299f, 92.2041f},
};

#if !defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "face gallery embeddings are stored little-endian"
#endif
static_assert(sizeof(float) == 4);

struct Entry {
  std::string id;
  std::string person_id;
  std::array<float, kEmbed> emb{};
};

struct Cache {
  std::string dir;
  bool have_mtime = false;
  std::filesystem::file_time_type dir_mtime{};
  std::vector<Entry> entries;
};

std::mutex gallery_mu;
Cache gallery;

uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  uint64_t bitlen = static_cast<uint64_t>(len) * 8;
  size_t pad = (len + 9 + 63) / 64 * 64;
  std::vector<uint8_t> msg(pad, 0);
  std::memcpy(msg.data(), data, len);
  msg[len] = 0x80;
  for (int i = 0; i < 8; ++i) msg[pad - 1 - i] = static_cast<uint8_t>(bitlen >> (8 * i));
  for (size_t off = 0; off < pad; off += 64) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      size_t p = off + static_cast<size_t>(i) * 4;
      w[i] = (uint32_t(msg[p]) << 24) | (uint32_t(msg[p + 1]) << 16) | (uint32_t(msg[p + 2]) << 8) |
             uint32_t(msg[p + 3]);
    }
    for (int i = 16; i < 64; ++i) {
      uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = hh + S1 + ch + K[i] + w[i];
      uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + maj;
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }
  for (int i = 0; i < 8; ++i) {
    out[i * 4] = static_cast<uint8_t>(h[i] >> 24);
    out[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
    out[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
    out[i * 4 + 3] = static_cast<uint8_t>(h[i]);
  }
}

std::string sha256_hex(const float* emb) {
  uint8_t dig[32];
  sha256(reinterpret_cast<const uint8_t*>(emb), kBytes, dig);
  static const char* hexd = "0123456789abcdef";
  std::string out(64, '0');
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = hexd[dig[i] >> 4];
    out[i * 2 + 1] = hexd[dig[i] & 0xf];
  }
  return out;
}

bool solve4(double m[4][5]) {
  for (int col = 0; col < 4; ++col) {
    int piv = col;
    for (int r = col + 1; r < 4; ++r) {
      if (std::fabs(m[r][col]) > std::fabs(m[piv][col])) piv = r;
    }
    if (std::fabs(m[piv][col]) < 1e-12) return false;
    if (piv != col) {
      for (int c = col; c < 5; ++c) std::swap(m[col][c], m[piv][c]);
    }
    double div = m[col][col];
    for (int c = col; c < 5; ++c) m[col][c] /= div;
    for (int r = 0; r < 4; ++r) {
      if (r == col) continue;
      double f = m[r][col];
      for (int c = col; c < 5; ++c) m[r][c] -= f * m[col][c];
    }
  }
  return true;
}

uint8_t sample_px(const RGBImage& img, int x, int y, int ch) {
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= img.width) x = img.width - 1;
  if (y >= img.height) y = img.height - 1;
  return img.pixels[static_cast<size_t>((y * img.width + x) * 3 + ch)];
}

bool embed_face(const RGBImage& img, const FaceBox& face, std::array<float, kEmbed>& out) {
  if (!face.has_landmarks || img.width <= 1 || img.height <= 1) return false;
  double ata[4][5] = {};
  for (int p = 0; p < 5; ++p) {
    double x = face.landmark[p].x * img.width;
    double y = face.landmark[p].y * img.height;
    double xp = kDst[p][0];
    double yp = kDst[p][1];
    double rows[2][5] = {{x, -y, 1, 0, xp}, {y, x, 0, 1, yp}};
    for (auto& row : rows) {
      for (int c = 0; c < 4; ++c) {
        ata[c][4] += row[c] * row[4];
        for (int d = 0; d < 4; ++d) ata[c][d] += row[c] * row[d];
      }
    }
  }
  if (!solve4(ata)) return false;
  double a = ata[0][4], b = ata[1][4], tx = ata[2][4], ty = ata[3][4];
  double det = a * a + b * b;
  if (det < 1e-8) return false;

  std::vector<float> chw(static_cast<size_t>(3 * kAlign * kAlign));
  for (int oy = 0; oy < kAlign; ++oy) {
    for (int ox = 0; ox < kAlign; ++ox) {
      double xd = ox - tx;
      double yd = oy - ty;
      double sx = (a * xd + b * yd) / det;
      double sy = (-b * xd + a * yd) / det;
      int x0 = static_cast<int>(std::floor(sx));
      int y0 = static_cast<int>(std::floor(sy));
      float wx = static_cast<float>(sx - x0);
      float wy = static_cast<float>(sy - y0);
      for (int ch = 0; ch < 3; ++ch) {
        float v = sample_px(img, x0, y0, ch) * (1 - wx) * (1 - wy) +
                  sample_px(img, x0 + 1, y0, ch) * wx * (1 - wy) +
                  sample_px(img, x0, y0 + 1, ch) * (1 - wx) * wy +
                  sample_px(img, x0 + 1, y0 + 1, ch) * wx * wy;
        chw[static_cast<size_t>(ch * kAlign * kAlign + oy * kAlign + ox)] = (v - 127.5f) / 127.5f;
      }
    }
  }

  std::array<int64_t, 4> shape{1, 3, kAlign, kAlign};
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  Ort::Value in =
      Ort::Value::CreateTensor<float>(mem, chw.data(), chw.size(), shape.data(), shape.size());
  const char* in_name = "input.1";
  const char* out_name = "516";
  auto outs = onnx::run(onnx::face_rec(), {in_name}, std::move(in), {out_name});
  if (outs.empty()) return false;
  auto info = outs[0].GetTensorTypeAndShapeInfo();
  if (info.GetElementCount() != static_cast<size_t>(kEmbed)) return false;
  float* emb = outs[0].GetTensorMutableData<float>();
  float norm = 0;
  for (int i = 0; i < kEmbed; ++i) norm += emb[i] * emb[i];
  norm = std::sqrt(norm);
  if (norm < 1e-6f) return false;
  for (int i = 0; i < kEmbed; ++i) out[static_cast<size_t>(i)] = emb[i] / norm;
  return true;
}

namespace fs = std::filesystem;

bool face_file(const fs::path& p) {
  auto name = p.filename().string();
  if (name.starts_with(".space-sync-tmp-")) return false;
  return name.size() == 68 && name.ends_with(".bin");
}

bool directory_mtime(const fs::path& dir, fs::file_time_type& out) {
  std::error_code ec;
  auto t = fs::last_write_time(dir, ec);
  if (ec) return false;
  out = t;
  return true;
}

void remember_mtime(const fs::path& dir) {
  gallery.dir = dir.string();
  gallery.have_mtime = directory_mtime(dir, gallery.dir_mtime);
}

float cosine(const Entry& a, const Entry& b) {
  float s = 0;
  for (int k = 0; k < kEmbed; ++k) s += a.emb[static_cast<size_t>(k)] * b.emb[static_cast<size_t>(k)];
  return s;
}

// Nearest stored face at or above the match threshold, or null when this face starts a person.
const Entry* nearest_person(const Entry& face) {
  const Entry* best = nullptr;
  float best_s = kMatch;
  for (const auto& e : gallery.entries) {
    if (e.id == face.id) continue;
    float s = cosine(face, e);
    if (s >= best_s) {
      best_s = s;
      best = &e;
    }
  }
  return best;
}

bool load_entry(const fs::path& p, Entry& e) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  in.read(reinterpret_cast<char*>(e.emb.data()), kBytes);
  if (in.gcount() != kBytes) return false;
  e.id = p.stem().string();
  if (e.id.size() != 64) return false;
  std::ifstream person(p.parent_path() / (e.id + ".person"));
  std::string line;
  if (person && std::getline(person, line) && line.size() == 64) e.person_id = line;
  else e.person_id = e.id;
  return true;
}

void load_gallery(const fs::path& dir) {
  gallery.entries.clear();
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) {
    remember_mtime(dir);
    return;
  }
  for (const auto& ent : fs::directory_iterator(dir, ec)) {
    if (ec || !ent.is_regular_file() || !face_file(ent.path())) continue;
    Entry e;
    if (!load_entry(ent.path(), e)) continue;
    gallery.entries.push_back(std::move(e));
  }
  remember_mtime(dir);
}

void ensure_gallery(const fs::path& dir) {
  fs::file_time_type mtime{};
  bool have = directory_mtime(dir, mtime);
  if (gallery.dir == dir.string() && gallery.have_mtime == have && (!have || gallery.dir_mtime == mtime)) {
    return;
  }
  load_gallery(dir);
}

const Entry* find_id(const std::string& id) {
  for (const auto& e : gallery.entries) {
    if (e.id == id) return &e;
  }
  return nullptr;
}

std::string store_person(const fs::path& dir, const Entry& e) {
  fs::path dest = dir / (e.id + ".person");
  std::error_code ec;
  if (fs::is_regular_file(dest, ec)) return "";
  fs::path tmp = dir / (".space-sync-tmp-" + e.id + ".person");
  {
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return "failed to write face gallery";
    out << e.person_id << "\n";
    if (!out) return "failed to write face gallery";
  }
  fs::rename(tmp, dest, ec);
  if (ec) {
    fs::remove(tmp, ec);
    if (!fs::is_regular_file(dest)) return "failed to write face gallery";
  }
  return "";
}

std::string store_face(const fs::path& dir, const Entry& e) {
  fs::path dest = dir / (e.id + ".bin");
  std::error_code ec;
  if (fs::is_regular_file(dest, ec)) return "";
  fs::path tmp = dir / (".space-sync-tmp-" + e.id);
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return "failed to write face gallery";
    out.write(reinterpret_cast<const char*>(e.emb.data()), kBytes);
    if (!out) return "failed to write face gallery";
  }
  fs::rename(tmp, dest, ec);
  if (ec) {
    fs::remove(tmp, ec);
    if (!fs::is_regular_file(dest)) return "failed to write face gallery";
  }
  return "";
}

}  // namespace

std::string identify_faces(const RGBImage& img, std::vector<FaceBox>& faces, const std::string& data_dir) {
  if (data_dir.empty()) return "";
  bool any = false;
  for (const auto& f : faces) {
    if (f.has_landmarks) any = true;
  }
  if (!any) return "";

  struct Fresh {
    size_t index;
    Entry entry;
  };
  std::vector<Fresh> fresh;
  fresh.reserve(faces.size());
  for (size_t i = 0; i < faces.size(); ++i) {
    if (!faces[i].has_landmarks) continue;
    Entry e;
    if (!embed_face(img, faces[i], e.emb)) continue;
    e.id = sha256_hex(e.emb.data());
    faces[i].id = e.id;
    fresh.push_back(Fresh{i, std::move(e)});
  }
  if (fresh.empty()) return "";

  std::lock_guard<std::mutex> lock(gallery_mu);
  fs::path dir = fs::path(data_dir) / "faces";
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) return "failed to create face gallery";
  ensure_gallery(dir);

  bool wrote = false;
  for (auto& f : fresh) {
    if (const Entry* existing = find_id(f.entry.id)) {
      faces[f.index].person_id = existing->person_id;
      continue;
    }
    if (const Entry* match = nearest_person(f.entry)) f.entry.person_id = match->person_id;
    else f.entry.person_id = f.entry.id;
    if (auto err = store_person(dir, f.entry); !err.empty()) return err;
    if (auto err = store_face(dir, f.entry); !err.empty()) return err;
    faces[f.index].person_id = f.entry.person_id;
    gallery.entries.push_back(std::move(f.entry));
    wrote = true;
  }
  if (wrote) remember_mtime(dir);
  return "";
}

}  // namespace eyes
