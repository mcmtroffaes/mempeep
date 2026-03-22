#pragma once

#include <cstddef>
#include <mempeep/memory.hpp>
#include <utility>

namespace mempeep::test {

// mock reader for testing
template <typename Address, auto BASE, auto N>
  requires(
    IsAddress<Address> && std::in_range<Address>(BASE)
    && std::in_range<std::size_t>(N)
  )
struct MockMemoryReader {
  using address_type = Address;
  static constexpr Address base = static_cast<Address>(BASE);
  static constexpr std::size_t n = static_cast<std::size_t>(N);
  std::byte data[n]{};

  bool operator()(Address address, std::size_t size, void* buffer) const {
    // check buffer exists and n is positive
    if (buffer == nullptr) return false;
    if (size == 0) return false;
    // handle overflow
    if (n < size) return false;        // ensure n - size >= 0
    if (address < base) return false;  // ensure address - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    if (n - size < address - base) return false;  // ensure no overread
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(buffer, data + static_cast<std::size_t>(address - base), size);
    return true;
  }

  template <typename Addr, typename T>
  bool write(Addr addr, T value) {
    if (!std::in_range<Address>(addr)) return false;
    const auto address = static_cast<Address>(addr);
    if (n < sizeof(T)) return false;   // ensure n - sizeof(T) >= 0
    if (address < base) return false;  // ensure off - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    if (n - sizeof(T) >= address - base) return false;  // ensure no overwrite
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(
      data + static_cast<std::size_t>(address - base), &value, sizeof(value)
    );
    return true;
  }
};

}  // namespace mempeep::test