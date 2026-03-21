#include <cassert>      // assert
#include <cstdint>      // std::int32_t, ...
#include <cstring>      // std::memcpy
#include <format>       // std::format
#include <functional>   // std::function
#include <iostream>     // std::cout, ...
#include <optional>     // std::optional
#include <type_traits>  // std::same_as, ...
#include <utility>      // std::cmp_less_equal

namespace mempeep {

// ============================================================
// Integer concepts
// ============================================================

// Shorthand.
template <typename T>
concept IsInteger
  = std::is_integral_v<T> && !std::same_as<T, bool> && !std::same_as<T, char>;

namespace detail {

template <IsInteger T>
struct TypeRange {
  static constexpr auto min = std::numeric_limits<T>::lowest();
  static constexpr auto max = std::numeric_limits<T>::max();
};

template <IsInteger auto N>
struct ValueRange {
  static constexpr auto min = N;
  static constexpr auto max = N;
};

// check FromRange is inside ToRange
// i.e. ToRange::min <= FromRange::min && FromRange::max <= ToRange::max
// note: use std::cmp_less_equal to avoid signed/unsigned comparison pitfalls
template <typename FromRange, typename ToRange>
concept IsInRange = std::cmp_less_equal(ToRange::min, FromRange::min)
                    && std::cmp_less_equal(FromRange::max, ToRange::max);

}  // namespace detail

template <typename From, typename To>
concept IsTypeRepresentableAs
  = detail::IsInRange<detail::TypeRange<From>, detail::TypeRange<To>>;

// ============================================================
// Member traits
// ============================================================

template <auto M>
concept IsMember = std::is_member_object_pointer_v<decltype(M)>;

template <auto M>
  requires IsMember<M>
struct member_traits;

template <typename C, typename T, T C::* M>
struct member_traits<M> {
  using member_type = T;
};

template <auto M>
  requires IsMember<M>
using member_type_t = typename member_traits<M>::member_type;

// helper to detect std::optional
template <typename T>
struct is_optional : std::false_type {};

template <typename U>
struct is_optional<std::optional<U>> : std::true_type {};

template <auto M>
  requires is_optional<member_type_t<M>>::value
struct member_optional_traits;

template <typename C, typename U, std::optional<U> C::* M>
struct member_optional_traits<M> {
  using optional_value_type = U;
};

template <auto M>
  requires is_optional<member_type_t<M>>::value
using optional_value_type_t =
  typename member_optional_traits<M>::optional_value_type;

// ============================================================
// Tracing
// ============================================================

// note: uint64_t address avoids dependence on MemoryReader
// this is ok for logging, and keeps implementation simple
template <typename Tracer>
concept IsTracer
  = requires(Tracer& tracer, std::uint64_t address, std::string_view s) {
      typename Tracer::Scope;
      { Tracer::Scope(tracer, address, s) };
      { tracer.error(s) } -> std::same_as<void>;
    };

namespace detail {

// Helper class to get default tracer argument
template <IsTracer Tracer>
Tracer& default_tracer() {
  static Tracer instance{};
  return instance;
}

}  // namespace detail

// Disables tracing
struct NoTracer {
  struct Scope {
    Scope() = default;

    Scope(NoTracer&, std::uint64_t, std::string_view) {}
  };

  void error(std::string_view) {}
};

// Deduces Tracer::Scope from Tracer, avoiding repetition at call sites
template <IsTracer Tracer, IsInteger N>
auto make_scope(Tracer& tracer, N address, std::string_view label) {
  if constexpr (std::same_as<Tracer, NoTracer>) {
    return NoTracer::Scope{};  // empty, zero cost
  } else {
    return typename Tracer::Scope(
      tracer, static_cast<std::uint64_t>(address), label
    );
  }
}

// Returns the scope so the caller controls its lifetime
template <IsTracer Tracer, IsInteger N, typename F>
  requires std::invocable<F>
           && std::same_as<std::invoke_result_t<F>, std::string>
auto make_lazy_scope(Tracer& tracer, N address, F&& label_fn) {
  if constexpr (std::same_as<Tracer, NoTracer>) {
    return NoTracer::Scope{};
  } else {
    return typename Tracer::Scope(
      tracer, static_cast<std::uint64_t>(address), label_fn()
    );
  }
}

// Simple tracer which prints errors along with the address where they occurred.
struct PrintTracer {
  int indent = 0;
  uint64_t address = 0;

