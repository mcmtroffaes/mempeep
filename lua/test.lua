package.path = package.path .. ";./test/?.lua"
local failures = 0
for _, name in ipairs({ "test_T", "test_D", "test_schema", "test_reader", "test_printer", "test_decoder" }) do
  local ok, err = pcall(require, name)
  if ok then
    print(name .. ": passed")
  else
    print(name .. ": FAILED -- " .. err)
    failures = failures + 1
  end
end
if failures > 0 then
  os.exit(1)
end
