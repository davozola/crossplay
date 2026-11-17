#pragma once

// FONDOS: the reader's wallpapers. A pack of pictures made for this panel, and
// two things to do with them: keep one as the sleep screen, or have the sleep
// screen be a different one each time.
//
// This is the part with no card in it: how the pack is laid out. Freestanding,
// so host-tests/fondos checks the pack that ships.
//
// The pack: "FND1", count (u16 LE), bytes per picture (u32 LE), then for each
// picture 96 bytes of text (title[56], artist[40], NUL padded, Latin-1), then
// the pictures, each a complete one-bit BMP of exactly that many bytes. It is
// built by tools_local/wallpapers/make_pack.py.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace fondos {

constexpr size_t kHeaderBytes = 10;
constexpr size_t kTitleBytes = 56;
constexpr size_t kArtistBytes = 40;
constexpr size_t kTextBytes = kTitleBytes + kArtistBytes;
constexpr int kMaxPictures = 200;

struct Header {
  int count = 0;
  uint32_t pictureBytes = 0;
  bool ok() const { return count > 0; }
  // Where picture `index`'s text and bytes start, from the top of the pack.
  size_t textAt(const int index) const { return kHeaderBytes + static_cast<size_t>(index) * kTextBytes; }
  size_t pictureAt(const int index) const {
    return kHeaderBytes + static_cast<size_t>(count) * kTextBytes + static_cast<size_t>(index) * pictureBytes;
  }
  size_t packBytes() const { return pictureAt(count); }
};

// Reads the ten bytes at the top of a pack. A header that is not one, or that
// describes more than `available` bytes hold, comes back with no pictures.
inline Header parseHeader(const uint8_t* bytes, const size_t available) {
  Header h;
  if (available < kHeaderBytes || std::memcmp(bytes, "FND1", 4) != 0) return h;
  const int count = bytes[4] | (bytes[5] << 8);
  const uint32_t each = static_cast<uint32_t>(bytes[6]) | (static_cast<uint32_t>(bytes[7]) << 8) |
                        (static_cast<uint32_t>(bytes[8]) << 16) | (static_cast<uint32_t>(bytes[9]) << 24);
  if (count <= 0 || count > kMaxPictures || each < 62 || each > 1024u * 1024u) return h;
  h.count = count;
  h.pictureBytes = each;
  if (h.packBytes() > available) h = Header{};
  return h;
}

// What the sleep screen does with the pack.
enum class Mode : uint8_t {
  Off,    // FONDOS has not been asked for anything
  One,    // the picture chosen, every time
  Every,  // a different one each time the reader sleeps
};

// The two settings as bytes for the card, and back.
constexpr int kStateBytes = 4;
inline void packState(const Mode mode, const int chosen, uint8_t* out) {
  out[0] = 1;  // version
  out[1] = static_cast<uint8_t>(mode);
  out[2] = static_cast<uint8_t>(chosen & 0xFF);
  out[3] = static_cast<uint8_t>((chosen >> 8) & 0xFF);
}
inline bool unpackState(const uint8_t* in, const int size, const int count, Mode& mode, int& chosen) {
  mode = Mode::Off;
  chosen = 0;
  if (size < kStateBytes || in[0] != 1 || in[1] > static_cast<uint8_t>(Mode::Every)) return false;
  const int index = in[2] | (in[3] << 8);
  if (index >= count) return false;
  mode = static_cast<Mode>(in[1]);
  chosen = index;
  return true;
}

// Steps round the pack.
inline int step(const int current, const int direction, const int count) {
  return count <= 0 ? 0 : ((current + direction) % count + count) % count;
}

}  // namespace fondos
