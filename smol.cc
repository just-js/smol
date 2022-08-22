#include "smol.h"

std::map<std::string, NAMESPACE::builtin*> NAMESPACE::builtins;
std::map<std::string, NAMESPACE::register_plugin> NAMESPACE::modules;
uint32_t scriptId = 1;
uint64_t* hrtimeptr;
clock_t clock_id = CLOCK_MONOTONIC;

ssize_t NAMESPACE::process_memory_usage() {
  char buf[1024];
  const char* s = NULL;
  ssize_t n = 0;
  unsigned long val = 0;
  int fd = 0;
  int i = 0;
  do {
    fd = open("/proc/thread-self/stat", O_RDONLY);
  } while (fd == -1 && errno == EINTR);
  if (fd == -1) return (ssize_t)errno;
  do
    n = read(fd, buf, sizeof(buf) - 1);
  while (n == -1 && errno == EINTR);
  close(fd);
  if (n == -1)
    return (ssize_t)errno;
  buf[n] = '\0';
  s = strchr(buf, ' ');
  if (s == NULL)
    goto err;
  s += 1;
  if (*s != '(')
    goto err;
  s = strchr(s, ')');
  if (s == NULL)
    goto err;
  for (i = 1; i <= 22; i++) {
    s = strchr(s + 1, ' ');
    if (s == NULL)
      goto err;
  }
  errno = 0;
  val = strtoul(s, NULL, 10);
  if (errno != 0)
    goto err;
  return val * (unsigned long)getpagesize();
err:
  return 0;
}

uint64_t NAMESPACE::hrtime() {
  struct timespec t;
  if (clock_gettime(clock_id, &t)) return 0;
  return (t.tv_sec * (uint64_t) 1e9) + t.tv_nsec;
}

void NAMESPACE::builtins_add (const char* name, const char* source, 
  unsigned int size) {
  struct builtin* b = new builtin();
  b->size = size;
  b->source = source;
  builtins[name] = b;
}

void NAMESPACE::SET_METHOD(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, FunctionCallback callback) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kInternalized).ToLocalChecked(), 
    FunctionTemplate::New(isolate, callback));
}

void NAMESPACE::SET_MODULE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<ObjectTemplate> module) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kInternalized).ToLocalChecked(), 
    module);
}

void NAMESPACE::SET_VALUE(Isolate *isolate, Local<ObjectTemplate> 
  recv, const char *name, Local<Value> value) {
  recv->Set(String::NewFromUtf8(isolate, name, 
    NewStringType::kInternalized).ToLocalChecked(), 
    value);
}

void NAMESPACE::PrintStackTrace(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope handleScope(isolate);
  Local<Message> message = try_catch.Message();
  Local<StackTrace> stack = message->GetStackTrace();
  Local<Value> scriptName = message->GetScriptResourceName();
  String::Utf8Value scriptname(isolate, scriptName);
  Local<Context> context = isolate->GetCurrentContext();
  int linenum = message->GetLineNumber(context).FromJust();
  v8::String::Utf8Value err_message(isolate, message->Get().As<String>());
  fprintf(stderr, "%s in %s on line %i\n", *err_message, *scriptname, linenum);
  if (stack.IsEmpty()) return;
  for (int i = 0; i < stack->GetFrameCount(); i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(isolate, i);
    Local<String> functionName = stack_frame->GetFunctionName();
    Local<String> scriptName = stack_frame->GetScriptName();
    String::Utf8Value fn_name_s(isolate, functionName);
    String::Utf8Value script_name(isolate, scriptName);
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();
    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr, "    at [eval] (%s:%i:%i)\n", *script_name,
          line_number, column);
      }
      break;
    }
    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr, "    at %s (%s:%i:%i)\n", *fn_name_s, *script_name,
        line_number, column);
    }
  }
  fflush(stderr);
}

void NAMESPACE::PromiseRejectCallback(PromiseRejectMessage data) {
  if (data.GetEvent() == v8::kPromiseRejectAfterResolved ||
      data.GetEvent() == v8::kPromiseResolveAfterResolved) {
    return;
  }
  Local<Promise> promise = data.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  if (data.GetEvent() == v8::kPromiseHandlerAddedAfterReject) {
    return;
  }
  Local<Value> exception = data.GetValue();
  v8::Local<Message> message;
  if (exception->IsObject()) {
    message = v8::Exception::CreateMessage(isolate, exception);
  }
  if (!exception->IsNativeError() &&
      (message.IsEmpty() || message->GetStackTrace().IsEmpty())) {
    exception = v8::Exception::Error(
        v8::String::NewFromUtf8Literal(isolate, "Unhandled Promise."));
    message = Exception::CreateMessage(isolate, exception);
  }
  Local<Context> context = isolate->GetCurrentContext();
  TryCatch try_catch(isolate);
  Local<Object> globalInstance = context->Global();
  Local<Value> func = globalInstance->Get(context, 
    String::NewFromUtf8Literal(isolate, "onUnhandledRejection", 
      NewStringType::kNormal)).ToLocalChecked();
  if (func.IsEmpty()) {
    return;
  }
  Local<Function> onUnhandledRejection = Local<Function>::Cast(func);
  if (try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Cast\n");
    return;
  }
  Local<Value> argv[1] = { exception };
  MaybeLocal<Value> result = onUnhandledRejection->Call(context, 
    globalInstance, 1, argv);
  if (result.IsEmpty() && try_catch.HasCaught()) {
    fprintf(stderr, "PromiseRejectCallback: Call\n");
  }
}

void NAMESPACE::FreeMemory(void* buf, size_t length, void* data) {
  free(buf);
}

char* readFile(char filename[]) {
  std::ifstream file;
  file.open(filename, std::ifstream::ate);
  char* contents;
  if (!file) {
    contents = new char[1];
    return contents;
  }
  size_t file_size = file.tellg();
  file.seekg(0);
  std::filebuf* file_buf = file.rdbuf();
  contents = new char[file_size + 1]();
  file_buf->sgetn(contents, file_size);
  file.close();
  return contents;
}

v8::MaybeLocal<v8::Module> loadModule(char code[],
                                      char name[],
                                      v8::Local<v8::Context> cx) {
  v8::Local<v8::String> vcode =
      v8::String::NewFromUtf8(cx->GetIsolate(), code).ToLocalChecked();
  v8::Local<v8::PrimitiveArray> opts =
      v8::PrimitiveArray::New(cx->GetIsolate(), NAMESPACE::HostDefinedOptions::kLength);
  opts->Set(cx->GetIsolate(), NAMESPACE::HostDefinedOptions::kType,
                            v8::Number::New(cx->GetIsolate(), NAMESPACE::ScriptType::kModule));
  v8::ScriptOrigin origin(cx->GetIsolate(), v8::String::NewFromUtf8(cx->GetIsolate(), name).ToLocalChecked(), // resource name
    0, // line offset
    0,  // column offset
    true, // is shared cross-origin
    -1,  // script id
    v8::Local<v8::Value>(), // source map url
    false, // is opaque
    false, // is wasm
    true, // is module
    opts);
  v8::Context::Scope context_scope(cx);
  v8::ScriptCompiler::Source source(vcode, origin);
  v8::MaybeLocal<v8::Module> mod;
  mod = v8::ScriptCompiler::CompileModule(cx->GetIsolate(), &source);
  return mod;
}

v8::MaybeLocal<v8::Module> NAMESPACE::OnModuleInstantiate(v8::Local<v8::Context> context,
  v8::Local<v8::String> specifier,
  v8::Local<v8::FixedArray> import_assertions, 
  v8::Local<v8::Module> referrer) {
  v8::String::Utf8Value str(context->GetIsolate(), specifier);
  return loadModule(readFile(*str), *str, context);
}

v8::Local<v8::Module> checkModule(v8::MaybeLocal<v8::Module> maybeModule,
  v8::Local<v8::Context> cx) {
  v8::Local<v8::Module> mod;
  if (!maybeModule.ToLocal(&mod)) {
    printf("Error loading module!\n");
    exit(EXIT_FAILURE);
  }
  v8::Maybe<bool> result = mod->InstantiateModule(cx, NAMESPACE::OnModuleInstantiate);
  if (result.IsNothing()) {
    printf("\nCan't instantiate module.\n");
    exit(EXIT_FAILURE);
  }
  return mod;
}

