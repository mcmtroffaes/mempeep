--- Type reference constructors.
-- A type reference, or "type_ref", is a table
-- describing the type of a value in memory.
-- They are used as arguments to the memory layout descriptors.
-- @module mempeep.T

local function make_float(size)
  assert(size == 4 or size == 8, "float must have size 4 or 8")
  local fmt = (size == 4) and "<f" or "<d"
  return {
    kind = "primitive",
    name = "f" .. (size * 8),
    size = size,
    decode = function(bytes)
      return string.unpack(fmt, bytes)
    end,
  }
end

local function make_int(size, signed)
  local fmt = (signed and "<i" or "<I") .. size
  return {
    kind = "primitive",
    name = (signed and "i" or "u") .. (size * 8),
    size = size,
    decode = function(bytes)
      return string.unpack(fmt, bytes)
    end,
  }
end

local T = {}

--- Fixed-size inline array.
T.array = function(element_type_ref, count)
  return { kind = "array", type_ref = element_type_ref, count = count }
end

--- Circular linked list (element struct must have a weak ptr field named "next").
T.circular_list = function(element_type_ref)
  return { kind = "circular_list", type_ref = element_type_ref }
end

-- Primitive types.
T.f32 = make_float(4)
T.f64 = make_float(8)
T.i8 = make_int(1, true)
T.i16 = make_int(2, true)
T.i32 = make_int(4, true)
T.i64 = make_int(8, true)
T.u8 = make_int(1, false)
T.u16 = make_int(2, false)
T.u32 = make_int(4, false)
T.u64 = make_int(8, false)

--- Nullable, non-followed pointer (raw address only).
T.optional_ptr = function(type_ref)
  return { kind = "ptr", type_ref = type_ref, optional = true, weak = true }
end

--- Nullable, followed pointer (pointee is read if non-null).
T.optional_ref = function(type_ref)
  return { kind = "ptr", type_ref = type_ref, optional = true, weak = false }
end

--- Non-null, non-followed pointer (raw address only).
T.ptr = function(type_ref)
  return { kind = "ptr", type_ref = type_ref, optional = false, weak = true }
end

--- Non-null, followed pointer (pointee is read; null raises an error).
T.ref = function(type_ref)
  return { kind = "ptr", type_ref = type_ref, optional = false, weak = false }
end

--- Null-terminated string.
T.string = function(size)
  return {
    kind = "primitive",
    name = "string",
    size = size,
    decode = function(bytes)
      return string.unpack("z", bytes)
    end,
  }
end

--- Reference to a named struct.
T.struct = function(name)
  return { kind = "struct", name = name }
end

--- Begin/end pointer pair (std::vector-style).
T.vector = function(element_type_ref)
  return { kind = "vector", type_ref = element_type_ref }
end

--- Asserts that a type_ref table is valid, raising an error if it is malformed.
-- Recursively validates nested type references.
-- Does not check cross-references between structs; that is deferred to
-- mempeep.D.assert_valid.
-- @param type_ref The type reference to validate.
-- @return type_ref if valid.
-- @raise if the type_ref is malformed or has an unknown kind.
local function assert_valid(type_ref)
  assert(type_ref.kind, "type_ref has no kind")
  if type_ref.kind == "array" then
    assert_valid(assert(type_ref.type_ref, "array has no type_ref"))
    assert(type_ref.count, "array has no count")
    assert(type_ref.count >= 1, "array count must be at least 1")
  elseif type_ref.kind == "circular_list" then
    assert_valid(assert(type_ref.type_ref, "circular_list has no type_ref"))
    assert(type_ref.type_ref.kind == "struct", "circular_list element must be a struct")
  elseif type_ref.kind == "primitive" then
    assert(type_ref.name, "primitive has no name")
    assert(type_ref.size >= 1, "primitive size must be a positive integer")
    assert(type(type_ref.decode) == "function", "primitive has no decode function")
  elseif type_ref.kind == "ptr" then
    assert_valid(assert(type_ref.type_ref, "ptr has no type_ref"))
    assert(type_ref.optional ~= nil, "ptr has no optional flag")
    assert(type_ref.weak ~= nil, "ptr has no weak flag")
  elseif type_ref.kind == "struct" then
    assert(type_ref.name, "struct type_ref has no name")
  elseif type_ref.kind == "vector" then
    assert_valid(assert(type_ref.type_ref, "vector has no type_ref"))
  else
    error("unknown type_ref kind: " .. type_ref.kind)
  end
  return type_ref
end

T.assert_valid = assert_valid

return T
