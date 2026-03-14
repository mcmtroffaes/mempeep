local T = require("mempeep.T")

-- primitive types
do
  local f32 = T.assert_valid(T.f32)
  local f64 = T.assert_valid(T.f64)
  local i8 = T.assert_valid(T.i8)
  local i16 = T.assert_valid(T.i16)
  local i32 = T.assert_valid(T.i32)
  local i64 = T.assert_valid(T.i64)
  local u8 = T.assert_valid(T.u8)
  local u16 = T.assert_valid(T.u16)
  local u32 = T.assert_valid(T.u32)
  local u64 = T.assert_valid(T.u64)
  local str = T.assert_valid(T.string(15))
  assert(f32.kind == "primitive")
  assert(f64.kind == "primitive")
  assert(i8.kind == "primitive")
  assert(i16.kind == "primitive")
  assert(i32.kind == "primitive")
  assert(i64.kind == "primitive")
  assert(str.kind == "primitive")
  assert(f32.size == 4)
  assert(f64.size == 8)
  assert(i8.size == 1)
  assert(i16.size == 2)
  assert(i32.size == 4)
  assert(i64.size == 8)
  assert(u8.size == 1)
  assert(u16.size == 2)
  assert(u32.size == 4)
  assert(u64.size == 8)
  assert(i8.decode("\xFF") == -1)
  assert(u8.decode("\xFF") == 255)
  assert(i16.decode("\x11\xFF") == -239)
  assert(u16.decode("\x11\xFF") == 65297)
  assert(i32.decode("\x44\x33\x22\x11") == 0x11223344)
  assert(i64.decode("\x66\x55\x44\x33\x22\x11\x00\x00") == 0x112233445566)
  assert(i64.decode("\x99\xAA\xBB\xCC\xDD\xEE\xFF\xFF") == -0x112233445567)
  assert(f32.decode("\x00\x00\x80\x3F") == 1.0)
  assert(f64.decode("\x00\x00\x00\x00\x00\x00\xF0\x3F") == 1.0)
  assert(str.size == 15)
  assert(str.decode("hello\0world!!!") == "hello")
  assert(str.decode("HelloThereWorld") == "HelloThereWorld")
  local ok, err = pcall(T.assert_valid, T.string(0)) -- requires size >= 1
  assert(not ok)
  assert(err:find("size"), err)
end

-- array
do
  local arr = T.assert_valid(T.array(T.i32, 5))
  assert(arr.kind == "array")
  assert(arr.typ == T.i32)
  assert(arr.count == 5)
  local ok, err = pcall(T.assert_valid, T.array(T.i32, 0)) -- no zero count
  assert(not ok)
  assert(err:find("count"), err)
end

-- circular_list
do
  local stru = T.assert_valid(T.struct("Item"))
  assert(stru.kind == "struct")
  assert(stru.name == "Item")

  local circ = T.assert_valid(T.circular_list(stru))
  assert(circ.kind == "circular_list")
  assert(circ.typ == stru)
  assert(circ.next_field == "next")

  local circ2 = T.assert_valid(T.circular_list(stru, "sibling"))
  assert(circ2.next_field == "sibling")

  local ok, err = pcall(T.assert_valid, T.circular_list(T.i32))
  assert(not ok)
  assert(err:find("element must be a struct"), err)
end

-- pointer types: single constructor, all four combinations via opts
do
  -- default: non-optional, non-weak (followed)
  local p = T.assert_valid(T.ptr(T.i32))
  assert(p.kind == "ptr")
  assert(p.typ == T.i32)
  assert(p.optional == false)
  assert(p.weak == false)

  -- non-optional weak (raw address)
  local pw = T.assert_valid(T.ptr(T.i32, { weak = true }))
  assert(pw.optional == false)
  assert(pw.weak == true)

  -- optional non-weak (followed, nullable)
  local po = T.assert_valid(T.ptr(T.i32, { optional = true }))
  assert(po.optional == true)
  assert(po.weak == false)

  -- optional weak (raw address, nullable)
  local pow = T.assert_valid(T.ptr(T.i32, { optional = true, weak = true }))
  assert(pow.optional == true)
  assert(pow.weak == true)
end

-- vector
do
  local vec = T.assert_valid(T.vector(T.i32))
  assert(vec.kind == "vector")
  assert(vec.typ == T.i32)
end

-- unknown kind errors
do
  local ok, err = pcall(T.assert_valid, {})
  assert(not ok)
  assert(err:find("kind"), err)

  local ok, err = pcall(T.assert_valid, { kind = "unknown" })
  assert(not ok)
  assert(err:find("kind"), err)
end
