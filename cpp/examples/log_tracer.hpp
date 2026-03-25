#pragma once

#include <format>
#include <mempeep/layout.hpp>
#include <mempeep/tracer.hpp>
#include <ostream>
#include <print>

template <auto M>
consteval std::string_view member_name() {
#if defined(_MSC_VER) && !defined(__clang__)
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
  return sig.substr(last_colon + 1, len);
}

template <mempeep::IsDescriptor Desc, auto M>
  requires std::is_member_object_pointer_v<decltype(M)>
std::string_view item_label(mempeep::Field_<Desc, M>) {
  return member_name<M>();
}

template <auto N>
std::string_view item_label(mempeep::Pad<N>) {
  return "(pad)";
}

template <auto N>
std::string_view item_label(mempeep::Seek<N>) {
  return "(seek)";
}

/** @brief Simple scoped tracer.
 *
 * Logs read operations throughout the layout.
 * Logs errors too.
 * Tracks if an error happened at any point.
 */
struct LogTracer {
  std::ostream& out;
  bool ok = true;
  int indent = 0;
  uint64_t address = 0;

  void log(std::string_view msg) {
    std::print(out, "[{:08X}] {: >{}}{}\n", address, "", indent, msg);
  }

  void error(mempeep::Error e) {
    ok = false;
    log(std::format("[ERROR {}]", static_cast<int>(e)));
  }

  template <typename T>
  void value(const T& val) {
    if constexpr (std::formattable<T, char>) {
      log(std::format("={}", val));
    } else {
      log("=???");
    }
  }

  bool success() const { return ok; }

  struct Scope {
    LogTracer& t;

    template <typename Item>
    Scope(LogTracer& t, uint64_t address, Item item) : t(t) {
      t.address = address;
      t.log(item_label(item));
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};