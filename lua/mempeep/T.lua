--- Type constructors.
-- A "type" (typ) is a table describing the type of a value in memory.
-- Types are used as arguments to the memory layout descriptors.
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
T.array = function(element_typ, count)
  return { kind = "array", typ = element_typ, count = count }
end

--- Circular linked list.
-- element_typ must be a struct type.
-- next_field is the name of the weak-ptr field on the element struct
-- that points to the next element (defaults to "next").
T.circular_list = function(element_typ, next_field)
  return { kind = "circular_list", typ = element_typ, next_field = next_field or "next" }
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

--- Pointer.
-- opts.optional: if true, a null pointer is allowed (value will be nil, no error).
--               Defaults to false.
-- opts.weak:     if true, the pointer is not followed; value is the raw address integer.
--               if false, the pointer is followed; value is a reading of the pointee.
--               Defaults to false.
T.ptr = function(typ, opts)
  opts = opts or {}
  return {
    kind = "ptr",
    typ = typ,
    optional = opts.optional or false,
    weak = opts.weak or false,
  }
end

--- Null-terminated string.
T.string = function(size)
  return {
    kind = "primitive",
    name = "string",
    size = size,
    decode = function(bytes)
      -- "z" format gives error if no null in bytes, so add one
      return string.unpack("z", bytes .. "\0")
    end,
  }
end

--- Reference to a named struct.
T.struct = function(name)
  return { kind = "struct", name = name }
end

--- Begin/end pointer pair (std::vector-style).
T.vector = function(element_typ)
  return { kind = "vector", typ = element_typ }
end

--- Asserts that a type is valid, raising an error if it is malformed.
-- Recursively validates nested types.
-- Does not check cross-references between structs; that is deferred to
-- mempeep.D.assert_valid.
-- @param typ The type to validate.
-- @return typ if valid.
-- @raise if the type is malformed or has an unknown kind.
local function assert_valid(typ)
  assert(typ.kind, "type has no kind")
  if typ.kind == "array" then
    assert_valid(assert(typ.typ, "array has no typ"))
    assert(typ.count, "array has no count")
    assert(typ.count >= 1, "array count must be at least 1")
  elseif typ.kind == "circular_list" then
    assert_valid(assert(typ.typ, "circular_list has no typ"))
    assert(typ.typ.kind == "struct", "circular_list element must be a struct")
    assert(typ.next_field, "circular_list has no next_field")
  elseif typ.kind == "primitive" then
    assert(typ.name, "primitive has no name")
    assert(typ.size >= 1, "primitive size must be a positive integer")
    assert(type(typ.decode) == "function", "primitive has no decode function")
  elseif typ.kind == "ptr" then
    assert_valid(assert(typ.typ, "ptr has no typ"))
    assert(typ.optional ~= nil, "ptr has no optional flag")
    assert(typ.weak ~= nil, "ptr has no weak flag")
  elseif typ.kind == "struct" then
    assert(typ.name, "struct type has no name")
  elseif typ.kind == "vector" then
    assert_valid(assert(typ.typ, "vector has no typ"))
  else
    error("unknown type kind: " .. typ.kind)
  end
  return typ
end

T.assert_valid = assert_valid

return T
