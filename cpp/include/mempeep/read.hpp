#pragma once

#include <limits>  // std::numeric_limits
#include <mempeep/descriptor.hpp>
#include <mempeep/memory.hpp>
#include <mempeep/tracer.hpp>

namespace mempeep::detail {

struct NoScope {};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsAddress Address, IsFieldsItem Item>
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

/**
 * @brief Reads from `addr` using the descriptor.
 *
 * Writes into `out`, and returns the cursor positioned just after the
 * bytes consumed at `addr` (not at the pointee for `Ref`, `Vector`, ...).
 */
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
  return advance(address, Pad<N>::count, tracer);
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
  return advance(base, Seek<N>::offset, tracer);
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
  RawAddr<AddrT> item,
  const MemoryReader& reader,
  address_t<MemoryReader> address,
  native_type_t<RawAddr<AddrT>>& target,  // AddrT
  Tracer& tracer
) {
  address_t<MemoryReader> raw{};
  auto cursor = read_value<Primitive<address_t<MemoryReader>>>(
    reader, address, raw, tracer
  );
  if (cursor) {
    target = static_cast<AddrT>(raw);
    if constexpr (requires { tracer.value(target); }) {
      tracer.value(target);
    }
  }
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
  NullableRef<Desc> item,
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
  Array<Desc, N> item,
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
  Vector<Desc, MaxLen> item,
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
  if (vector_cursor && vector_cursor.value() != end_ptr) {
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
  CircularList<Desc, Next, MaxLen> item,
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
    if (!read_value<Desc>(reader, *list_cursor, elem, tracer)) return {};
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

// read_value dispatch
template <IsDescriptor Desc, IsMemoryReader MemoryReader, IsTracer Tracer>
[[nodiscard]] Cursor<MemoryReader> read_value(
  const MemoryReader& reader,
  address_t<MemoryReader> addr,
  native_type_t<Desc>& out,
  Tracer& tracer
) {
  return read_value_impl(Desc{}, reader, addr, out, tracer);
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