v8::Local<v8::Value> execModule(v8::Local<v8::Module> mod,
  v8::Local<v8::Context> cx,
  bool nsObject) {
  v8::Local<v8::Value> retValue;
  if (!mod->Evaluate(cx).ToLocal(&retValue)) {
    printf("Error evaluating module!\n");
    exit(EXIT_FAILURE);
  }
  if (nsObject)
    return mod->GetModuleNamespace();
  else
    return retValue;
}

v8::MaybeLocal<v8::Promise> OnDynamicImport(v8::Local<v8::Context> context,
  v8::Local<v8::ScriptOrModule> referrer,
  v8::Local<v8::String> specifier,
  v8::Local<v8::FixedArray> import_assertions) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Promise::Resolver::New(context).ToLocalChecked();
  v8::MaybeLocal<v8::Promise> promise(resolver->GetPromise());
  v8::String::Utf8Value name(context->GetIsolate(), specifier);
  v8::Local<v8::Module> mod =
      checkModule(loadModule(readFile(*name), *name, context), context);
  v8::Local<v8::Value> retValue = execModule(mod, context, true);
  resolver->Resolve(context, retValue).ToChecked();
  return promise;
}

int NAMESPACE::CreateIsolate(int argc, char** argv, 
  const char* main_src, unsigned int main_len, 
  const char* js, unsigned int js_len, struct iovec* buf, int fd,
  uint64_t start, const char* name_space, const char* scriptname) {
  Isolate::CreateParams create_params;
  int statusCode = 0;
  create_params.array_buffer_allocator = 
    ArrayBuffer::Allocator::NewDefaultAllocator();
  Isolate *isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    // TODO: make this a config option
    isolate->SetCaptureStackTraceForUncaughtExceptions(true, 1000, 
      StackTrace::kDetailed);
    Local<ObjectTemplate> global = ObjectTemplate::New(isolate);
    Local<ObjectTemplate> runtime = ObjectTemplate::New(isolate);
    NAMESPACE::Init(isolate, runtime);
    global->Set(String::NewFromUtf8(isolate, name_space, 
      NewStringType::kInternalized, strnlen(name_space, 256)).ToLocalChecked(), runtime);
    Local<Context> context = Context::New(isolate, NULL, global);
    Context::Scope context_scope(context);
    isolate->SetPromiseRejectCallback(PromiseRejectCallback);
    isolate->SetHostImportModuleDynamicallyCallback(OnDynamicImport);
    Local<Array> arguments = Array::New(isolate);
    for (int i = 0; i < argc; i++) {
      arguments->Set(context, i, String::NewFromUtf8(isolate, argv[i], 
        NewStringType::kNormal, strlen(argv[i])).ToLocalChecked()).Check();
    }
    Local<Object> globalInstance = context->Global();
    globalInstance->Set(context, String::NewFromUtf8Literal(isolate, 
      "global", 
      NewStringType::kNormal), globalInstance).Check();
    Local<Value> obj = globalInstance->Get(context, 
      String::NewFromUtf8(
        isolate, name_space, 
        NewStringType::kInternalized, strnlen(name_space, 256)).ToLocalChecked()).ToLocalChecked();
    Local<Object> runtimeInstance = Local<Object>::Cast(obj);
    if (buf != NULL) {
      std::unique_ptr<BackingStore> backing = SharedArrayBuffer::NewBackingStore(
          buf->iov_base, buf->iov_len, [](void*, size_t, void*){}, nullptr);
      Local<SharedArrayBuffer> ab = SharedArrayBuffer::New(isolate, std::move(backing));
      runtimeInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "buffer", NewStringType::kNormal), ab).Check();
    }
    if (start > 0) {
      runtimeInstance->Set(context, String::NewFromUtf8Literal(isolate, "start", 
        NewStringType::kNormal), 
        BigInt::New(isolate, start)).Check();
    }
    if (fd != 0) {
      runtimeInstance->Set(context, String::NewFromUtf8Literal(isolate, "fd", 
        NewStringType::kNormal), 
        Integer::New(isolate, fd)).Check();
    }
    runtimeInstance->Set(context, String::NewFromUtf8Literal(isolate, "args", 
      NewStringType::kNormal), arguments).Check();
    if (js_len > 0) {
      runtimeInstance->Set(context, String::NewFromUtf8Literal(isolate, 
        "workerSource", NewStringType::kNormal), 
        String::NewFromUtf8(isolate, js, NewStringType::kNormal, 
        js_len).ToLocalChecked()).Check();
    }
    TryCatch try_catch(isolate);
    Local<v8::PrimitiveArray> opts =
        v8::PrimitiveArray::New(isolate, NAMESPACE::HostDefinedOptions::kLength);
    opts->Set(isolate, NAMESPACE::HostDefinedOptions::kType, 
      v8::Number::New(isolate, NAMESPACE::ScriptType::kModule));
    ScriptOrigin baseorigin(
      isolate,
      String::NewFromUtf8(isolate, scriptname, NewStringType::kInternalized, strnlen(scriptname, 1024)).ToLocalChecked(),
      0, // line offset
      0,  // column offset
      false, // is shared cross-origin
      scriptId++,  // script id
      Local<Value>(), // source map url
      false, // is opaque
      false, // is wasm
      true,  // is module
      opts
    );
    Local<String> base;
    base = String::NewFromUtf8(isolate, main_src, NewStringType::kNormal, 
      main_len).ToLocalChecked();
    ScriptCompiler::Source basescript(base, baseorigin);
    Local<Module> module;
    if (!ScriptCompiler::CompileModule(isolate, &basescript).ToLocal(&module)) {
      PrintStackTrace(isolate, try_catch);
      return 1;
    }
    Maybe<bool> ok2 = module->InstantiateModule(context, NAMESPACE::OnModuleInstantiate);
    if (ok2.IsNothing()) {
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        try_catch.ReThrow();
      }
      return 1;
    }
    module->Evaluate(context).ToLocalChecked();
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
      return 1;
    }
    Local<Value> func = globalInstance->Get(context, 
      String::NewFromUtf8Literal(isolate, "onExit", 
        NewStringType::kNormal)).ToLocalChecked();
    if (func->IsFunction()) {
      Local<Function> onExit = Local<Function>::Cast(func);
      Local<Value> argv[1] = {Integer::New(isolate, 0)};
      MaybeLocal<Value> result = onExit->Call(context, globalInstance, 0, argv);
      if (!result.IsEmpty()) {
        statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
      }
      if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
        NAMESPACE::PrintStackTrace(isolate, try_catch);
        return 2;
      }
      statusCode = result.ToLocalChecked()->Uint32Value(context).ToChecked();
    }
  }
  isolate->ContextDisposedNotification();
  isolate->LowMemoryNotification();
  isolate->ClearKeptObjects();
  bool stop = false;
  while(!stop) {
    stop = isolate->IdleNotificationDeadline(1);  
  }
  isolate->Dispose();
  delete create_params.array_buffer_allocator;
  isolate = nullptr;
  return statusCode;
}

