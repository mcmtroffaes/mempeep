// this is used by examples to simulate

#pragma once

#include <array>
#include <cstddef>
#include <cstring>

// No bounds checking, tiny address_type, for examples only.
struct BufferReader {
  using address_type = std::uint8_t;
  const char* data;

  bool operator()(std::uint8_t addr, std::size_t size, void* buf) const {
    std::memcpy(buf, data + addr, size);
    return true;
  }
};