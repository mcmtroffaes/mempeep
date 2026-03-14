--- Descriptor constructors for describing struct memory layouts.
-- Structs are described as an ordered array so that
-- we can resolve offsets and sizes in declaration order. Example:
--   local structs = {
--       D.struct("Foo", { D.field("x", T.i32), D.pad(4), ... }),
--       D.struct("Bar", { D.field("foo", T.struct("Foo")), ... }),
--   }
-- @module mempeep.D

local T = require("mempeep.T")

local D = {}

D.pad = function(n)
  return { kind = "pad", n = n }
end

D.offset = function(n)
  return { kind = "offset", n = n }
end

D.field = function(name, typ, opts)
  return { kind = "field", name = name, typ = typ, opts = opts }
end

D.struct = function(name, descriptors)
  return { name = name, descriptors = descriptors }
end

local function assert_valid_circular_list_typ(typ, struct_index)
  local next_field = typ.next_field
  local struct = assert(
    struct_index[typ.typ.name],
    "D.assert_valid: circular_list references unknown struct '" .. typ.typ.name .. "'"
  )
  for _, desc in ipairs(struct.descriptors) do
    if desc.kind == "field" and desc.name == next_field then
      if desc.typ.kind ~= "ptr" or not desc.typ.weak or desc.typ.optional then
        error(
          "D.assert_valid: circular_list '"
            .. next_field
            .. "' field "
            .. "in struct '"
            .. struct.name
            .. "' must be a non-optional weak ptr, got kind: "
            .. desc.typ.kind
            .. (
              desc.typ.kind == "ptr"
                and (", optional=" .. tostring(desc.typ.optional) .. ", weak=" .. tostring(desc.typ.weak))
              or ""
            )
        )
      end
      local next_typ = desc.typ.typ
      if next_typ.kind ~= "struct" or next_typ.name ~= struct.name then
        error(
          "D.assert_valid: circular_list '"
            .. next_field
            .. "' field "
            .. "must be a ptr to struct '"
            .. struct.name
            .. "'"
        )
      end
      return -- found and valid
    end
  end
  error("D.assert_valid: circular_list '" .. next_field .. "' field " .. "not found in struct '" .. struct.name .. "'")
end

--- Assert that an array of struct descriptors is valid.
-- Checks that all types are well-formed and that all cross-references
-- between structs are satisfied.  Operates on the raw descriptor arrays,
-- before any sizes or offsets are computed.
-- @param structs Array of structs (each element built with D.struct(...)).
-- @return structs if valid.
D.assert_valid = function(structs)
  local struct_index = {}
  for _, s in ipairs(structs) do
    struct_index[s.name] = s
  end
  for _, s in ipairs(structs) do
    for _, desc in ipairs(assert(s.descriptors, "D.assert_valid: struct is missing descriptors")) do
      if desc.kind == "field" then
        local typ = assert(desc.typ, "D.assert_valid: descriptor is missing typ")
        T.assert_valid(typ)
        if typ.kind == "struct" then
          assert(
            struct_index[typ.name],
            "D.assert_valid: struct '"
              .. s.name
              .. "' field '"
              .. desc.name
              .. "' references unknown struct '"
              .. typ.name
              .. "'"
          )
        elseif typ.kind == "circular_list" then
          assert_valid_circular_list_typ(typ, struct_index)
        end
      end
    end
  end
  return structs
end

return D