int NAMESPACE::CreateIsolate(int argc, char** argv, const char* main_src, 
  unsigned int main_len, uint64_t start) {
  return CreateIsolate(argc, argv, main_src, main_len, NULL, 0, NULL, 0, start, "smol", "main.js");
}

void NAMESPACE::Print(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stdout, "%s\n", cstr);
  } else {
    fprintf(stdout, "%s", cstr);
  }
}

void NAMESPACE::Error(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  if (args[0].IsEmpty()) return;
  String::Utf8Value str(args.GetIsolate(), args[0]);
  int endline = 1;
  if (args.Length() > 1) {
    endline = static_cast<int>(args[1]->BooleanValue(isolate));
  }
  const char *cstr = *str;
  if (endline == 1) {
    fprintf(stderr, "%s\n", cstr);
  } else {
    fprintf(stderr, "%s", cstr);
  }
}

void NAMESPACE::Load(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<ObjectTemplate> exports = ObjectTemplate::New(isolate);
  if (args[0]->IsString()) {
    String::Utf8Value name(isolate, args[0]);
    auto iter = NAMESPACE::modules.find(*name);
    if (iter == NAMESPACE::modules.end()) {
      return;
    } else {
      register_plugin _init = (*iter->second);
      auto _register = reinterpret_cast<InitializerCallback>(_init());
      _register(isolate, exports);
    }
  } else {
    Local<BigInt> address64 = Local<BigInt>::Cast(args[0]);
    void* ptr = reinterpret_cast<void*>(address64->Uint64Value());
    register_plugin _init = reinterpret_cast<register_plugin>(ptr);
    auto _register = reinterpret_cast<InitializerCallback>(_init());
    _register(isolate, exports);
  }
  args.GetReturnValue().Set(exports->NewInstance(context).ToLocalChecked());
}

