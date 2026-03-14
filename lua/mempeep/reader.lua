--- Read values from memory.
-- Provides reader.new(schema, memory) -> read, where read(addr, typ)
-- reads a typed value.
--
-- schema is an instance produced by mempeep.schema.new(pointer_size, structs).
--
-- memory(addr, size) is a function that reads size bytes starting at addr,
-- returning them as a string of exactly that length, or nil if unreadable.
--
-- read(addr, typ) returns a reading: {addr, value, error}.
-- error is a string if an error occurred, nil otherwise.
-- For containers, value is a table of child readings.
-- @module mempeep.reader

local function new(schema, memory)
  local ptr_size = schema.pointer_size

  local read -- forward declaration for mutual recursion

  -- Read a pointer-sized integer from addr. Returns nil on failure.
  local function read_pointer(addr)
    local s = memory(addr, ptr_size)
    if s == nil then
      return nil
    end
    local fmt = "<i" .. ptr_size
    return string.unpack(fmt, s)
  end

  local readers = {

    array = function(addr, typ)
      local sub_typ = typ.typ
      local size = schema.sizeof(sub_typ)
      local items = {}
      local p = addr
      for i = 1, typ.count do
        items[i] = read(p, sub_typ)
        p = p + size
      end
      return { addr = addr, value = items }
    end,

    circular_list = function(addr, typ)
      local sub_typ = typ.typ
      local next_field = typ.next_field
      local items = {}
      local head = read_pointer(addr)
      if head == nil then
        return { addr = addr, value = nil, error = "Invalid head pointer" }
      elseif head ~= 0 then
        local p = head
        repeat
          local reading = read(p, sub_typ)
          items[#items + 1] = reading
          local struct_value = reading.value
          if struct_value then
            -- next is a weak ptr field; its raw value is the address integer
            local next_reading = struct_value[next_field]
            local next_addr = next_reading and next_reading.value
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

    ptr = function(addr, typ)
      local p = read_pointer(addr)
      if p == nil then
        return { addr = addr, value = nil, error = "Could not read pointer" }
      end
      if p == 0 then
        if typ.optional then
          return { addr = addr, value = nil }
        end
        return { addr = addr, value = nil, error = "Null pointer" }
      end
      if typ.weak then
        return { addr = addr, value = p }
      end
      return { addr = addr, value = read(p, typ.typ) }
    end,

    primitive = function(addr, typ)
      local s = memory(addr, typ.size)
      if s == nil then
        return { addr = addr, value = nil, error = "Could not read " .. typ.name }
      end
      return { addr = addr, value = typ.decode(s) }
    end,

    struct = function(addr, typ)
      local field_descs = schema.fields(typ.name)
      local obj = {}
      for _, fd in ipairs(field_descs) do
        obj[fd.name] = read(addr + fd.offset, fd.typ)
      end
      return { addr = addr, value = obj }
    end,

    vector = function(addr, typ)
      local sub_typ = typ.typ
      local begin_ptr = read_pointer(addr)
      local end_ptr = read_pointer(addr + ptr_size)
      if begin_ptr == nil or end_ptr == nil then
        return { addr = addr, value = nil, error = "Invalid vector pointers" }
      end
      if begin_ptr > end_ptr then
        return { addr = addr, value = nil, error = "Vector begin pointer is past end pointer" }
      end
      local items = {}
      local p = begin_ptr
      local size = schema.sizeof(sub_typ)
      while p < end_ptr do
        items[#items + 1] = read(p, sub_typ)
        p = p + size
      end
      if p ~= end_ptr then
        items[#items + 1] = { addr = p, value = nil, error = "Vector pointer overflow" }
      end
      return { addr = addr, value = items }
    end,
  }

  read = function(addr, typ)
    if addr == nil then
      return { addr = nil, value = nil, error = "nil address" }
    end
    local r = assert(readers[typ.kind], "reader: unknown type kind: " .. typ.kind)
    return r(addr, typ)
  end

  return read
end

local reader = {}

reader.new = new

return reader
