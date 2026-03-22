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

  // note: this function is only safe in debug mode
  template <typename Addr, typename T>
  void write(Addr addr, T value) {
    assert(std::in_range<Address>(addr));
    const auto address = static_cast<Address>(addr);
    assert(n >= sizeof(T));   // ensure n - sizeof(T) >= 0
    assert(address >= base);  // ensure off - base >= 0
    // both sides of inequality below are non-negative so safe to compare
    assert(n - sizeof(T) >= address - base);  // ensure no overwrite
    // address - base <= n so now it is safe to cast to std::size_t
    std::memcpy(
      data + static_cast<std::size_t>(address - base), &value, sizeof(value)
    );
  }
};

}  // namespace mempeep::test