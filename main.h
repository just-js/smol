extern char _binary_just_cc_start[];
extern char _binary_just_cc_end[];
extern char _binary_Makefile_start[];
extern char _binary_Makefile_end[];
extern char _binary_main_cc_start[];
extern char _binary_main_cc_end[];
extern char _binary_just_h_start[];
extern char _binary_just_h_end[];
extern char _binary_main_js_start[];
extern char _binary_main_js_end[];
extern "C" {
  //extern void* _register_sys();
}
void register_builtins() {
  just::builtins_add("just.cc", _binary_just_cc_start, _binary_just_cc_end - _binary_just_cc_start);
  just::builtins_add("Makefile", _binary_Makefile_start, _binary_Makefile_end - _binary_Makefile_start);
  just::builtins_add("main.cc", _binary_main_cc_start, _binary_main_cc_end - _binary_main_cc_start);
  just::builtins_add("just.h", _binary_just_h_start, _binary_just_h_end - _binary_just_h_start);
  just::builtins_add("main.js", _binary_main_js_start, _binary_main_js_end - _binary_main_js_start);
  //just::modules["sys"] = &_register_sys;
}
static unsigned int just_js_len = _binary_main_js_end - _binary_main_js_start;
static const char* just_js = _binary_main_js_start;
static unsigned int index_js_len = 0;
static const char* index_js = NULL;
static unsigned int _use_index = 0;
static const char* v8flags = "--stack-trace-limit=10 --use-strict --disallow-code-generation-from-strings";
static unsigned int _v8flags_from_commandline = 1;