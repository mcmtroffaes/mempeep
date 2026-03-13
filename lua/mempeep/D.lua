--- Descriptor constructors for describing struct memory layouts.
-- Structs are described as an ordered array so that
-- we can resolve offsets and sizes in declaration order. Example:
--
--   local structs = {
--       D.struct("Foo", { D.field("x", T.i32()), D.pad(4), ... }),
--       D.struct("Bar", { D.field("foo", T.struct("Foo")), ... }),
--   }

local T = require("mempeep.T")

local D = {}

D.pad = function(n)
    return {kind = "pad", n = n}
end

D.offset = function(n)
    return {kind = "offset", n = n}
end

D.field = function(name, type_ref, opts)
    return {kind = "field", name = name, type_ref = type_ref, opts = opts}
end

D.struct = function(name, descriptors)
    return {name = name, descriptors = descriptors}
end

local function assert_valid_circular_list_type_ref(type_ref, struct_by_name)
    local struct = assert(
        struct_by_name[type_ref.name],
        "D.assert_valid: circular_list references unknown struct '" .. type_ref.name .. "'"
    )
    for _, desc in ipairs(struct.descriptors) do
        if desc.kind == "field" and desc.name == "next" then
            if desc.type_ref.kind ~= "ptr"
                or not desc.type_ref.weak
                or desc.type_ref.optional then
                error(
                    "D.assert_valid: circular_list 'next' field " ..
                    "in struct '" .. struct.name .. "' must be a ptr, got: " ..
                    desc.type_ref.kind
                )
            end
            local next_type = desc.type_ref.type_ref
            if next_type.kind ~= "struct" or next_type.name ~= struct.name then
                error(
                    "D.assert_valid: circular_list 'next' field " ..
                    "must be a ptr to struct '" .. struct.name .. "'"
                )
            end
            return  -- found and valid
        end
    end
    error(
        "D.assert_valid: circular_list 'next' field " ..
        "not found in struct '" .. struct.name .. "'"
    )
end

--- Assert that an array of struct descriptors is valid.
-- Checks that all type_refs are well-formed and that all cross-references
-- between structs are satisfied.  Operates on the raw descriptor arrays,
-- before any sizes or offsets are computed.
-- @param structs Array of structs (each element built with D.struct(...)).
-- @return structs if valid.
D.assert_valid = function(structs)
    local struct_by_name = {}
    for _, s in ipairs(structs) do
        struct_by_name[s.name] = s
    end
    for _, s in ipairs(structs) do
        for _, desc in ipairs(assert(s.descriptors, "D.assert_valid: struct is missing descriptors")) do
            if desc.kind == "field" then
                local tr = assert(desc.type_ref, "D.assert_valid: descriptor is missing type_ref")
                T.assert_valid(tr)
                if tr.kind == "struct" then
                    assert(
                        struct_by_name[tr.name],
                        "D.assert_valid: struct '" .. s.name ..
                        "' field '" .. desc.name ..
                        "' references unknown struct '" .. tr.name .. "'"
                    )
                elseif tr.kind == "circular_list" then
                    assert_valid_circular_list_type_ref(tr.type_ref, struct_by_name)
                end
            end
        end
    end
    return structs
end

return D
