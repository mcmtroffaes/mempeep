#pragma once

#include <cstring>  // std::memcpy
#include <mempeep/memory.hpp>
#include <string>  // std::data, std::size

namespace mempeep::test {

/**
 * @brief Mock memory reader which always gives same result.
 */
template <typename Address, bool Result>
  requires IsAddress<Address>
struct MockMemoryConst {
  using address_type = Address;

  static bool operator()(Address address, std::size_t size, void* buffer) {
    return Result;
  }
};

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
template <typename Address, auto& Data>
  requires IsAddress<Address>
struct MockMemoryReader {
  using address_type = Address;
  static constexpr auto data = std::data(Data);
  static constexpr auto data_size = std::size(Data);

  static bool operator()(Address address, std::size_t size, void* buffer) {
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    if (size > data_size) return false;
    if (address > data_size - size) return false;
    std::memcpy(buffer, data + address, size);
    return true;
  }
};

}  // namespace mempeep::test