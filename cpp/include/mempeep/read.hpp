#pragma once

#include <limits>  // std::numeric_limits
#include <mempeep/layout.hpp>
#include <mempeep/memory.hpp>
#include <mempeep/tracer.hpp>

namespace mempeep::detail {

struct NoScope {};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsAddress Address, IsLayoutItem Item>
auto make_scope(Tracer& tracer, Address address, Item item) {
  if constexpr (IsScopedTracer<Tracer>) {
    const auto addr = static_cast<std::uint64_t>(address);
    return typename Tracer::Scope(tracer, addr, item);
  } else {
    return NoScope{};
  }
}

// Abstract unsigned addition with overflow check.
template <std::unsigned_integral S, std::unsigned_integral T>
[[nodiscard]] constexpr std::optional<S> checked_add(S s, T t) noexcept {
  if (t > std::numeric_limits<S>::max() - s) return {};
  return static_cast<S>(s + t);
}

// Advance address by n with traced error in case of overflow.
template <IsAddress Addr, IsTracer Tracer>
[[nodiscard]] std::optional<Addr> advance(
  Addr addr, std::size_t n, Tracer& tracer
) {
  auto u = checked_add(addr, n);
  if (!u) tracer.error(Error::ADDRESS_OVERFLOW);
  return u;
}

// Cursor tracks the current read position within a layout.
//
// It starts at the layout's base address and advances as each item is read.
// It becomes nullopt when the current position can no longer be determined,
// for example, due to a failed memory read or an address overflow.
//
// Key invariant: a cursor only becomes nullopt if an error was already
// reported to the tracer. nullopt without a tracer error never occurs.
//
// Errors are contained locally: a child layout's cursor becoming nullopt
// does not invalidate the parent's cursor. This allows reading to continue
// past failed items (e.g. a bad address), recovering as much data as
// possible. Child errors are still reported through the tracer.
template <IsMemoryReader MemoryReader>
using Cursor = std::optional<address_t<MemoryReader>>;

// Forward declaration to support recursive reading:
// read -> read_layout -> read_layout_item -> read.
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_into(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
);

// thin wrapper around reader with tracing
template <IsMemoryReader MemoryReader, IsTracer Tracer, typename T>
[[nodiscard]] Cursor<MemoryReader> read_bytes(
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  if (reader(address, sizeof(target), &target)) {
    return advance(address, sizeof(target), tracer);
  } else {
    tracer.error(Error::READ_FAILED);
    return {};
  }
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Pad<N> item,
  const MemoryReader&,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return advance(address, Pad<N>::count, tracer);
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Seek<N> item,
  const MemoryReader&,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return advance(base, Seek<N>::offset, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Field<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  auto& field = target.*M;
  return read_into(reader, address, field, tracer);
}

template <IsAddress T, IsMemoryReader MemoryReader, IsTracer Tracer>
  requires(
    std::numeric_limits<address_t<MemoryReader>>::max()
    <= std::numeric_limits<T>::max()
  )
[[nodiscard]] Cursor<MemoryReader> read_address_into(
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  address_t<MemoryReader> ptr{};
  auto cursor = read_bytes(reader, address, ptr, tracer);
  // static_cast safe since target can hold ptr by requires
  if (cursor) {
    target = static_cast<T>(ptr);
    if constexpr (requires { tracer.value(target); }) {
      tracer.value(target);
    }
  }
  return cursor;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  RawAddr<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return read_address_into(reader, address, target.*M, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Ref<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_address_into(reader, address, target_ptr, tracer);
  if (!cursor) return {};
  if (target_ptr) {
    // we always try to read as much as possible
    // so ignore output since cursor is still valid, only inner read failed
    std::ignore = read_into(reader, target_ptr, target.*M, tracer);
  } else {
    tracer.error(Error::ADDRESS_NULL);
  }
  return cursor;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<unwrap_optional_t<member_type_t<M>>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  NullableRef<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_address_into(reader, address, target_ptr, tracer);
  if (!cursor) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    auto& target_value = field.emplace();
    // we always try to read as much as possible
    // so ignore output since cursor is still valid, only inner read failed
    // keep field emplaced even if read fails to retain partially read data
    std::ignore = read_into(reader, target_ptr, target_value, tracer);
  }
  // note: null target_ptr is ok, no error reported
  return cursor;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<unwrap_array_t<member_type_t<M>>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Array<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  auto& field = target.*M;
  Cursor<MemoryReader> cursor{address};
  for (auto& elem : field) {
    cursor = read_into(reader, cursor.value(), elem, tracer);
    if (!cursor) return {};  // quit when cursor becomes invalid
  }
  return cursor;
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<unwrap_vector_t<member_type_t<M>>>
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  Vector<M> item,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  address_t<MemoryReader> begin_ptr{};
  auto cursor = read_address_into(reader, address, begin_ptr, tracer);
  if (!cursor) return {};
  address_t<MemoryReader> end_ptr{};
  cursor = read_address_into(reader, cursor.value(), end_ptr, tracer);
  if (!cursor) return {};
  if (begin_ptr == 0) {
    tracer.error(Error::ADDRESS_NULL);
    return cursor;
  }
  if (begin_ptr > end_ptr) {
    tracer.error(Error::VECTOR_INVALID_RANGE);
    return cursor;
  }
  auto& field = target.*M;
  Cursor<MemoryReader> vector_cursor{begin_ptr};
  field.clear();
  while (vector_cursor && vector_cursor.value() < end_ptr) {
    auto& elem = field.emplace_back();
    vector_cursor = read_into(reader, vector_cursor.value(), elem, tracer);
  }
  if (vector_cursor && vector_cursor.value() != end_ptr) {
    tracer.error(Error::VECTOR_MISALIGNED);
  }
  return cursor;
}

template <
  auto M,
  auto N,
  std::size_t L,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
  requires IsReadable<unwrap_vector_t<member_type_t<M>>>
           && IsAddress<member_type_t<N>>
           && (std::numeric_limits<address_t<MemoryReader>>::max() <= std::numeric_limits<member_type_t<N>>::max())
[[nodiscard]] Cursor<MemoryReader> read_layout_item(
  CircularList<M, N, L> item,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  address_t<MemoryReader> head_ptr{};
  auto cursor = read_address_into(reader, address, head_ptr, tracer);
  if (!cursor) return {};
  if (head_ptr == 0) return cursor;  // empty list
  auto& field = target.*M;
  Cursor<MemoryReader> list_cursor{head_ptr};
  field.clear();
  do {
    // TODO max size to avoid overflow
    auto& elem = field.emplace_back();
    if (!read_into(reader, list_cursor.value(), elem, tracer)) return {};
    list_cursor = static_cast<address_t<MemoryReader>>(elem.*N);
  } while (list_cursor && list_cursor.value() != head_ptr && field.size() < L);
  return cursor;
}

template <
  IsLayoutItem... Items,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_layout(
  Layout<Items...>,
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  Cursor<MemoryReader> cursor{base};
  // Process each layout item in order, stopping if the cursor becomes nullopt.
  // This is a comma fold: (expr, ...) evaluates each expr left-to-right.
  // Each expr is: cursor && (cursor = read_layout_item(...))
  // The && is plain short-circuit evaluation, not a fold operator:
  // if cursor is nullopt (falsy), the assignment is skipped.
  // Items{} constructs a tag value at zero cost to select the right overload.
  ((
     cursor
     && (cursor = read_layout_item(Items{}, reader, base, cursor.value(), target, tracer))
   ),
   ...);
  return cursor;
}

template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_into(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  if constexpr (IsStruct<T>) {
    return detail::read_layout(
      fields_t<T>{}, reader, base, target, tracer
    );
  } else {
    auto cursor = detail::read_bytes(reader, base, target, tracer);
    if constexpr (requires { tracer.value(target); }) {
      if (cursor) tracer.value(target);
    }
    return cursor;
  }
}

}  // namespace mempeep::detail

namespace mempeep {

/**
 * @brief Reads data from remote memory into a native object based on a
 * specified layout.
 *
 * The function will try to read as much data as possible, i.e. even after
 * failing to read subfields.
 * Returns the result of `tracer.success()`.
 * By convention, this value is convertible to bool, and evaluates to true
 * if no errors were reported, and false otherwise.
 *
 * @tparam MemoryReader The type for the reader callback.
 * @tparam T          The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryReader` function.
 * @param base   The remote address to start reading from.
 * @param target The native object to populate.
 * @return The result of `tracer.success()` (convertible to bool).
 */
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
auto read(
  const MemoryReader& reader,
  address_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  // Passing tracer by reference internally so all recursive calls share
  // the same state, without copying on each call.
  std::ignore = detail::read_into(reader, base, target, tracer);
  return tracer.success();
}

}  // namespace mempeep