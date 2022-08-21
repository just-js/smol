#include "just.h"
#include "main.h"

int main(int argc, char** argv) {
  uint64_t start = just::hrtime();
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();
  v8::V8::SetFlagsFromString(v8flags);
  if (_v8flags_from_commandline == 1) {
    v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  }
  register_builtins();
  if (_use_index) {
    just::CreateIsolate(argc, argv, just_js, just_js_len, 
      index_js, index_js_len, 
      NULL, 0, start);
  } else {
    just::CreateIsolate(argc, argv, just_js, just_js_len, start);
  }
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  platform.reset();
  return 0;
}