  void error(std::string_view reason) {
    auto whitespace = std::string(indent, ' ');
    std::cout << std::format("[{:08X}] ", address) << whitespace << whitespace
              << reason << std::endl;
  }

  struct Scope {
    PrintTracer& t;

    Scope(PrintTracer& _t, uint64_t address, std::string_view label) : t(_t) {
      t.address = address;
      t.error(label);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};

template <auto M>
consteval std::string_view member_name() {
#if defined(_MSC_VER)
  constexpr std::string_view sig = __FUNCSIG__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind('>');
#else
  constexpr std::string_view sig = __PRETTY_FUNCTION__;
  constexpr auto last_colon = sig.rfind(':');
  constexpr auto close = sig.rfind(']');
#endif
  static_assert(
    last_colon != std::string_view::npos,
    "member_name(): failed to find ':' in function signature"
  );
  static_assert(
    close != std::string_view::npos,
    "member_name(): failed to find closing delimiter in function signature"
  );
  static_assert(
    last_colon + 1 < close, "member_name(): invalid substring bounds"
  );
  constexpr auto len = close - last_colon - 1;
  static_assert(len > 0, "member_name(): extracted name is empty");
  return sig.substr(last_colon + 1, close - last_colon - 1);
}

struct TestMemberName {
  int the_member;
};

static_assert(member_name<&TestMemberName::the_member>() == "the_member");

// ============================================================
// Layout items
// ============================================================

/**
 * @brief For tagging layout items.
 *
 * The IsLayoutItem concept selects those types that have this tag.
 */
struct LayoutItem {};

template <typename T>
concept IsLayoutItem
  = std::is_base_of_v<LayoutItem, T> && !std::same_as<T, LayoutItem>;

/**
 * @brief Defines a remote layout.
 *
 * Each item must be a Field, Pad, or Seek.
 * Example:
 *
 *   Layout<Field<&Pos::x>, Pad<4>, Field<&Pos::y>>
 *
 * @tparam Items Sequence of Field, Seek, and Pad types.
 */
template <IsLayoutItem... Items>
struct Layout {};

template <typename T>
concept IsReadable
  = std::is_standard_layout_v<T> && std::is_default_constructible_v<T>;

/**
 * @brief Tag for registering the remote layout of a native struct.
 *
 * Example:
 *
 *   auto layout_of(LayoutOf<Pos>) -> Layout<Field<&Pos::x>, Pad<4>,
 * Field<&Pos::y>>;
 *
 * @tparam T Native struct to register the layout for.
 */
template <typename T>
  requires IsReadable<T>
struct LayoutOf {};

/**
 * @brief Does T have a custom layout?
 *
 * Checks if the function layout_of(LayoutOf<T>) exists.
 */
template <typename T>
concept HasLayout = requires { layout_of(LayoutOf<T>{}); };

/**
 * @brief Shorthand for the return type of `layout_of(LayoutOf<T>{})`.
 */
template <typename T>
  requires HasLayout<T>
using layout_of_t = decltype(layout_of(LayoutOf<T>{}));

/**
 * @brief A field. Its type must be readable.
 *
 * For example, Field<&Class::member>.
 *
 * @tparam M The native field to deserialize into.
 */
template <auto M>
  requires IsReadable<member_type_t<M>>
struct Field : LayoutItem {};

/**
 * @brief Padding relative to the current position in the layout.
 * @tparam N Number of bytes.
 *           Its value must be representable by pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Pad : LayoutItem {};

/**
 * @brief Absolute offset relative to base position of the layout.
 * @tparam N The offset (in bytes).
 *           Its value must be representable by pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <IsInteger auto N>
struct Seek : LayoutItem {};

/**
 * @brief Raw address, not followed.
 * @tparam M The native field to deserialize the address into.
 *           Its type must be wide enough to hold pointer_type_t<MemoryReader>.
 *           The read template will not instantiate otherwise.
 */
template <auto M>
  requires IsInteger<member_type_t<M>>
struct Ptr : LayoutItem {};

/**
 * @brief Follow address and read pointee.
 * @tparam M The native field to deserialize the pointee into.
 *           Must have type std::optional<T>.
 * @tparam Required If true (default), a null address is an error.
 */
template <auto M, bool Required = true>
  requires IsReadable<member_type_t<M>>
struct Ref : LayoutItem {};

template <auto M>
using NullableRef = Ref<M, false>;

/**
 * @brief Extract pointer_type from MemoryReader.
 */
template <typename MemoryReader>
using pointer_type_t = typename MemoryReader::pointer_type;

/**
 * @brief Functor concept to read a block of memory from a remote source.
 *
 * Copy `size` bytes into `buffer` from remote memory at `address`.
 * On failure, return false, otherwise return true.
 *
 * @param address Remote source pointer.
 * @param size    Number of bytes to read.
 * @param buffer  Native destination buffer.
 * @return        `true` on success, `false` on failure.
 */
template <typename MemoryReader>
concept IsMemoryReader = requires(
  MemoryReader reader,
  pointer_type_t<MemoryReader> address,
  std::size_t size,
  void* buffer
) {
  { reader(address, size, buffer) } -> std::same_as<bool>;
};

// Stores result of reading: address after reading, or null if read failed.
// Note error messages are propagated separately through the tracer.
template <IsMemoryReader MemoryReader>
using ReadCursor = std::optional<pointer_type_t<MemoryReader>>;

// Addition with overflow check, handling mixed types.
template <IsInteger S, IsInteger T>
[[nodiscard]] constexpr std::optional<S> checked_add(S s, T t) noexcept {
  // safely cast t to S
  if (!std::in_range<S>(t)) {
    return {};
  }
  S tt = static_cast<S>(t);
  // safely check min <= s + tt <= max
  using Limits = std::numeric_limits<S>;
  constexpr S min = Limits::min();
  constexpr S max = Limits::max();
  if (tt > 0) {
    if (s > max - tt) {
      return {};
    }
  } else if (tt < 0) {
    if (s < min - tt) {
      return {};
    }
  }
  return static_cast<S>(s + tt);
}

// checked_add but traced (for pointer arithmetic)
template <IsInteger S, IsInteger T, IsTracer Tracer>
[[nodiscard]] std::optional<S> safe_offset(S s, T t, Tracer& tracer) {
  auto u = checked_add(s, t);
  if (!u) tracer.error("pointer overflow");
  return u;
}

// Forward declaration to support recursive reading.
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer
);

namespace detail {

// ============================================================
// Helpers
// ============================================================

// thin wrapper around reader with tracing
template <IsMemoryReader MemoryReader, IsTracer Tracer, typename T>
[[nodiscard]] bool read_bytes(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  if (!reader(address, sizeof(target), &target)) {
    tracer.error("memory read failed");
    return false;
  }
  return true;
}

template <
  IsInteger auto N,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Pad<N> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_lazy_scope(tracer, address, [&] {
    return std::format("pad(0x{:X})", N);
  });
  return safe_offset(address, N, tracer);
}

template <
  IsInteger auto N,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Seek<N> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T&,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_lazy_scope(tracer, address, [&] {
    return std::format("offset(0x{:X})", N);
  });
  return safe_offset(base, N, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsReadable<member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Field<M> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  auto& field = target.*M;
  return read(reader, address, field, tracer);
}

template <auto M, IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer>
  requires IsTypeRepresentableAs<pointer_type_t<MemoryReader>, member_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Ptr<M> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryReader> target_ptr{};
  if (!read_bytes(reader, address, target_ptr, tracer)) return {};
  auto& field = target.*M;
  // static_cast safe by requires IsTypeRepresentableAs
  field = static_cast<member_type_t<M>>(target_ptr);
  return safe_offset(address, sizeof(target_ptr), tracer);
}

template <
  auto M,
  bool Required,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
  requires IsReadable<optional_value_type_t<M>>
[[nodiscard]] ReadCursor<MemoryReader> read_layout_item(
  Ref<M, Required> item,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  pointer_type_t<MemoryReader> address,
  T& target,
  Tracer& tracer
) {
  [[maybe_unused]] auto scope = make_scope(tracer, address, member_name<M>());
  pointer_type_t<MemoryReader> target_ptr{};
  if (!read_bytes(reader, address, target_ptr, tracer)) return {};
  auto& field = target.*M;
  field.reset();
  if (target_ptr) {
    using U = optional_value_type_t<M>;  // std::optional<U> -> U
    U value{};
    if (read(reader, target_ptr, value, tracer)) {
      field = std::move(value);
    }
  } else if constexpr (Required) {
    tracer.error("null pointer");
  }
  return safe_offset(address, sizeof(target_ptr), tracer);
}

template <
  IsLayoutItem... Items,
  IsMemoryReader MemoryReader,
  IsReadable T,
  IsTracer Tracer>
[[nodiscard]] ReadCursor<MemoryReader> read_layout(
  Layout<Items...>,
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer
) {
  ReadCursor<MemoryReader> result{base};
  // - ((expr), ...) is intentional: comma has the lowest precedence
  // - comma operator sequences the effects from left to right
  // - && short-circuits the evaluation so we stop on first failure
  // - Items{} is just a tag to select the overload, construction costs nothing
  ((
     result
     && (result = read_layout_item(Items{}, reader, base, result.value(), target, tracer))
   ),
   ...);
  return result;
}

}  // namespace detail

