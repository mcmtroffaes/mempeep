--- Computes sizes of types and field offsets of structs.
-- @module mempeep.schema

--- Creates a schema instance bound to a specific pointer size and struct array.
-- Offsets are resolved eagerly at construction time.
-- Structs must be declared before any struct that embeds them inline;
-- pointer-typed fields are fine in any order.
-- @param pointer_size Byte size of a pointer in the target process (4 or 8).
-- @param structs Array of structs produced by D.struct(...).
-- @return Table with sizeof, fields, and pointer_size.
local function new(pointer_size, structs)
  local struct_index = {}
  for i, s in ipairs(structs) do
    struct_index[s.name] = i
  end

  local resolved_fields = {}
  local resolved_sizes = {}

  local function sizeof(typ)
    local kind = typ.kind
    if kind == "primitive" then
      return typ.size
    elseif kind == "ptr" then
      return pointer_size
    elseif kind == "circular_list" then
      return pointer_size
    elseif kind == "vector" then
      return pointer_size * 2
    elseif kind == "array" then
      return sizeof(typ.typ) * typ.count
    elseif kind == "struct" then
      local i = assert(struct_index[typ.name], "schema.sizeof: unknown struct '" .. typ.name .. "'")
      local size = assert(
        resolved_sizes[i],
        "schema.sizeof: struct '" .. typ.name .. "' has not been resolved yet (declare it earlier in the array)"
      )
      return size
    else
      error("schema.sizeof: unknown type kind '" .. tostring(kind) .. "'")
    end
  end

  for i, s in ipairs(structs) do
    local field_list = {}
    local cursor = 0
    for _, desc in ipairs(s.descriptors) do
      if desc.kind == "pad" then
        cursor = cursor + desc.n
      elseif desc.kind == "offset" then
        if desc.n <= cursor then
          error(
            string.format(
              "schema: D.offset(%d) in struct '%s' would not move cursor forwards (currently at %d)",
              desc.n,
              s.name,
              cursor
            )
          )
        end
        cursor = desc.n
      elseif desc.kind == "field" then
        field_list[#field_list + 1] = {
          name = desc.name,
          typ = desc.typ,
          offset = cursor,
          opts = desc.opts,
        }
        cursor = cursor + sizeof(desc.typ)
      else
        error("schema: unknown descriptor kind '" .. tostring(desc.kind) .. "' in struct '" .. s.name .. "'")
      end
    end
    resolved_fields[i] = field_list
    resolved_sizes[i] = cursor
  end

  --- Returns an array of {name, typ, offset, opts} for each field in the named struct.
  -- Pad and offset descriptors are excluded.
  -- offset is the byte offset from the start of the struct to the start of the field.
  -- @param struct_name Name of the struct.
  -- @return Array of field tables.
  local function fields(struct_name)
    local i = struct_index[struct_name]
    if not i then
      error("schema.fields: unknown struct '" .. struct_name .. "'")
    end
    return resolved_fields[i]
  end

  local schema = {}

  schema.pointer_size = pointer_size
  schema.sizeof = sizeof
  schema.fields = fields

  return schema
end

local schema = {}

schema.new = new

return schema
