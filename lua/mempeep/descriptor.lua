--- Descriptor constructors and validators.
-- Descriptors declare how values are read from remote memory.

local M = {}

-- Valid descriptor tags.
local descriptor_tags = {
  Primitive = true,
  RawAddr = true,
  Ref = true,
  NullableRef = true,
  Array = true,
  Vector = true,
  CircularList = true,
  Struct = true,
}

-- Valid fields item tags.
local fields_item_tags = {
  Field = true,
  Seek = true,
  Pad = true,
}

--------------------------------------------------------------------------------
-- Validators
--------------------------------------------------------------------------------

--- Assert that `v` is a table.
-- @param v value to check
-- @return v
function M.assert_table(v)
  assert(type(v) == "table", "expected a table, got " .. type(v))
  return v
end

--- Assert that `v` is a string.
-- @param v value to check
-- @return v
function M.assert_string(v)
  assert(type(v) == "string", "expected a string, got " .. type(v))
  return v
end

--- Assert that `v` is a valid format string.
-- @param v value to check
-- @return v
function M.assert_fmt(v)
  M.assert_string(v)
  local ok, size = pcall(string.packsize, v)
  assert(ok and size > 0, "invalid or non-fixed-size format: " .. v)
  return v
end

--- Assert that `v` is an unsigned integer format string ("I1" through "I16").
-- @param v value to check
-- @return v
function M.assert_uint_fmt(v)
  M.assert_fmt(v)
  local n = v:match("^I(%d+)$")
  assert(
    n and tonumber(n) >= 1 and tonumber(n) <= 16,
    "expected an unsigned integer format string (I1..I16), got " .. v
  )
  return v
end

--- Assert that `v` is a non-negative integer.
-- @param v value to check
-- @return v
function M.assert_count(v)
  assert(
    type(v) == "number" and math.type(v) == "integer" and v >= 0,
    "expected a non-negative integer, got " .. tostring(v)
  )
  return v
end

--- Assert that `v` is a descriptor table.
-- @param v value to check
-- @return v
function M.assert_descriptor(v)
  M.assert_table(v)
  assert(descriptor_tags[v.tag], "expected a descriptor, got " .. tostring(v.tag))
  return v
end

--- Assert that `v` is a fields item table.
-- @param v value to check
-- @return v
function M.assert_fields_item(v)
  M.assert_table(v)
  assert(fields_item_tags[v.tag], "expected a fields item (Field, Pad, or Seek), got " .. tostring(v.tag))
  return v
end

--- Assert that `v` is a fields array.
-- @param v value to check
-- @return v
function M.assert_fields(v)
  M.assert_table(v)
  local seen = {}
  for _, item in ipairs(v) do
    M.assert_fields_item(item)
    if item.tag == "Field" then
      assert(not seen[item.key], "duplicate field key: " .. item.key)
      seen[item.key] = true
    end
  end
  return v
end

--------------------------------------------------------------------------------
-- Descriptor constructors
--------------------------------------------------------------------------------

--- Read `string.packsize(fmt)` bytes directly into a native value.
-- @param fmt a string.unpack format string, e.g. `"i4"`, `"I1"`, `"f"`
-- @return Primitive descriptor
function M.Primitive(fmt)
  return { tag = "Primitive", fmt = M.assert_fmt(fmt) }
end

--- Read an address-sized integer without following it.
-- @return RawAddr descriptor
function M.RawAddr(fmt)
  return { tag = "RawAddr" }
end

--- Read an address and follow it, reading the pointee using `desc`.
-- A null (zero) address is an error.
-- @param desc descriptor for the pointee
-- @return Ref descriptor
function M.Ref(desc)
  return { tag = "Ref", desc = M.assert_descriptor(desc) }
end

--- Like Ref, but a null address is allowed and yields nil.
-- @param desc descriptor for the pointee
-- @return NullableRef descriptor
function M.NullableRef(desc)
  return { tag = "NullableRef", desc = M.assert_descriptor(desc) }
end

--- Read `n` consecutive elements using `desc`.
-- @param desc descriptor for each element
-- @param n number of elements (non-negative integer)
-- @return Array descriptor
function M.Array(desc, n)
  return { tag = "Array", desc = M.assert_descriptor(desc), n = M.assert_count(n) }
end

--- Read a begin/end address pair, then each element using `desc`.
-- @param desc descriptor for each element
-- @param max_len maximum number of elements before an error is raised
-- @return Vector descriptor
function M.Vector(desc, max_len)
  return { tag = "Vector", desc = M.assert_descriptor(desc), max_len = M.assert_count(max_len) }
end

--- Read a circular intrusive linked list.
-- Reads a head address; traverses nodes using `desc` until the head is
-- revisited. The `next_key` field of each decoded node holds the next address.
-- @param desc descriptor for each node (must be a Struct)
-- @param next_key string key in the decoded node table that holds the next address
-- @param max_len maximum number of nodes before an error is raised
-- @return CircularList descriptor
function M.CircularList(desc, next_key, max_len)
  M.assert_descriptor(desc)
  assert(desc.tag == "Struct", "expected a Struct")
  M.assert_string(next_key)
  local found = false
  for _, field in ipairs(desc.fields) do
    if field.tag == "Field" and field.key == next_key then    
      found = true
      assert(field.desc.tag == "RawAddr", "expected a RawAddr for '" .. next_key .. "' field")
      break
    end
  end
  assert(found, "next_key '" .. next_key .. "' not found in Struct")
  return {
    tag = "CircularList",
    desc = desc,
    next_key = next_key,
    max_len = M.assert_count(max_len),
  }
end

--- Read a struct by applying a sequence of field items in order.
-- @param ... Items produced by `Field()`, `Pad()`, or `Seek()`
-- @return Struct descriptor
function M.Struct(...)
  return { tag = "Struct", fields = M.assert_fields({ ... }) }
end

--------------------------------------------------------------------------------
-- Fields items
--------------------------------------------------------------------------------

--- Map a descriptor onto a key in the output table.
-- @param desc descriptor for the field
-- @param key string key to write into the output table
-- @return Field fields item
function M.Field(desc, key)
  return { tag = "Field", desc = M.assert_descriptor(desc), key = M.assert_string(key) }
end

--- Skip `n` bytes relative to the current position.
-- @param n number of bytes to skip (non-negative integer)
-- @return Pad fields item
function M.Pad(n)
  return { tag = "Pad", n = M.assert_count(n) }
end

--- Seek to an absolute byte offset from the struct base address.
-- @param n byte offset (non-negative integer)
-- @return Seek fields item
function M.Seek(n)
  return { tag = "Seek", n = M.assert_count(n) }
end

return M
