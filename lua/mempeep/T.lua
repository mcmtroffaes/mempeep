--- Type reference constructors.
-- A type reference, or "type_ref", is a table
-- describing the type of a value in memory.
-- They are used as arguments to the memory layout descriptors.
-- @module mempeep.T

local T = {}

T.array = function(element_type_ref, count)
    return {kind = "array", type_ref = element_type_ref, count = count}
end
T.circular_list = function(element_type_ref)
    return {kind = "circular_list", type_ref = element_type_ref}
end
T.float = {kind = "float"}
T.i8 = {kind = "i8"}
T.i16 = {kind = "i16"}
T.i32 = {kind = "i32"}
T.i64 = {kind = "i64"}
T.optional_ptr = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = true, weak = true}
end
T.optional_ref = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = true, weak = false}
end
T.ptr = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = false, weak = true}
end
T.ref = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = false, weak = false}
end
T.string = function(max_length)
    return {kind = "string", max_length = max_length}
end
T.struct = function(name)
    return {kind = "struct", name = name}
end
T.vector = function(element_type_ref)
    return {kind = "vector", type_ref = element_type_ref}
end

local function validate(type_ref)
    if not type_ref.kind then
        error("type_ref has no kind")
    end
    if type_ref.kind == "array" then
        assert(type_ref.type_ref, "array has no type_ref")
        assert(type_ref.count, "array has no count")
        assert(type_ref.count >= 1, "array count must be at least 1")
        validate(type_ref.type_ref)
    elseif type_ref.kind == "circular_list" then
        assert(type_ref.type_ref, "circular_list has no type_ref")
        validate(type_ref.type_ref)
    elseif type_ref.kind == "float" then
        assert(true)
    elseif type_ref.kind == "i8" then
        assert(true)
    elseif type_ref.kind == "i16" then
        assert(true)
    elseif type_ref.kind == "i32" then
        assert(true)
    elseif type_ref.kind == "i64" then
        assert(true)
    elseif type_ref.kind == "ptr" then
        assert(type_ref.type_ref, "ptr has no type_ref")
        validate(type_ref.type_ref)
    elseif type_ref.kind == "string" then
        assert(type_ref.max_length, "string has no max_length")
        assert(type_ref.max_length >= 1, "string max_length must be at least 1")
    elseif type_ref.kind == "struct" then
        assert(type_ref.name, "struct type_ref has no name")
    elseif type_ref.kind == "vector" then
        assert(type_ref.type_ref, "vector has no type_ref")
        validate(type_ref.type_ref)
    else
        error("unknown type_ref kind: " .. type_ref.kind)
    end
end

T.validate = validate

return T

