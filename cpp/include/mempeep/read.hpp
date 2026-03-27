#pragma once

#include <concepts>  // std::unsigned_integral
#include <cstdint>   // std::uint64_t
#include <limits>    // std::numeric_limits
#include <mempeep/concepts/memory.hpp>
#include <mempeep/concepts/tracer.hpp>
#include <mempeep/descriptor.hpp>
#include <optional>  // std::optional
#include <utility>   // std::ignore

namespace mempeep::detail {

struct NoScope {};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
// make_scope is called only for layout items (Field, Pad, Seek),
// not for descriptors (Struct, Array, ...). This is intentional:
// scoping is a layout-level concept, not a value-reading concept.
template <IsTracer Tracer, IsAddress Address, IsFieldsItem Item>
auto make_scope(Tracer& tracer, Address address, Item item) {
  if constexpr (IsScopedTracer<Tracer>) {
    const auto addr = static_cast<std::uint64_t>(address);
    return typename Tracer::Scope(tracer, addr, item);
  } else {
    return NoScope{};
  }
}

template <IsTracer Tracer, IsAddress Address, IsDescriptor Desc>
auto make_desc_scope(Tracer& tracer, Address address, Desc desc) {
  if constexpr (IsDescScopedTracer<Tracer>) {
    const auto addr = static_cast<std::uint64_t>(address);
    return typename Tracer::DescScope(tracer, addr, desc);
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

// Forward declaration for mutual recursion.
template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value(
  const MemoryReader& reader,
  address_t<MemoryReader> addr,
  native_type_t<Desc>& out,
  Tracer& tracer
);

template <IsPrimitive T, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  Primitive<T>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Primitive<T>>& target,  // T
  Tracer& tracer
) {
  if (reader(address, sizeof(target), &target)) {
    if constexpr (requires { tracer.value(target); }) {
      tracer.value(target);
    }
    return advance(address, sizeof(target), tracer);
  } else {
    tracer.error(Error::READ_FAILED);
    return {};
  }
}

template <std::size_t Len, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  String<Len>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<String<Len>>& target,  // std::string
  Tracer& tracer
) {
  char buffer[Len]{};
  if (reader(address, sizeof(buffer), &buffer)) {
    target = std::string(buffer, Len);
    if constexpr (requires { tracer.value(target); }) {
      tracer.value(target);
    }
    return advance(address, Len, tracer);
  } else {
    tracer.error(Error::READ_FAILED);
    return {};
  }
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_fields_item(
  Pad<N> item,
  const MemoryReader&,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return advance(address, N, tracer);
}

template <auto N, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_fields_item(
  Seek<N> item,
  const MemoryReader&,
  address_t<MemoryReader> base,
  address_t<MemoryReader> address,
  auto&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return advance(base, N, tracer);
}

template <
  IsDescriptor Desc,
  auto M,
  IsMemoryReader MemoryReader,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_fields_item(
  Field<Desc, M> item,
  const MemoryReader& reader,
  address_t<MemoryReader>,
  address_t<MemoryReader> address,
  detail::member_class_t<M>& target,  // ensure target.*M is valid
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, item);
  return read_value<Desc>(reader, address, target.*M, tracer);
}

// read_value_impl are the dispatch implementations for read_value
// dispatch happens on first argument

template <IsAddress AddrT, IsMemoryReader MemoryReader, IsTracer Tracer>
  requires(
    std::numeric_limits<address_t<MemoryReader>>::max()
    <= std::numeric_limits<AddrT>::max()
  )
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  RawAddr<AddrT>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<RawAddr<AddrT>>& target,  // AddrT
  Tracer& tracer
) {
  address_t<MemoryReader> raw{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, raw, tracer
  );
  if (cursor) target = static_cast<AddrT>(raw);
  return cursor;
}

template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  Ref<Desc>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Ref<Desc>>& target,  // native_type_t<Desc>
  Tracer& tracer
) {
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, target_ptr, tracer
  );
  if (!cursor) return {};
  if (target_ptr) {
    // we always try to read as much as possible
    // so ignore output since cursor is still valid, only inner read failed
    std::ignore = read_value<Desc>(reader, target_ptr, target, tracer);
  } else {
    tracer.error(Error::ADDRESS_NULL);
  }
  return cursor;
}

template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  NullableRef<Desc>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<NullableRef<Desc>>& target,  // std::optional
  Tracer& tracer
) {
  address_t<MemoryReader> target_ptr{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, target_ptr, tracer
  );
  if (!cursor) return {};
  target.reset();
  if (target_ptr) {
    auto& target_value = target.emplace();
    // we always try to read as much as possible
    // so ignore output since cursor is still valid, only inner read failed
    // keep field emplaced even if read fails to retain partially read data
    std::ignore = read_value<Desc>(reader, target_ptr, target_value, tracer);
  }
  // note: null target_ptr is ok, no error reported
  return cursor;
}

template <
  IsDescriptor Desc,
  std::size_t N,
  IsMemoryReader MemoryReader,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  Array<Desc, N>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Array<Desc, N>>& target,  // std::array
  Tracer& tracer
) {
  Cursor<MemoryReader> cursor{address};
  for (auto& elem : target) {
    if (!cursor) return {};  // quit when cursor becomes invalid
    cursor = read_value<Desc>(reader, *cursor, elem, tracer);
  }
  return cursor;
}

template <
  IsDescriptor Desc,
  std::size_t MaxLen,
  IsMemoryReader MemoryReader,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  Vector<Desc, MaxLen>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Vector<Desc, MaxLen>>& target,  // std::vector
  Tracer& tracer
) {
  address_t<MemoryReader> begin_ptr{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, begin_ptr, tracer
  );
  if (!cursor) return {};
  address_t<MemoryReader> end_ptr{};
  cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, *cursor, end_ptr, tracer
  );
  if (!cursor) return {};
  if (begin_ptr == 0) {
    tracer.error(Error::ADDRESS_NULL);
    return cursor;
  }
  if (begin_ptr > end_ptr) {
    tracer.error(Error::VECTOR_INVALID_RANGE);
    return cursor;
  }
  std::size_t count = 0;
  target.clear();
  Cursor<MemoryReader> vector_cursor{begin_ptr};
  while (vector_cursor && *vector_cursor < end_ptr) {
    auto& elem = target.emplace_back();
    vector_cursor = read_value<Desc>(reader, *vector_cursor, elem, tracer);
    if (++count > MaxLen) {
      tracer.error(Error::VECTOR_TOO_LONG);
      return cursor;
    }
  }
  if (vector_cursor && *vector_cursor != end_ptr) {
    tracer.error(Error::VECTOR_MISALIGNED);
  }
  return cursor;
}

template <
  IsDescriptor Desc,
  auto Next,
  std::size_t MaxLen,
  IsMemoryReader MemoryReader,
  IsTracer Tracer>
  requires(
    std::numeric_limits<address_t<MemoryReader>>::max()
    <= std::numeric_limits<detail::member_type_t<Next>>::max()
  )
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  CircularList<Desc, Next, MaxLen>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<CircularList<Desc, Next, MaxLen>>& target,
  Tracer& tracer
) {
  address_t<MemoryReader> head_ptr{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, head_ptr, tracer
  );
  if (!cursor) return {};
  if (head_ptr == 0) return cursor;  // empty list
  Cursor<MemoryReader> list_cursor{head_ptr};
  std::size_t count = 0;
  target.clear();
  do {
    auto& elem = target.emplace_back();
    if (!read_value<Desc>(reader, *list_cursor, elem, tracer)) return cursor;
    list_cursor = static_cast<address_t<MemoryReader>>(elem.*Next);
    if (!list_cursor) {
      tracer.error(Error::ADDRESS_NULL);
      return cursor;
    }
    if (++count > MaxLen) {
      tracer.error(Error::CIRCULAR_LIST_TOO_LONG);
      return cursor;
    }
  } while (*list_cursor != head_ptr);
  return cursor;
}

template <
  IsFieldsItem... Items,
  typename T,
  IsMemoryReader MemoryReader,
  IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value_impl(
  Struct<T, Fields<Items...>>,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Struct<T, Fields<Items...>>>& target,  // T
  Tracer& tracer
) {
  Cursor<MemoryReader> cursor{address};
  // Process each field item in order, stopping if the cursor becomes nullopt.
  // This is a comma fold: (expr, ...) evaluates each expr left-to-right.
  // Each expr is: cursor && (cursor = read_fields_item(...))
  // The && is plain short-circuit evaluation, not a fold operator:
  // if cursor is nullopt (falsy), the assignment is skipped.
  // Items{} constructs a tag value at zero cost to select the right overload.
  ((
     cursor
     && (cursor = read_fields_item(Items{}, reader, address, *cursor, target, tracer))
   ),
   ...);
  return cursor;
}

/**
 * @brief Reads value from `addr` using the descriptor `Desc`.
 *
 * Writes into `out`, and returns the cursor positioned just after the
 * bytes consumed at `addr` (not at the pointee for `Ref`, `Vector`, ...).
 */
template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value(
  const MemoryReader& reader,
  address_t<MemoryReader> addr,
  native_type_t<Desc>& out,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_desc_scope(tracer, addr, Desc{});
  return read_value_impl(Desc{}, reader, addr, out, tracer);
}

}  // namespace mempeep::detail

namespace mempeep {

/**
 * @brief Reads data from remote memory into a native object.
 *
 * Reads `native_type_t<Desc>` from `address` using `reader`, populating
 * `target`. Attempts to read as much as possible even after partial
 * failures. Returns the result of `tracer.success()`.
 *
 * @tparam Desc          Descriptor controlling how the value is read.
 * @tparam MemoryReader  Type satisfying IsMemoryReader.
 * @tparam Tracer        Type satisfying IsTracer.
 * @param reader  The memory reader.
 * @param address Remote address to read from.
 * @param target  Native object to populate.
 * @param tracer  Receives error reports; its `success()` is returned.
 * @return The result of `tracer.success()` (convertible to bool).
 */
template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] auto read(
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<Desc>& target,
  Tracer& tracer
) {
  std::ignore = detail::read_value<Desc>(reader, address, target, tracer);
  return tracer.success();
};

}  // namespace mempeep