void NAMESPACE::Builtin(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value name(isolate, args[0]);
  NAMESPACE::builtin* b = builtins[*name];
  if (b == nullptr) {
    args.GetReturnValue().Set(Null(isolate));
    return;
  }
  if (args.Length() == 1) {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, b->source, 
      NewStringType::kNormal, b->size).ToLocalChecked());
    return;
  }
  std::unique_ptr<BackingStore> backing = SharedArrayBuffer::NewBackingStore(
      (void*)b->source, b->size, [](void*, size_t, void*){}, nullptr);
  Local<SharedArrayBuffer> ab = SharedArrayBuffer::New(isolate, std::move(backing));
  args.GetReturnValue().Set(ab);
}

void NAMESPACE::MemoryUsage(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  ssize_t rss = NAMESPACE::process_memory_usage();
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);
  Local<BigUint64Array> array;
  Local<ArrayBuffer> ab;
  if (args.Length() > 0) {
    array = args[0].As<BigUint64Array>();
    ab = array->Buffer();
  } else {
    ab = ArrayBuffer::New(isolate, 16 * 8);
    array = BigUint64Array::New(ab, 0, 16);
  }
  std::shared_ptr<BackingStore> backing = ab->GetBackingStore();
  uint64_t *fields = static_cast<uint64_t *>(backing->Data());
  fields[0] = rss;
  fields[1] = v8_heap_stats.total_heap_size();
  fields[2] = v8_heap_stats.used_heap_size();
  fields[3] = v8_heap_stats.external_memory();
  fields[4] = v8_heap_stats.does_zap_garbage();
  fields[5] = v8_heap_stats.heap_size_limit();
  fields[6] = v8_heap_stats.malloced_memory();
  fields[7] = v8_heap_stats.number_of_detached_contexts();
  fields[8] = v8_heap_stats.number_of_native_contexts();
  fields[9] = v8_heap_stats.peak_malloced_memory();
  fields[10] = v8_heap_stats.total_available_size();
  fields[11] = v8_heap_stats.total_heap_size_executable();
  fields[12] = v8_heap_stats.total_physical_size();
  fields[13] = isolate->AdjustAmountOfExternalAllocatedMemory(0);
  args.GetReturnValue().Set(array);
}

void NAMESPACE::Sleep(const FunctionCallbackInfo<Value> &args) {
  sleep(Local<Integer>::Cast(args[0])->Value());
}

void NAMESPACE::Exit(const FunctionCallbackInfo<Value>& args) {
  exit(Local<Integer>::Cast(args[0])->Value());
}

void NAMESPACE::PID(const FunctionCallbackInfo<Value> &args) {
  args.GetReturnValue().Set(Integer::New(args.GetIsolate(), getpid()));
}

void NAMESPACE::Chdir(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  String::Utf8Value path(isolate, args[0]);
  args.GetReturnValue().Set(Integer::New(isolate, chdir(*path)));
}

void NAMESPACE::Builtins(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Array> b = Array::New(isolate);
  int i = 0;
  for (auto const& builtin : builtins) {
    b->Set(context, i++, String::NewFromUtf8(isolate, builtin.first.c_str(), 
      NewStringType::kNormal, builtin.first.length()).ToLocalChecked()).Check();
  }
  args.GetReturnValue().Set(b);
}