// ============================================================
// Public API: read
// ============================================================

/**
 * @brief Reads data from remote memory into a native object based on a
 * specified layout.
 *
 * @tparam MemoryReader The type for the reader callback.
 * @tparam T          The native type to deserialize into.
 * @param memory The memory abstraction providing the `MemoryReader` function.
 * @param base   The remote address to start reading from.
 * @param target The native object to populate.
 * @return Updated remote pointer after reading, as returned by `MemoryReader`.
 */
template <IsMemoryReader MemoryReader, IsReadable T, IsTracer Tracer = NoTracer>
[[nodiscard]] ReadCursor<MemoryReader> read(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  T& target,
  Tracer& tracer = detail::default_tracer<Tracer>()
) {
  if constexpr (HasLayout<T>) {
    return detail::read_layout(layout_of_t<T>{}, reader, base, target, tracer);
  } else {
    if (!detail::read_bytes(reader, base, target, tracer)) return {};
    return safe_offset(base, sizeof(target), tracer);
  }
}

// convenience wrapper for read so no local object needs to be created
template <IsReadable T, IsMemoryReader MemoryReader, IsTracer Tracer = NoTracer>
std::optional<T> read_at(
  const MemoryReader& reader,
  pointer_type_t<MemoryReader> base,
  Tracer& tracer = detail::default_tracer<Tracer>()
) {
  T target{};
  return read(reader, base, target, tracer) ? std::move(target)
                                            : std::optional<T>{};
}

}  // namespace mempeep

