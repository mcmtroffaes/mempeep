--- Print raw values read from memory.
-- Provides printer.new(structs) -> print_rawval, where
-- print_rawval(type_ref, rawval, indent, label) prints a human-readable
-- representation of a raw value to stdout.
-- @module mempeep.printer

local function new(structs)
  -- Build name->struct lookup for O(1) access.
  local struct_by_name = {}
  for _, s in ipairs(structs) do
    struct_by_name[s.name] = s
  end

  local print_rawval -- forward declaration for mutual recursion

  local function print_line(addr, indent, label, value_str)
    local pad = string.rep("  ", indent)
    local addr_str = addr and string.format("[%08X]", addr) or "[--------]"
    local label_str = label and (label .. ": ") or ""
    print(addr_str .. " " .. pad .. label_str .. value_str)
  end

  local printers = {}

  printers.array = function(type_ref, value, addr, indent, label)
    print_line(addr, indent, label, "array(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_rawval(type_ref.type_ref, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  printers.circular_list = function(type_ref, value, addr, indent, label)
    print_line(addr, indent, label, "circular_list(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_rawval(type_ref.type_ref, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  printers.primitive = function(type_ref, value, addr, indent, label)
    print_line(addr, indent, label, tostring(value))
  end

  printers.ptr = function(type_ref, value, addr, indent, label)
    if type_ref.weak then
      -- Weak pointer: value is a raw address integer.
      print_line(addr, indent, label, string.format("0x%X", value))
    else
      -- Followed pointer: value is a nested rawval for the pointee.
      print_line(addr, indent, label, "ptr")
      print_rawval(type_ref.type_ref, value, indent + 1, nil)
    end
  end

  printers.struct = function(type_ref, value, addr, indent, label)
    local s = struct_by_name[type_ref.name]
    if not s then
      error("print: unknown struct '" .. type_ref.name .. "'")
    end
    print_line(addr, indent, label, type_ref.name)
    for _, desc in ipairs(s.descriptors) do
      if desc.kind == "field" then
        if not (desc.opts and desc.opts.print == false) then
          print_rawval(desc.type_ref, value[desc.name], indent + 1, desc.name)
        end
      end
    end
  end

  printers.vector = function(type_ref, value, addr, indent, label)
    print_line(addr, indent, label, "vector(" .. #value .. ")")
    for i, elem in ipairs(value) do
      print_rawval(type_ref.type_ref, elem, indent + 1, "[" .. (i - 1) .. "]")
    end
  end

  --- Print raw value.
  -- rawval is the {addr, value, error} table produced by mempeep.reader.
  -- Containers hold a table of child rawvals as their value.
  print_rawval = function(type_ref, rawval, indent, label)
    indent = indent or 0

    if rawval == nil then
      print_line(nil, indent, label, "nil")
      return
    end

    local addr = rawval.addr

    if rawval.error then
      print_line(addr, indent, label, "ERROR: " .. rawval.error)
      -- If there is also a partial value, fall through and print it.
      if rawval.value == nil then
        return
      end
    elseif rawval.value == nil then
      print_line(addr, indent, label, "nil")
      return
    end

    local p = assert(printers[type_ref.kind], "print: unknown type kind: " .. type_ref.kind)
    p(type_ref, rawval.value, addr, indent, label)
  end

  return print_rawval
end

local printer = {}

printer.new = new

return printer
