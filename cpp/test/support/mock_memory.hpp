#pragma once

#include <cstring>  // std::memcpy
#include <mempeep/concepts/address.hpp>
#include <string>  // std::data, std::size

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
  const char* data;
  const std::size_t data_size;

  MockMemoryReader(auto& src)
      : data(reinterpret_cast<const char*>(std::data(src))),
        data_size(std::size(src)) {};

  bool operator()(Address address, std::size_t size, void* buffer) const {
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    if (size > data_size) return false;
    if (address > data_size - size) return false;
    std::memcpy(buffer, data + address, size);
    return true;
  }
};

}  // namespace mempeep::test