--- Unwrap raw values into plain values.
-- Provides unwrapper.new(structs) -> unwrap, where
-- unwrap(type_ref, rawval) converts a raw value tree produced by
-- mempeep.reader into plain Lua values, surfacing errors as a flat list of
-- context-prefixed strings.
-- @module mempeep.unwrapper

local function new(structs)
  local struct_by_name = {}
  for _, s in ipairs(structs) do
    struct_by_name[s.name] = s
  end

  local unwrap -- forward declaration for mutual recursion

  local function prefix_errors(errors, context)
    local result = {}
    for _, err in ipairs(errors) do
      result[#result + 1] = context .. ": " .. err
    end
    return result
  end

  local function merge_errors(dest, src)
    for _, err in ipairs(src) do
      dest[#dest + 1] = err
    end
  end

  local unwrappers = {}

  unwrappers.array = function(type_ref, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = unwrap(type_ref.type_ref, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  unwrappers.circular_list = function(type_ref, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = unwrap(type_ref.type_ref, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  unwrappers.float = function(type_ref, value)
    return value, {}
  end
  unwrappers.i8 = function(type_ref, value)
    return value, {}
  end
  unwrappers.i16 = function(type_ref, value)
    return value, {}
  end
  unwrappers.i32 = function(type_ref, value)
    return value, {}
  end
  unwrappers.i64 = function(type_ref, value)
    return value, {}
  end
  unwrappers.string = function(type_ref, value)
    return value, {}
  end

  unwrappers.ptr = function(type_ref, value)
    if type_ref.weak then
      -- Weak pointer: value is a raw address integer; pass through as-is.
      return value, {}
    end
    -- Followed pointer: value is a nested rawval for the pointee.
    return unwrap(type_ref.type_ref, value)
  end

  unwrappers.struct = function(type_ref, value)
    local s = struct_by_name[type_ref.name]
    if not s then
      error("unwrapper: unknown struct '" .. type_ref.name .. "'")
    end
    local result = {}
    local errors = {}
    for _, desc in ipairs(s.descriptors) do
      if desc.kind == "field" then
        local v, errs = unwrap(desc.type_ref, value[desc.name])
        result[desc.name] = v
        merge_errors(errors, prefix_errors(errs, desc.name))
      end
    end
    return result, errors
  end

  unwrappers.vector = function(type_ref, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = unwrap(type_ref.type_ref, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  --- Unwrap a value.
  -- @param type_ref The type of the value (same as what was passed to the reader).
  -- @param rawval The {addr, value, error} as produced by mempeep.reader.
  -- @return The unwrapped plain Lua value (or nil on failure) and a flat list of context-prefixed error strings.
  unwrap = function(type_ref, rawval)
    if rawval == nil or rawval.value == nil then
      local errors = rawval and rawval.error and { rawval.error } or {}
      return nil, errors
    end
    local u = unwrappers[type_ref.kind]
    if not u then
      error("unwrapper: unknown type kind: " .. type_ref.kind)
    end
    return u(type_ref, rawval.value)
  end

  return unwrap
end

local unwrapper = {}

unwrapper.new = new

return unwrapper
