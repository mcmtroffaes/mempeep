--- Computes sizes of type_refs and field offsets of structs.
-- @module mempeep.compute

--- Creates a compute instance bound to a specific pointer size and struct array.
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

    local resolved_offsets = {}

    --- Returns the byte size of any type_ref.
    -- @param type_ref The type reference to size.
    -- @return Byte size as a number.
    local function sizeof(type_ref)
        local kind = type_ref.kind
        if     kind == "i8"            then return 1
        elseif kind == "i16"           then return 2
        elseif kind == "i32"           then return 4
        elseif kind == "float"         then return 4
        elseif kind == "i64"           then return 8
        elseif kind == "string"        then return type_ref.max_length
        elseif kind == "ptr"           then return pointer_size
        elseif kind == "circular_list" then return pointer_size
        elseif kind == "vector"        then return pointer_size * 2
        elseif kind == "array"         then
            return sizeof(type_ref.type_ref) * type_ref.count
        elseif kind == "struct" then
            local i = struct_index[type_ref.name]
            if not i then
                error("compute.sizeof: unknown struct '" .. type_ref.name .. "'")
            end
            local offs = resolved_offsets[i]
            if not offs then
                error("compute.sizeof: struct '" .. type_ref.name ..
                      "' has not been resolved yet (declare it earlier in the array)")
            end
            return offs[#offs]
        else
            error("compute.sizeof: unknown type kind '" .. tostring(kind) .. "'")
        end
    end

    for i, s in ipairs(structs) do
        local cursor       = 0
        local desc_offsets = {}
        for _, desc in ipairs(s.descriptors) do
            if desc.kind == "pad" then
                cursor = cursor + desc.n
            elseif desc.kind == "offset" then
                if desc.n < cursor then
                    error(string.format(
                        "compute: D.offset(%d) in struct '%s' would move " ..
                        "cursor backwards (currently at %d)", desc.n, s.name, cursor))
                end
                cursor = desc.n
            elseif desc.kind == "field" then
                cursor = cursor + sizeof(desc.type_ref)
            else
                error("compute: unknown descriptor kind '" ..
                      tostring(desc.kind) .. "' in struct '" .. s.name .. "'")
            end
            desc_offsets[#desc_offsets + 1] = cursor
        end
        resolved_offsets[i] = desc_offsets
    end

    --- Returns an array of {name, type_ref, offset} for each field in the named struct.
    -- Pad and offset descriptors are excluded.
    -- offset is the byte offset from the start of the struct to the start of the field.
    -- @param struct_name Name of the struct.
    -- @return Array of field descriptor tables.
    local function fields(struct_name)
        local i = struct_index[struct_name]
        if not i then
            error("compute.fields: unknown struct '" .. struct_name .. "'")
        end
        local s         = structs[i]
        local s_offsets = resolved_offsets[i]
        local result    = {}
        for j, desc in ipairs(s.descriptors) do
            if desc.kind == "field" then
                result[#result + 1] = {
                    name     = desc.name,
                    type_ref = desc.type_ref,
                    offset   = s_offsets[j - 1] or 0,
                }
            end
        end
        return result
    end

    local compute = {}

    compute.pointer_size = pointer_size
    compute.sizeof       = sizeof
    compute.fields       = fields

    return compute
end

local compute = {}

compute.new = new

return compute
