#include "VozNet.h"

#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <cstring>
#include <memory>

#include "VozAudio.h"

#if !defined(SIMULATOR)
#include <SecureHttpClient.h>
#endif

namespace voz::net {

namespace {

// Long enough for a model to listen to a minute and write it out.
constexpr uint32_t kTimeoutMs = 90000;

struct Buffer {
  char* data = nullptr;
  size_t size = 0;
  ~Buffer() {
    if (data) audio::release(data);
  }
};

// The WAV file read into `into` at `at`, encoded as base64 when asked.
bool readWav(const char* path, char* into, const size_t at, const size_t fileBytes, const bool encode) {
  HalFile file;
  if (!Storage.openFileForRead("VOZ", path, file)) return false;
  // A multiple of three, so the pieces encode as one. Off the stack: the
  // loop task's is a few kilobytes.
  constexpr size_t kPiece = 12288;
  std::unique_ptr<uint8_t, void (*)(void*)> held(static_cast<uint8_t*>(audio::acquire(kPiece)), &audio::release);
  uint8_t* piece = held.get();
  if (!piece) return false;
  size_t o = at;
  for (size_t done = 0; done < fileBytes;) {
    const size_t want = fileBytes - done < kPiece ? fileBytes - done : kPiece;
    const int got = file.read(piece, want);
    if (got != static_cast<int>(want)) return false;
    if (encode) {
      o += base64(piece, want, into + o);
    } else {
      std::memcpy(into + o, piece, want);
      o += want;
    }
    done += want;
  }
  return true;
}

#if !defined(SIMULATOR)
int post(const std::string& url, const char* contentType, const std::string& authName, const std::string& authValue,
         const uint8_t* body, const size_t length, std::string& answer) {
  freeink::SecureHttpClient http;
  http.setInsecure();
  http.setTimeout(kTimeoutMs);
  http.setReuse(false);
  http.setUserAgent("CrossPlay-Voz");
  if (!http.begin(url)) return 0;
  http.addHeader("Content-Type", contentType);
  http.addHeader(authName, authValue);
  const int status = http.sendRequest("POST", body, length);
  answer = http.getString();
  http.end();
  LOG_INF("VOZ", "POST %s: %d, %u bytes back", url.c_str(), status, static_cast<unsigned>(answer.size()));
  return status < 0 ? 0 : status;
}
#endif

}  // namespace

bool transcribe(const char* wavPath, const Service service, const char* key, const Models& models, Note& note,
                int& status) {
  status = 0;
  if (!key || !key[0]) {
    status = -2;
    return false;
  }
  size_t fileBytes = 0;
  {
    HalFile file;
    if (!Storage.openFileForRead("VOZ", wavPath, file)) {
      status = -1;
      return false;
    }
    fileBytes = file.fileSize();
  }
  if (WiFi.status() != WL_CONNECTED) return false;

#if defined(SIMULATOR)
  (void)service;
  (void)models;
  (void)fileBytes;
  note.kind = Kind::Note;
  note.title = "Nota de prueba";
  note.summary = "El simulador no tiene micrófono ni envía nada.";
  note.transcript = "Esta nota la escribe el simulador para que las pantallas se puedan ver.";
  status = 200;
  return true;
#else
  // The TLS session needs internal memory the buffers below do not touch.
  if (ESP.getFreeHeap() < 40000 || ESP.getMaxAllocHeap() < 20000) {
    LOG_ERR("VOZ", "too little heap for TLS: %u", static_cast<unsigned>(ESP.getFreeHeap()));
    status = -3;
    return false;
  }
  std::string answer;
  if (service == Service::Gemini) {
    const std::string head = geminiHead();
    const char* tail = geminiTail();
    Buffer body;
    body.size = head.size() + base64Length(fileBytes) + std::strlen(tail);
    body.data = static_cast<char*>(audio::acquire(body.size));
    if (!body.data) {
      status = -3;
      return false;
    }
    std::memcpy(body.data, head.data(), head.size());
    if (!readWav(wavPath, body.data, head.size(), fileBytes, true)) {
      status = -1;
      return false;
    }
    std::memcpy(body.data + head.size() + base64Length(fileBytes), tail, std::strlen(tail));
    status = post(geminiUrl(models), "application/json", "x-goog-api-key", key,
                  reinterpret_cast<const uint8_t*>(body.data), body.size, answer);
    if (status != 200) return false;
    std::string text;
    if (!geminiText(answer, text) || !noteFrom(text, note)) {
      LOG_ERR("VOZ", "unreadable answer: %.200s", answer.c_str());
      status = -1;
      return false;
    }
    return true;
  }

  // OpenAI: the words first.
  const std::string head = multipartHead(models);
  const std::string tail = multipartTail();
  std::string transcript;
  {
    Buffer body;
    body.size = head.size() + fileBytes + tail.size();
    body.data = static_cast<char*>(audio::acquire(body.size));
    if (!body.data) {
      status = -3;
      return false;
    }
    std::memcpy(body.data, head.data(), head.size());
    if (!readWav(wavPath, body.data, head.size(), fileBytes, false)) {
      status = -1;
      return false;
    }
    std::memcpy(body.data + head.size() + fileBytes, tail.data(), tail.size());
    const std::string bearer = std::string("Bearer ") + key;
    status = post(kOpenAiTranscribeUrl, multipartType().c_str(), "Authorization", bearer,
                  reinterpret_cast<const uint8_t*>(body.data), body.size, answer);
  }
  if (status != 200) return false;
  if (!stringField(answer, "text", transcript) || transcript.empty()) {
    LOG_ERR("VOZ", "unreadable transcription: %.200s", answer.c_str());
    status = -1;
    return false;
  }
  // Then the title, the kind and the summary from the words.
  const std::string chat = chatBody(models, transcript);
  const std::string bearer = std::string("Bearer ") + key;
  status = post(kOpenAiChatUrl, "application/json", "Authorization", bearer,
                reinterpret_cast<const uint8_t*>(chat.data()), chat.size(), answer);
  std::string text;
  if (status == 200 && stringField(answer, "content", text) && noteFrom(text, note)) return true;
  // The words are worth keeping even when the summary did not come.
  LOG_ERR("VOZ", "summary failed (%d); keeping the transcript", status);
  note.kind = Kind::Note;
  note.title = "Nota sin resumen";
  note.summary.clear();
  note.transcript = transcript;
  plainPunctuation(note.transcript);
  status = 200;
  return true;
#endif
}

}  // namespace voz::net
