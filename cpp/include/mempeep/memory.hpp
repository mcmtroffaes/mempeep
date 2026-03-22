#pragma once

#include <concepts>  // std::unsigned_integral, ...

namespace mempeep {

/** @brief Possible user address types. */
template <typename T>
concept IsAddress = std::unsigned_integral<T> && !std::same_as<T, bool>
                    && !std::same_as<T, char>;

/**
 * @brief Extract address_type from MemoryReader.
 */
template <typename MemoryReader>
using address_t = typename MemoryReader::address_type;

/**
 * @brief Functor concept to read a block of memory from a remote source.
 *
 * Copy `size` bytes into `buffer` from remote memory at `address`.
 * On failure, return false, otherwise return true.
 *
 * @param address Remote source address.
 * @param size    Number of bytes to read.
 * @param buffer  Native destination buffer.
 * @return        `true` on success, `false` on failure.
 */
template <typename MemoryReader>
concept IsMemoryReader = requires(
  MemoryReader reader,
  address_t<MemoryReader> address,
  std::size_t size,
  void* buffer
) {
  { reader(address, size, buffer) } -> std::same_as<bool>;
};

}  // namespace mempeep