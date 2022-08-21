.global _binary_just_cc_start
_binary_just_cc_start:
        .incbin "just.cc"
        .global _binary_just_cc_end
_binary_just_cc_end:
.global _binary_Makefile_start
_binary_Makefile_start:
        .incbin "Makefile"
        .global _binary_Makefile_end
_binary_Makefile_end:
.global _binary_main_cc_start
_binary_main_cc_start:
        .incbin "main.cc"
        .global _binary_main_cc_end
_binary_main_cc_end:
.global _binary_just_h_start
_binary_just_h_start:
        .incbin "just.h"
        .global _binary_just_h_end
_binary_just_h_end:
.global _binary_just_js_start
_binary_main_js_start:
        .incbin "main.js"
        .global _binary_main_js_end
_binary_main_js_end:
