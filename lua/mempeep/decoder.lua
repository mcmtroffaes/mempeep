--- Decode readings into plain Lua values.
-- Provides decoder.new(schema) -> decode, where
-- decode(typ, reading) converts a reading produced by mempeep.reader
-- into a plain Lua value, surfacing errors as a flat list of
-- context-prefixed strings.
-- Partial readings (both a value and an error present) are decoded;
-- the error is surfaced alongside whatever value could be recovered.
-- @module mempeep.decoder

local function new(schema)
  local decode -- forward declaration for mutual recursion

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

  local decoders = {}

  decoders.array = function(typ, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = decode(typ.typ, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  decoders.circular_list = function(typ, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = decode(typ.typ, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  decoders.primitive = function(typ, value)
    return value, {}
  end

  decoders.ptr = function(typ, value)
    if typ.weak then
      -- Weak pointer: value is a raw address integer; pass through as-is.
      return value, {}
    end
    -- Followed pointer: value is a nested reading for the pointee.
    return decode(typ.typ, value)
  end

  decoders.struct = function(typ, value)
    local field_descs = schema.fields(typ.name)
    local result = {}
    local errors = {}
    for _, fd in ipairs(field_descs) do
      local v, errs = decode(fd.typ, value[fd.name])
      result[fd.name] = v
      merge_errors(errors, prefix_errors(errs, fd.name))
    end
    return result, errors
  end

  decoders.vector = function(typ, value)
    local result = {}
    local errors = {}
    for i, elem in ipairs(value) do
      local v, errs = decode(typ.typ, elem)
      result[i] = v
      merge_errors(errors, prefix_errors(errs, "[" .. (i - 1) .. "]"))
    end
    return result, errors
  end

  --- Decode a reading.
  -- @param typ The type of the reading (same as what was passed to the reader).
  -- @param reading The {addr, value, error} table produced by mempeep.reader.
  -- @return The decoded plain Lua value (or nil if no value is available),
  --         and a flat list of context-prefixed error strings.
  decode = function(typ, reading)
    local u = assert(decoders[typ.kind], "decoder: unknown type kind: " .. typ.kind)

    if reading == nil or reading.value == nil then
      local errors = reading and reading.error and { reading.error } or {}
      return nil, errors
    end

    -- Honour partial readings: decode whatever value is present, then
    -- prepend any error from the reading itself before returning.
    local value, errors = u(typ, reading.value)

    if reading.error then
      table.insert(errors, 1, reading.error)
    end

    return value, errors
  end

  return decode
end

local decoder = {}

decoder.new = new

return decoder
