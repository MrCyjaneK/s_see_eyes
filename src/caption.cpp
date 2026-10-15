#include "caption.h"

#include "onnx_session.h"
#include "tags.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace eyes {

namespace {

std::string gpt2_decode_piece(const std::string& piece) {
  std::string out;
  for (size_t i = 0; i < piece.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(piece[i]);
    if (c < 128) {
      out.push_back(static_cast<char>(c));
    } else if (i + 1 < piece.size()) {
      unsigned char c2 = static_cast<unsigned char>(piece[i + 1]);
      int cp = ((c & 0x1f) << 6) | (c2 & 0x3f);
      out.push_back(static_cast<char>(cp));
      ++i;
    }
  }
  size_t p = 0;
  while ((p = out.find("Ġ", p)) != std::string::npos) {
    out.replace(p, 2, " ");
    ++p;
  }
  return out;
}

std::vector<std::string> load_vocab(const std::string& path) {
  std::ifstream in(path);
  nlohmann::json tok;
  in >> tok;
  const auto& vocab = tok["model"]["vocab"];
  size_t max_id = 0;
  for (auto it = vocab.begin(); it != vocab.end(); ++it) {
    max_id = std::max(max_id, it.value().get<size_t>());
  }
  std::vector<std::string> id2tok(max_id + 1);
  for (auto it = vocab.begin(); it != vocab.end(); ++it) {
    id2tok[it.value().get<size_t>()] = gpt2_decode_piece(it.key());
  }
  return id2tok;
}

int64_t argmax(const float* v, size_t n) {
  int64_t best = 0;
  float bv = v[0];
  for (size_t i = 1; i < n; ++i) {
    if (v[i] > bv) {
      bv = v[i];
      best = static_cast<int64_t>(i);
    }
  }
  return best;
}

std::string caption_from_tags(const RGBImage& img) {
  auto tags = classify_tags(img);
  if (tags.empty()) return "";
  std::string fallback = "A photo of " + tags[0].first;
  for (size_t i = 1; i < std::min<size_t>(3, tags.size()); ++i) {
    fallback += (i + 1 == std::min<size_t>(3, tags.size())) ? " and " : ", ";
    fallback += tags[i].first;
  }
  return fallback;
}

}  // namespace

std::string generate_caption(const RGBImage& img) {
  if (img.width <= 0 || img.height <= 0) return "";

  static std::vector<std::string> vocab;
  static int64_t decoder_start = 50256;
  static int64_t eos = 50256;
  static bool ready = false;
  if (!ready) {
    std::ifstream cfg(onnx::models_dir() + "/caption_config.json");
    nlohmann::json c;
    cfg >> c;
    decoder_start = c.value("decoder_start_token_id", 50256);
    eos = c.value("eos_token_id", 50256);
    vocab = load_vocab(onnx::models_dir() + "/caption_tokenizer.json");
    ready = true;
  }

  const int k = 224;
  auto chw = resize_rgb_chw(img, k, k);
  for (size_t i = 0; i < chw.size(); i += 3) {
    chw[i] = (chw[i] - 0.485f) / 0.229f;
    chw[i + 1] = (chw[i + 1] - 0.456f) / 0.224f;
    chw[i + 2] = (chw[i + 2] - 0.406f) / 0.225f;
  }
  Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  std::array<int64_t, 4> ishape{1, 3, k, k};
  Ort::Value pixel = Ort::Value::CreateTensor<float>(mem, chw.data(), chw.size(), ishape.data(), ishape.size());
  auto enc = onnx::run(onnx::caption_encoder(), {"pixel_values"}, {std::move(pixel)}, {"last_hidden_state"});
  float* hidden_ptr = enc[0].GetTensorMutableData<float>();
  auto hshape = enc[0].GetTensorTypeAndShapeInfo().GetShape();
  size_t hidden_len = 1;
  for (auto d : hshape) hidden_len *= static_cast<size_t>(d);
  std::vector<float> hidden_copy(hidden_ptr, hidden_ptr + hidden_len);

  std::vector<int64_t> tokens = {decoder_start};
  std::string caption;
  const int max_tokens = 16;
  for (int step = 0; step < max_tokens; ++step) {
    std::array<int64_t, 2> tshape{1, static_cast<int64_t>(tokens.size())};
    Ort::Value input_ids =
        Ort::Value::CreateTensor<int64_t>(mem, tokens.data(), tokens.size(), tshape.data(), tshape.size());
    Ort::Value hidden = Ort::Value::CreateTensor<float>(mem, hidden_copy.data(), hidden_copy.size(), hshape.data(),
                                                        hshape.size());
    std::vector<Ort::Value> inputs;
    inputs.push_back(std::move(input_ids));
    inputs.push_back(std::move(hidden));
    auto dec = onnx::run(onnx::caption_decoder(), {"input_ids", "encoder_hidden_states"}, std::move(inputs), {"logits"});
    float* logits = dec[0].GetTensorMutableData<float>();
    auto lshape = dec[0].GetTensorTypeAndShapeInfo().GetShape();
    int64_t vocab_size = lshape.back();
    int64_t seq_len = lshape[1];
    int64_t offset = (seq_len - 1) * vocab_size;
    int64_t next = argmax(logits + offset, static_cast<size_t>(vocab_size));
    if (next == eos) break;
    if (next >= 0 && next < static_cast<int64_t>(vocab.size())) caption += vocab[static_cast<size_t>(next)];
    tokens.push_back(next);
  }

  while (!caption.empty() && caption.front() == ' ') caption.erase(caption.begin());
  if (!caption.empty()) return caption;
  return caption_from_tags(img);
}

}  // namespace eyes