// ============================================================
// Example
// ============================================================

#ifndef _MSC_VER

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wliteral-suffix"
#pragma GCC diagnostic ignored "-Wdeprecated-literal-operator"

constexpr std::int32_t operator"" i32(unsigned long long v) {
  return v <= std::numeric_limits<std::int32_t>::max()
           ? static_cast<std::int32_t>(v)
           : throw "i32 literal out of range";
}

constexpr std::int16_t operator"" i16(unsigned long long v) {
  return v <= std::numeric_limits<std::int16_t>::max()
           ? static_cast<std::int16_t>(v)
           : throw "i16 literal out of range";
}

#pragma GCC diagnostic pop

#endif

// example with 16 bit pointers, for fun
template <int16_t BASE, int16_t N>
  requires(BASE > 0 && N > 0)
struct MockMemoryReader {
  using pointer_type = int16_t;
  std::byte data[N]{};

  bool operator()(int16_t address, std::size_t size, void* buffer) const {
    // handle overflow/underflow
    // note: static_cast is safe, size <= N means it fits
    if (!(buffer && size > 0 && size <= N && address >= BASE
          && address - BASE <= N - static_cast<int16_t>(size))) {
      return false;
    }
    std::memcpy(buffer, data + (address - BASE), size);
    return true;
  }

