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

return T

