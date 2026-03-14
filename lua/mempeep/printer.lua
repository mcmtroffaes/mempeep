--- Print readings from memory.
-- Provides printer.new(schema) -> print_reading, where
-- print_reading(typ, reading, indent, label) prints a human-readable
-- representation of a reading to stdout.
-- @module mempeep.printer

local function new(schema)
  local print_reading -- forward declaration for mutual recursion

  local function print_line(addr, indent, label, value_str)
    local pad = string.rep("  ", indent)
    local addr_str = addr and string.format("[%08X]", addr) or "[--------]"
    local label_str = label and (label .. ": ") or ""
    print(addr_str .. " " .. pad .. label_str .. value_str)
  end

  local printers = {}

  printers.array = function(typ, value, addr, indent, label)
    print_line(addr, indent, label, "array(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_reading(typ.typ, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  printers.circular_list = function(typ, value, addr, indent, label)
    print_line(addr, indent, label, "circular_list(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_reading(typ.typ, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  printers.primitive = function(typ, value, addr, indent, label)
    print_line(addr, indent, label, tostring(value))
  end

  printers.ptr = function(typ, value, addr, indent, label)
    if typ.weak then
      -- Weak pointer: value is a raw address integer.
      print_line(addr, indent, label, string.format("0x%X", value))
    else
      -- Followed pointer: value is a nested reading for the pointee.
      print_line(addr, indent, label, "ptr")
      print_reading(typ.typ, value, indent + 1, nil)
    end
  end

  printers.struct = function(typ, value, addr, indent, label)
    local field_descs = schema.fields(typ.name)
    print_line(addr, indent, label, typ.name)
    for _, fd in ipairs(field_descs) do
      if not (fd.opts and fd.opts.print == false) then
        print_reading(fd.typ, value[fd.name], indent + 1, fd.name)
      end
    end
  end

  printers.vector = function(typ, value, addr, indent, label)
    print_line(addr, indent, label, "vector(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_reading(typ.typ, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  --- Print a reading.
  -- reading is the {addr, value, error} table produced by mempeep.reader.
  -- Containers hold a table of child readings as their value.
  print_reading = function(typ, reading, indent, label)
    indent = indent or 0

    if reading == nil then
      print_line(nil, indent, label, "nil")
      return
    end

    local addr = reading.addr

    if reading.error then
      print_line(addr, indent, label, "ERROR: " .. reading.error)
      -- If there is also a partial value, fall through and print it.
      if reading.value == nil then
        return
      end
    elseif reading.value == nil then
      print_line(addr, indent, label, "nil")
      return
    end

    local p = assert(printers[typ.kind], "print: unknown type kind: " .. typ.kind)
    p(typ, reading.value, addr, indent, label)
  end

  return print_reading
end

local printer = {}

printer.new = new

return printer