  template <typename T>
  void write(int16_t offset, T value) {
    assert(
      sizeof(value) <= N && offset >= BASE && offset - BASE <= N - sizeof(value)
    );
    std::memcpy(data + (offset - BASE), &value, sizeof(value));
  }
};

struct Pos {
  int32_t x, y;
};

struct Player {
  int32_t health;
  Pos pos;
  int16_t target_ptr;
  int32_t shop_ptr;  // wider than needed, still fine
  int16_t weapon_ptr;
  std::optional<Pos> prev_pos;
  std::optional<Pos> tagged_pos;
  std::optional<Pos> house_pos;
  int32_t mana;
};

struct Game {
  Player player;
};

using namespace mempeep;

// intentionally have 4 padding bytes at end, for testing
auto layout_of(LayoutOf<Pos>)
  -> Layout<Field<&Pos::x>, Pad<4i16>, Field<&Pos::y>, Pad<4i16>>;

auto layout_of(LayoutOf<Player>) -> Layout<
  Seek<8>,
  Field<&Player::health>,
  Seek<16>,
  Field<&Player::pos>,
  Ptr<&Player::target_ptr>,
  Ptr<&Player::shop_ptr>,
  Ptr<&Player::weapon_ptr>,
  Ref<&Player::prev_pos>,
  NullableRef<&Player::tagged_pos>,
  NullableRef<&Player::house_pos>,
  Field<&Player::mana>>;

auto layout_of(LayoutOf<Game>) -> Layout<Seek<6>, Field<&Game::player>>;

int main() {
  MockMemoryReader<1, 128> reader{};
  reader.write(18, 123i32);  // health
  reader.write(26, 11i32);   // pos.x
  reader.write(34, 22i32);   // pos.y
  reader.write(42, 0i16);    // target_ptr (optional)
  reader.write(44, 2i16);    // shop_ptr (optional)
  reader.write(46, 6i16);    // weapon_ptr
  reader.write(48, 60i16);   // prev_pos ref
  reader.write(50, 80i16);   // tagged_pos ref (optional)
  reader.write(52, 0i32);    // house_pos ref (optional)
  reader.write(54, 47i32);   // mana
  reader.write(60, 88i32);   // prev_pos.x
  reader.write(68, 99i32);   // prev_pos.y
  reader.write(80, 55i32);   // tagged_pos.x
  reader.write(88, 66i32);   // tagged_pos.y

  PrintTracer tracer{};
  auto game = mempeep::read_at<Game>(reader, 4i16, tracer);
  assert(game);
  assert(game->player.health == 123);
  assert(game->player.pos.x == 11);
  assert(game->player.pos.y == 22);
  assert(game->player.target_ptr == 0);
  assert(game->player.shop_ptr == 2);
  assert(game->player.weapon_ptr == 6);
  assert(game->player.prev_pos.has_value());
  assert(game->player.prev_pos->x == 88);
  assert(game->player.prev_pos->y == 99);
  assert(game->player.mana == 47);
  assert(game->player.tagged_pos.has_value());
  assert(game->player.tagged_pos->x == 55);
  assert(game->player.tagged_pos->y == 66);
  assert(!game->player.house_pos.has_value());
}
