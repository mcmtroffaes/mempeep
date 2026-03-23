#pragma once

#include <cstddef>
#include <cstring>
#include <mempeep/memory.hpp>

namespace mempeep::test {

/**
 * @brief Mock memory reader backed by an immutable string literal.
 * Addresses are zero-based byte indices into the buffer.
 *
 * Construct from a string literal:
 *
 *   MockMemoryReader<uint8_t> reader{"\x01\x02\x03"};
 *
 * The buffer length is deduced from the string literal.
 *
 * All reads are bounds-checked.  Reads that extend past the end of the buffer
 * fail gracefully by returning false without touching the output buffer.
 */
template <typename Address>
  requires IsAddress<Address>
struct MockMemoryReader {
  using address_type = Address;

  template <std::size_t N>
  explicit constexpr MockMemoryReader(const char (&literal)[N])
      : _data(literal), _size(N) {}

  bool operator()(Address address, std::size_t size, void* buffer) const {
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    if (size > _size) return false;
    if (address > _size - size) return false;
    std::memcpy(buffer, _data + address, size);
    return true;
  }

private:
  const char* _data;
  std::size_t _size;
};

}  // namespace mempeep::test