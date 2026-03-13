--- Type reference constructors.
-- A type reference, or "type_ref", is a table
-- describing the type of a value in memory.
-- They are used as arguments to the memory layout descriptors.
-- @module mempeep.T
local T = {}

--- Fixed-size inline array.
T.array = function(element_type_ref, count)
    return {kind = "array", type_ref = element_type_ref, count = count}
end

--- Circular linked list (element struct must have a weak ptr field named "next").
T.circular_list = function(element_type_ref)
    return {kind = "circular_list", type_ref = element_type_ref}
end

--- 32-bit float.
T.float = function() return {kind = "float"} end

--- Signed 8-bit integer.
T.i8 = function() return {kind = "i8"} end

--- Signed 16-bit integer.
T.i16 = function() return {kind = "i16"} end

--- Signed 32-bit integer.
T.i32 = function() return {kind = "i32"} end

--- Signed 64-bit integer.
T.i64 = function() return {kind = "i64"} end

--- Nullable, non-followed pointer (raw address only).
T.optional_ptr = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = true, weak = true}
end

--- Nullable, followed pointer (pointee is read if non-null).
T.optional_ref = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = true, weak = false}
end

--- Non-null, non-followed pointer (raw address only).
T.ptr = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = false, weak = true}
end

--- Non-null, followed pointer (pointee is read; null raises an error).
T.ref = function(type_ref)
    return {kind = "ptr", type_ref = type_ref, optional = false, weak = false}
end

--- Fixed-length string.
T.string = function(max_length)
    return {kind = "string", max_length = max_length}
end

--- Reference to a named struct.
T.struct = function(name)
    return {kind = "struct", name = name}
end

--- Begin/end pointer pair (std::vector-style).
T.vector = function(element_type_ref)
    return {kind = "vector", type_ref = element_type_ref}
end

--- Asserts that a type_ref table is valid, raising an error if it is malformed.
-- Recursively validates nested type references.
-- Does not check cross-references between structs; that is deferred to
-- mempeep.D.assert_valid.
-- @param type_ref The type reference to validate.
-- @return type_ref if valid.
-- @raise if the type_ref is malformed or has an unknown kind.
local function assert_valid(type_ref)
    if not type_ref.kind then
        error("type_ref has no kind")
    end
    if type_ref.kind == "array" then
        assert_valid(assert(type_ref.type_ref, "array has no type_ref"))
        assert(type_ref.count, "array has no count")
        assert(type_ref.count >= 1, "array count must be at least 1")
    elseif type_ref.kind == "circular_list" then
        assert_valid(assert(type_ref.type_ref, "circular_list has no type_ref"))
        assert(type_ref.type_ref.kind == "struct", "circular_list element must be a struct")
    elseif type_ref.kind == "float"
        or type_ref.kind == "i8"
        or type_ref.kind == "i16"
        or type_ref.kind == "i32"
        or type_ref.kind == "i64" then
        -- no additional validation needed
    elseif type_ref.kind == "ptr" then
        assert_valid(assert(type_ref.type_ref, "ptr has no type_ref"))
    elseif type_ref.kind == "string" then
        assert(type_ref.max_length, "string has no max_length")
        assert(type_ref.max_length >= 1, "string max_length must be at least 1")
    elseif type_ref.kind == "struct" then
        assert(type_ref.name, "struct type_ref has no name")
    elseif type_ref.kind == "vector" then
        assert_valid(assert(type_ref.type_ref, "vector has no type_ref"))
    else
        error("unknown type_ref kind: " .. type_ref.kind)
    end
    return type_ref
end

T.assert_valid = assert_valid

return T
