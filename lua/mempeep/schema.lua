--- Computes sizes of types and field offsets of structs.
-- @module mempeep.schema

--- Creates a schema instance bound to a specific pointer size and array of struct descriptors.
-- Offsets are resolved eagerly at construction time.
-- Structs must be declared before any struct that embeds them inline;
-- pointer-typed fields are fine in any order.
-- @param pointer_size Byte size of a pointer (4 or 8).
-- @param structs Array of structs produced by D.struct(...).
-- @return Table with dump, fields, pointer_size, and sizeof.
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
        assert(desc.n > 0, string.format("schema: D.pad(%d) in struct '%s' must be strictly positive", desc.n, s.name))
        cursor = cursor + desc.n
      elseif desc.kind == "offset" then
        assert(
          desc.n > cursor,
          string.format(
            "schema: D.offset(%d) in struct '%s' would not move cursor forwards (currently at %d)",
            desc.n,
            s.name,
            cursor
          )
        )
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

  --- Print all structs in C++ style syntax.
  local function dump()
    local function get_typ_name(typ)
      local dispatch = {
        array = function(typ)
          return "array<" .. get_typ_name(typ.typ) .. ", " .. typ.count .. ">"
        end,
        circular_list = function(typ)
          return "circular_list<" .. get_typ_name(typ.typ) .. ">"
        end,
        ptr = function(typ)
          return get_typ_name(typ.typ) .. "*"
        end,
        primitive = function(typ)
          return typ.name ~= "string" and typ.name or "char[" .. typ.size .. "]"
        end,
        struct = function(typ)
          return typ.name
        end,
        vector = function(typ)
          return "vector<" .. get_typ_name(typ.typ) .. ">"
        end,
      }
      return dispatch[typ.kind](typ)
    end

    local function dump_field(field_info)
      print(string.format("  %-24s %-24s  // offset 0x%02X", field_info.typ, field_info.name .. ";", field_info.offset))
    end

    local function dump_struct(struct_name, size, field_infos)
      print(string.format("struct %s {", struct_name))
      print(string.format("  // size 0x%02X", size))
      for _, field_info in ipairs(field_infos) do
        dump_field(field_info)
      end
      print("};")
    end

    print("static_assert(sizeof(void*) == " .. pointer_size .. ")")
    print("template <typename T>")
    dump_struct("vector", 8, {
      { typ = "T*", name = "begin", offset = 0 },
      { typ = "T*", name = "end", offset = 4 },
    })
    for _, struct in ipairs(structs) do
      -- everything should be resolved by now, use assert for extra safety
      local size = assert(resolved_sizes[assert(struct_index[struct.name])])
      local field_infos = {}
      for i, field in ipairs(fields(struct.name)) do
        field_infos[i] = { typ = get_typ_name(field.typ), name = field.name, offset = field.offset }
      end
      dump_struct(struct.name, size, field_infos)
    end
  end

  local schema = {}

  schema.pointer_size = pointer_size
  schema.sizeof = sizeof
  schema.fields = fields
  schema.dump = dump

  return schema
end

local schema = {}

schema.new = new

return schema
