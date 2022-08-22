#include "smol.h"
#include "main.h"

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  std::unique_ptr<Platform> platform = v8::platform::NewDefaultPlatform();
  V8::InitializePlatform(platform.get());
  V8::Initialize();
  V8::SetFlagsFromString(v8flags);
  if (_v8flags_from_commandline == 1) {
    V8::SetFlagsFromCommandLine(&argc, argv, true);
  }
  register_builtins();
  smol::CreateIsolate(argc, argv, smol_js, smol_js_len, smol::hrtime());
  V8::Dispose();
  V8::ShutdownPlatform();
  platform.reset();
  return 0;
}
