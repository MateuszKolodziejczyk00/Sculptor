#pragma once


#define SPT_MAYBE_UNUSED [[maybe_unused]]
#define SPT_NODISCARD [[nodiscard]]
#define SPT_INLINE inline

#define OUT

#define SPT_STRING(s) #s
#define SPT_EVAL_STRING(s) SPT_STRING(s)

#define SPT_SOURCE_LOCATION "File: " SPT_EVAL_STRING(__FILE__) ", Line: " SPT_EVAL_STRING(__LINE__)