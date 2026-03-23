#pragma once

#include <mempeep/layout.hpp>
#include <mempeep/tracer.hpp>
#include <ostream>

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

template <auto M>
std::string_view item_label(mempeep::Field<M>) {
  return member_name<M>();
}

template <auto M>
std::string_view item_label(mempeep::RawAddr<M>) {
  return member_name<M>();
}

template <auto M>
std::string_view item_label(mempeep::Ref<M>) {
  return member_name<M>();
}

template <auto M>
std::string_view item_label(mempeep::NullableRef<M>) {
  return member_name<M>();
}

template <auto N>
std::string_view item_label(mempeep::Pad<N>) {
  return "pad";
}

template <auto N>
std::string_view item_label(mempeep::Seek<N>) {
  return "seek";
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
  std::string_view label{};

  void log(std::string_view msg) {
    const auto whitespace = std::string(indent, ' ');
    out << std::format("[{:08X}] ", address) << whitespace << msg << std::endl;
  }

  void error(mempeep::Error e) {
    ok = false;
    log(std::format("{} [ERROR {}]", label, static_cast<int>(e)));
  }

  template <typename T>
  void value(const T& val) {
    if constexpr (std::formattable<T, char>) {
      log(std::format("{}={}", label, val));
    } else {
      log(std::format("{}=???", label));
    }
  }

  bool success() const { return ok; }

  struct Scope {
    LogTracer& t;

    template <typename Item>
    Scope(LogTracer& _t, uint64_t address, Item item) : t(_t) {
      t.address = address;
      t.label = item_label(item);
      t.indent++;
    }

    ~Scope() { t.indent--; }
  };
};