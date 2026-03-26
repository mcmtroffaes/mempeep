#pragma once

#include <mempeep/error.hpp>

namespace mempeep {

/**
 * @brief Minimal IsTracer implementation that reports whether any error
 * occurred. Suitable when error details are not needed.
 */
struct ErrorTracer {
  bool ok = true;

  void error(Error) { ok = false; }

  bool success() const { return ok; }
};

}  // namespace mempeep