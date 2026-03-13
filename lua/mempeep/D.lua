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

local function assert_valid_circular_list_next(sub_struct, sub_name)
    for _, sub_desc in ipairs(sub_struct.descriptors) do
        if sub_desc.kind == "field" and sub_desc.name == "next" then
            if sub_desc.type_ref.kind ~= "ptr" or not sub_desc.type_ref.weak then
                error(
                    "D.validate: circular_list 'next' field " ..
                    "in struct '" .. sub_name .. "' must be a ptr, got: " ..
                    sub_desc.type_ref.kind
                )
            end
            local next_type = sub_desc.type_ref.type_ref
            if next_type.kind ~= "struct" or next_type.name ~= sub_name then
                error(
                    "D.validate: circular_list 'next' field " ..
                    "must be a ptr to struct '" .. sub_name .. "'"
                )
            end
            return  -- found and valid
        end
    end
    error(
        "D.validate: circular_list 'next' field " ..
        "not found in struct '" .. sub_name .. "'"
    )
end

--- Validate an array of struct descriptors.
-- Checks that all type_refs are well-formed and that all cross-references
-- between structs are satisfied.  Operates on the raw descriptor arrays,
-- before any sizes or offsets are computed.
-- @param structs Array of structs (each element built with D.struct(...)).
D.validate = function(structs)
    -- Build a name->struct lookup for cross-reference checks.
    local struct_by_name = {}
    for _, s in ipairs(structs) do
        struct_by_name[s.name] = s
    end

    -- Cross-reference checks now that all struct names are known.
    for _, s in ipairs(structs) do
        for _, desc in ipairs(assert(s.descriptors, "D.validate: struct is missing descriptors")) do
            if desc.kind == "field" then
                local tr = assert(desc.type_ref, "D.validate: descriptor is missing type_ref")
                T.validate(tr)
                if tr.kind == "struct" then
                    assert(
                        struct_by_name[tr.name],
                        "D.validate: struct '" .. s.name ..
                        "' field '" .. desc.name ..
                        "' references unknown struct '" .. tr.name .. "'"
                    )
                elseif tr.kind == "circular_list" then
                    local sub_name = tr.type_ref.name
                    local sub_struct = assert(
                        struct_by_name[sub_name],
                        "D.validate: circular_list references unknown struct '" .. sub_name .. "'"
                    )
                    assert_valid_circular_list_next(sub_struct, sub_name)
                end
            end
        end
    end
end

return D
