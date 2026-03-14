local T       = require("mempeep.T")
local D       = require("mempeep.D")
local compute = require("mempeep.compute")
local reader  = require("mempeep.reader")

--- Build a flat byte table from a list of {offset, bytes} pairs, where each
--- entry in bytes is a list of byte values.  All accesses outside the written
--- ranges return nil (simulating unreadable memory).
local function make_mem(pointer_size, writes)
    local bytes = {}
    for _, w in ipairs(writes) do
        local base = w[1]
        for i, b in ipairs(w[2]) do
            bytes[base + i - 1] = b
        end
    end

    local function read_int(addr, size, signed)
        local v = 0
        for i = size - 1, 0, -1 do
            local b = bytes[addr + i]
            if b == nil then return nil end
            v = v * 256 + b
        end
        -- sign-extend if requested and top bit is set
        if signed then
            local limit = 2 ^ (size * 8 - 1)
            if v >= limit then v = v - limit * 2 end
        end
        return v
    end

    local mem = {}

    mem.pointer = function(addr) return read_int(addr, pointer_size, true) end
    mem.i8      = function(addr) return read_int(addr, 1, true) end
    mem.i16     = function(addr) return read_int(addr, 2, true) end
    mem.i32     = function(addr) return read_int(addr, 4, true) end
    mem.i64     = function(addr) return read_int(addr, 8, true) end
    mem.float   = function(addr)
        -- return a dummy float-shaped number for testing purposes
        return read_int(addr, 4, false)
    end
    mem.string  = function(addr, max_length)
        local chars = {}
        for i = 0, max_length - 1 do
            local b = bytes[addr + i]
            if b == nil then return nil end
            if b == 0 then break end
            chars[#chars + 1] = string.char(b)
        end
        return table.concat(chars)
    end

    return mem
end

--- Encode a little-endian integer into a byte list of the given width.
local function le(value, width)
    local out = {}
    for _ = 1, width do
        out[#out + 1] = value % 256
        value = math.floor(value / 256)
    end
    return out
end

--- Concatenate multiple byte lists into one.
local function cat(...)
    local out = {}
    for _, t in ipairs({...}) do
        for _, b in ipairs(t) do out[#out + 1] = b end
    end
    return out
end

-- ---------------------------------------------------------------------------
-- Scalars
-- ---------------------------------------------------------------------------

do
    local structs = { D.struct("S", { D.field("v", T.i32) }) }
    local c   = compute.new(4, structs)
    local mem = make_mem(4, { {0x100, le(42, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x100, T.i32)
    assert(r.value == 42)
    assert(r.addr  == 0x100)
    assert(r.error == nil)
end

do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x200, le(255, 1)} })
    local read = reader.new(c, mem)

    -- 255 as unsigned byte = -1 as signed i8
    local r = read(0x200, T.i8())
    assert(r.value == -1)
end

do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x300, le(1000, 2)} })
    local read = reader.new(c, mem)

    local r = read(0x300, T.i16())
    assert(r.value == 1000)
end

do
    local c   = compute.new(8, {})
    local mem = make_mem(8, { {0x400, le(123456789, 8)} })
    local read = reader.new(c, mem)

    local r = read(0x400, T.i64())
    assert(r.value == 123456789)
end

-- Scalar with unreadable address returns error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {})
    local read = reader.new(c, mem)

    local r = read(0x999, T.i32)
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- nil address returns error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {})
    local read = reader.new(c, mem)

    local r = read(nil, T.i32)
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- String
-- ---------------------------------------------------------------------------

do
    local hello = {104, 101, 108, 108, 111, 0}  -- "hello\0"
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x500, hello} })
    local read = reader.new(c, mem)

    local r = read(0x500, T.string(16))
    assert(r.value == "hello")
    assert(r.error == nil)
end

-- String with no null terminator reads up to max_length
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x510, {65, 66, 67}} })  -- "ABC", no null
    local read = reader.new(c, mem)

    local r = read(0x510, T.string(3))
    assert(r.value == "ABC")
end

-- Unreadable string address
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {})
    local read = reader.new(c, mem)

    local r = read(0xDEAD, T.string(8))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Struct
-- ---------------------------------------------------------------------------

