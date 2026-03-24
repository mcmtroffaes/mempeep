#pragma once

namespace mempeep {

enum class Error {
  READ_FAILED,
  ADDRESS_OVERFLOW,
  ADDRESS_NULL,
  VECTOR_INVALID_RANGE,
  VECTOR_MISALIGNED,
};

}