--- Read values from memory.
-- Provides reader.new(compute, read_bytes) -> read, where read(addr, type_ref)
-- reads a typed value.
--
-- compute is an instance produced by mempeep.compute.new(pointer_size, structs).
--
-- read_bytes(addr, size) is a function that reads size bytes at addr, returning them as a string.
-- The returned string *must* have length size.
-- Returns nil if the memory is not readable.
--
-- read(addr, type_ref) returns {addr, value, error}.
-- error is a string if an error occurred, nil otherwise.
-- For containers, value is a table of {addr, value, error} elements.
-- @module mempeep.reader

local function new(compute, read_bytes)
  local ptr_size = compute.pointer_size

  local read -- forward declaration for mutual recursion

  -- Read a pointer-sized integer from addr. Returns nil on failure.
  local function read_pointer(addr)
    local s = read_bytes(addr, ptr_size)
    if s == nil then
      return nil
    end
    local fmt = "<i" .. ptr_size
    return string.unpack(fmt, s)
  end

  local readers = {

    array = function(addr, type_ref)
      local sub_type = type_ref.type_ref
      local size = compute.sizeof(sub_type)
      local items = {}
      local p = addr
      for i = 1, type_ref.count do
        table.insert(items, read(p, sub_type))
        p = p + size
      end
      return { addr = addr, value = items }
    end,

    circular_list = function(addr, type_ref)
      local sub_type = type_ref.type_ref
      local items = {}
      local head = read_pointer(addr)
      if head == nil then
        return { addr = addr, value = nil, error = "Invalid head pointer" }
      elseif head ~= 0 then
        local p = head
        repeat
          local rawval = read(p, sub_type)
          table.insert(items, rawval)
          local struct_value = rawval.value
          if struct_value then
            -- next is a weak ptr field; its raw value is the address integer
            local next_addr = struct_value["next"] and struct_value["next"].value
            if next_addr == nil then
              return { addr = addr, value = items, error = "Invalid next pointer" }
            end
            p = next_addr
          else
            return { addr = addr, value = items, error = "Could not read struct" }
          end
        until p == head
      end
      return { addr = addr, value = items }
    end,

    ptr = function(addr, type_ref)
      local p = read_pointer(addr)
      if p == nil then
        return { addr = addr, value = nil, error = "Could not read pointer" }
      end
      if p == 0 then
        if type_ref.optional then
          return { addr = addr, value = nil }
        end
        return { addr = addr, value = nil, error = "Null pointer" }
      end
      if type_ref.weak then
        return { addr = addr, value = p }
      end
      return { addr = addr, value = read(p, type_ref.type_ref) }
    end,

    primitive = function(addr, type_ref)
      local s = read_bytes(addr, type_ref.size)
      if s == nil then
        return { addr = addr, value = nil, error = "Could not read " .. type_ref.name }
      end
      local val = type_ref.decode(s)
      return { addr = addr, value = val }
    end,

    struct = function(addr, type_ref)
      local field_descs = compute.fields(type_ref.name)
      local obj = {}
      for _, fd in ipairs(field_descs) do
        obj[fd.name] = read(addr + fd.offset, fd.type_ref)
      end
      return { addr = addr, value = obj }
    end,

    vector = function(addr, type_ref)
      local begin_ptr = read_pointer(addr)
      local end_ptr = read_pointer(addr + ptr_size)
      local sub_type = type_ref.type_ref
      if begin_ptr == nil or end_ptr == nil then
        return { addr = addr, value = nil, error = "Invalid vector pointers" }
      end
      if begin_ptr > end_ptr then
        return { addr = addr, value = nil, error = "Vector begin pointer is past end pointer" }
      end
      local items = {}
      local p = begin_ptr
      local size = compute.sizeof(sub_type)
      while p < end_ptr do
        table.insert(items, read(p, sub_type))
        p = p + size
      end
      if p ~= end_ptr then
        table.insert(items, { addr = p, value = nil, error = "Vector pointer overflow" })
      end
      return { addr = addr, value = items }
    end,
  }

  read = function(addr, type_ref)
    if addr == nil then
      return { addr = nil, value = nil, error = "nil address" }
    end
    local r = assert(readers[type_ref.kind], "reader: unknown type kind: " .. type_ref.kind)
    return r(addr, type_ref)
  end

  return read
end

local reader = {}

reader.new = new

return reader