do
    local point = D.struct("Point", {
        D.field("x", T.i32),
        D.field("y", T.i32),
    })
    local c   = compute.new(4, {point})
    local mem = make_mem(4, {
        {0x600, cat(le(10, 4), le(20, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0x600, T.struct("Point"))
    assert(r.error == nil)
    assert(r.value ~= nil)
    assert(r.value.x.value == 10)
    assert(r.value.y.value == 20)
end

-- Struct with pad
do
    local padded = D.struct("Padded", {
        D.field("a", T.i32),
        D.pad(4),
        D.field("b", T.i32),
    })
    local c   = compute.new(4, {padded})
    -- a at 0, pad 4 bytes, b at 8
    local mem = make_mem(4, {
        {0x700, cat(le(1, 4), le(0, 4), le(2, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0x700, T.struct("Padded"))
    assert(r.value.a.value == 1)
    assert(r.value.b.value == 2)
end

-- Struct with D.offset
do
    local s = D.struct("Sparse", {
        D.field("a", T.i32),
        D.offset(16),
        D.field("b", T.i32),
    })
    local c   = compute.new(4, {s})
    local mem = make_mem(4, {
        {0x800, le(7, 4)},
        {0x810, le(99, 4)},
    })
    local read = reader.new(c, mem)

    local r = read(0x800, T.struct("Sparse"))
    assert(r.value.a.value == 7)
    assert(r.value.b.value == 99)
end

-- ---------------------------------------------------------------------------
-- Pointers
-- ---------------------------------------------------------------------------

-- T.ptr (weak, non-optional): returns raw address
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x900, le(0xABCD, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x900, T.ptr(T.i32))
    assert(r.value == 0xABCD)
    assert(r.error == nil)
end

-- T.ptr (weak, non-optional) with null pointer returns error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x910, le(0, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x910, T.ptr(T.i32))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- T.optional_ptr with null pointer: no error, nil value
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x920, le(0, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x920, T.optional_ptr(T.i32))
    assert(r.value == nil)
    assert(r.error == nil)
end

-- T.optional_ptr with non-null: returns raw address (weak)
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x930, le(0x1234, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x930, T.optional_ptr(T.i32))
    assert(r.value == 0x1234)
end

-- T.ref (non-weak, non-optional): follows pointer and reads pointee
do
    local c   = compute.new(4, {})
    -- addr 0x940 holds pointer to 0xA00; 0xA00 holds i32 value 77
    local mem = make_mem(4, {
        {0x940, le(0xA00, 4)},
        {0xA00, le(77, 4)},
    })
    local read = reader.new(c, mem)

    local r = read(0x940, T.ref(T.i32))
    assert(r.error == nil)
    assert(r.value ~= nil)
    local inner = r.value
    assert(type(inner) == "table")
    assert(inner.value == 77)
end

-- T.ref null pointer: error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x950, le(0, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x950, T.ref(T.i32))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- T.optional_ref with null: nil value, no error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, { {0x960, le(0, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x960, T.optional_ref(T.i32))
    assert(r.value == nil)
    assert(r.error == nil)
end

-- T.optional_ref non-null: follows and reads
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {
        {0x970, le(0xB00, 4)},
        {0xB00, le(55, 4)},
    })
    local read = reader.new(c, mem)

    local r = read(0x970, T.optional_ref(T.i32))
    assert(r.error == nil)
    assert(r.value ~= nil)
    assert(r.value.value == 55)
end

-- ---------------------------------------------------------------------------
-- Array
-- ---------------------------------------------------------------------------

do
    local c   = compute.new(4, {})
    -- array of 3 x i32 at 0xC00
    local mem = make_mem(4, {
        {0xC00, cat(le(10, 4), le(20, 4), le(30, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0xC00, T.array(T.i32, 3))
    assert(r.error == nil)
    assert(#r.value == 3)
    assert(r.value[1].value == 10)
    assert(r.value[2].value == 20)
    assert(r.value[3].value == 30)
end

do
    local point = D.struct("Point2", {
        D.field("x", T.i16()),
        D.field("y", T.i16()),
    })
    local c   = compute.new(4, {point})
    local mem = make_mem(4, {
        {0xD00, cat(le(1, 2), le(2, 2), le(3, 2), le(4, 2))}
    })
    local read = reader.new(c, mem)

    local r = read(0xD00, T.array(T.struct("Point2"), 2))
    assert(r.error == nil)
    assert(#r.value == 2)
    assert(r.value[1].value.x.value == 1)
    assert(r.value[1].value.y.value == 2)
    assert(r.value[2].value.x.value == 3)
    assert(r.value[2].value.y.value == 4)
end

-- ---------------------------------------------------------------------------
-- Vector
-- ---------------------------------------------------------------------------

do
    local c   = compute.new(4, {})
    -- begin=0xE10, end=0xE1C (3 x i32 = 12 bytes)
    local mem = make_mem(4, {
        {0xE00, cat(le(0xE10, 4), le(0xE1C, 4))},
        {0xE10, cat(le(100, 4), le(200, 4), le(300, 4))},
    })
    local read = reader.new(c, mem)

    local r = read(0xE00, T.vector(T.i32))
    assert(r.error == nil)
    assert(#r.value == 3)
    assert(r.value[1].value == 100)
    assert(r.value[2].value == 200)
    assert(r.value[3].value == 300)
end

-- Empty vector (begin == end)
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {
        {0xF00, cat(le(0x1000, 4), le(0x1000, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0xF00, T.vector(T.i32))
    assert(r.error == nil)
    assert(#r.value == 0)
end

-- Vector begin > end: error
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {
        -- begin=0x2000, end=0x1000  (inverted)
        {0x1F00, cat(le(0x2000, 4), le(0x1000, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0x1F00, T.vector(T.i32))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- Unreadable vector pointers
do
    local c   = compute.new(4, {})
    local mem = make_mem(4, {})
    local read = reader.new(c, mem)

    local r = read(0xDEAD, T.vector(T.i32))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Circular list
-- ---------------------------------------------------------------------------

do
    -- Three-node circular list: A -> B -> C -> A
    -- Each node: value(i32) at +0, next(ptr) at +4  => 8 bytes per node
    -- Pointer size 4.
    -- head pointer at 0x3000, pointing to node A at 0x3010
    -- A at 0x3010: value=1, next=0x3018
    -- B at 0x3018: value=2, next=0x3020
    -- C at 0x3020: value=3, next=0x3010 (back to head)

    local node = D.struct("Node", {
        D.field("value", T.i32),
        D.field("next",  T.ptr(T.struct("Node"))),
    })
    D.assert_valid({node})

    local c   = compute.new(4, {node})
    local mem = make_mem(4, {
        {0x3000, le(0x3010, 4)},
        {0x3010, cat(le(1, 4), le(0x3018, 4))},
        {0x3018, cat(le(2, 4), le(0x3020, 4))},
        {0x3020, cat(le(3, 4), le(0x3010, 4))},
    })
    local read = reader.new(c, mem)

    local container = D.struct("Container", {
        D.field("nodes", T.circular_list(T.struct("Node"))),
    })
    local c2   = compute.new(4, {node, container})
    local read2 = reader.new(c2, mem)

    local r = read2(0x3000, T.circular_list(T.struct("Node")))
    assert(r.error == nil)
    assert(#r.value == 3)
    assert(r.value[1].value.value.value == 1)
    assert(r.value[2].value.value.value == 2)
    assert(r.value[3].value.value.value == 3)
end

-- Empty circular list (head pointer is null)
do
    local node = D.struct("NodeE", {
        D.field("value", T.i32),
        D.field("next",  T.ptr(T.struct("NodeE"))),
    })
    local c   = compute.new(4, {node})
    local mem = make_mem(4, { {0x4000, le(0, 4)} })
    local read = reader.new(c, mem)

    local r = read(0x4000, T.circular_list(T.struct("NodeE")))
    assert(r.error == nil)
    assert(#r.value == 0)
end

-- Unreadable head pointer
do
    local node = D.struct("NodeU", {
        D.field("value", T.i32),
        D.field("next",  T.ptr(T.struct("NodeU"))),
    })
    local c   = compute.new(4, {node})
    local mem = make_mem(4, {})
    local read = reader.new(c, mem)

    local r = read(0xBAD0, T.circular_list(T.struct("NodeU")))
    assert(r.value == nil)
    assert(r.error ~= nil)
end

-- ---------------------------------------------------------------------------
-- Nested struct (struct field inside struct)
-- ---------------------------------------------------------------------------

do
    local inner = D.struct("Inner2", {
        D.field("a", T.i32),
        D.field("b", T.i32),
    })
    local outer = D.struct("Outer2", {
        D.field("inner", T.struct("Inner2")),
        D.field("c",     T.i32),
    })
    local c   = compute.new(4, {inner, outer})
    local mem = make_mem(4, {
        {0x5000, cat(le(11, 4), le(22, 4), le(33, 4))}
    })
    local read = reader.new(c, mem)

    local r = read(0x5000, T.struct("Outer2"))
    assert(r.error == nil)
    assert(r.value.inner.value.a.value == 11)
    assert(r.value.inner.value.b.value == 22)
    assert(r.value.c.value             == 33)
end

-- ---------------------------------------------------------------------------
-- Unknown type kind
-- ---------------------------------------------------------------------------

do
    local c    = compute.new(4, {})
    local mem  = make_mem(4, {})
    local read = reader.new(c, mem)

    local ok, err = pcall(read, 0x100, {kind = "bogus"})
    assert(not ok)
    assert(err:find("bogus"))
end