void NAMESPACE::Modules(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Array> m = Array::New(isolate);
  int i = 0;
  for (auto const& module : modules) {
    m->Set(context, i++, String::NewFromUtf8(isolate, module.first.c_str(), 
      NewStringType::kNormal, module.first.length()).ToLocalChecked()).Check();
  }
  args.GetReturnValue().Set(m);
}

void NAMESPACE::AllocHRTime(const FunctionCallbackInfo<Value> &args) {
  hrtimeptr = (uint64_t*)args[0].As<ArrayBuffer>()->GetBackingStore()->Data();
}

void NAMESPACE::HRTime(const FunctionCallbackInfo<Value> &args) {
  *hrtimeptr = NAMESPACE::hrtime();
}

void NAMESPACE::RunScript(const FunctionCallbackInfo<Value> &args) {
  Isolate *isolate = args.GetIsolate();
  Local<Context> context = isolate->GetEnteredOrMicrotaskContext();
  TryCatch try_catch(isolate);
  Local<String> source = args[0].As<String>();
  Local<String> path = args[1].As<String>();
  Local<v8::PrimitiveArray> opts =
      v8::PrimitiveArray::New(isolate, 1);
  opts->Set(isolate, 0, v8::Number::New(isolate, 1));
  ScriptOrigin baseorigin(isolate, path, // resource name
    0, // line offset
    0,  // column offset
    false, // is shared cross-origin
    -1,  // script id
    Local<Value>(), // source map url
    false, // is opaque
    false, // is wasm
    false, // is module
    opts);
  Local<Script> script;
  ScriptCompiler::Source basescript(source, baseorigin);
  bool ok = ScriptCompiler::Compile(context, &basescript).ToLocal(&script);
  if (!ok) {
    if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
      try_catch.ReThrow();
    }
    return;
  }
  MaybeLocal<Value> result = script->Run(context);
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    try_catch.ReThrow();
    return;
  }
  args.GetReturnValue().Set(result.ToLocalChecked());
}

void NAMESPACE::NextTick(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->EnqueueMicrotask(args[0].As<Function>());
}

void NAMESPACE::Init(Isolate* isolate, Local<ObjectTemplate> target) {
  Local<ObjectTemplate> version = ObjectTemplate::New(isolate);
  SET_VALUE(isolate, version, "smol", String::NewFromUtf8Literal(isolate, 
    VERSION));
  SET_VALUE(isolate, version, "v8", String::NewFromUtf8(isolate, 
    v8::V8::GetVersion()).ToLocalChecked());
  Local<ObjectTemplate> kernel = ObjectTemplate::New(isolate);
  utsname kernel_rec;
  int rc = uname(&kernel_rec);
  if (rc == 0) {
    kernel->Set(String::NewFromUtf8Literal(isolate, "os", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.sysname).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "release", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.release).ToLocalChecked());
    kernel->Set(String::NewFromUtf8Literal(isolate, "version", 
      NewStringType::kNormal), String::NewFromUtf8(isolate, 
      kernel_rec.version).ToLocalChecked());
  }
  version->Set(String::NewFromUtf8Literal(isolate, "kernel", 
    NewStringType::kNormal), kernel);
  SET_METHOD(isolate, target, "print", Print);
  SET_METHOD(isolate, target, "error", Error);
  SET_METHOD(isolate, target, "exit", Exit);
  SET_METHOD(isolate, target, "pid", PID);
  SET_METHOD(isolate, target, "chdir", Chdir);
  SET_METHOD(isolate, target, "nextTick", NextTick);
  SET_METHOD(isolate, target, "sleep", Sleep);
  SET_METHOD(isolate, target, "allochrtime", AllocHRTime);
  SET_METHOD(isolate, target, "hrtime", HRTime);
  SET_MODULE(isolate, target, "version", version);
  SET_METHOD(isolate, target, "memoryUsage", MemoryUsage);
  SET_METHOD(isolate, target, "load", Load);
  SET_METHOD(isolate, target, "builtin", Builtin);
  SET_METHOD(isolate, target, "builtins", Builtins);
  SET_METHOD(isolate, target, "modules", Modules);
}
