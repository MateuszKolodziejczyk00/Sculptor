#pragma once


#define SPT_MAYBE_UNUSED [[maybe_unused]]
#define SPT_NODISCARD [[nodiscard]]
#define SPT_INLINE inline

#define IN
#define OUT
#define INOUT

#define SPT_STRING(s) #s
#define SPT_EVAL_STRING(s) SPT_STRING(s)

#define SPT_SOURCE_LOCATION "File: " SPT_EVAL_STRING(__FILE__) ", Line: " SPT_EVAL_STRING(__LINE__)

#define SPT_SCOPE_NAME_CONCAT(begin, end) begin ## end
#define SPT_SCOPE_NAME_EVAL(begin, end) SPT_SCOPE_NAME_CONCAT(begin, end)

#define SPT_SCOPE_NAME(name) SPT_SCOPE_NAME_EVAL(name, __LINE__)

/** Macro that can be used to send argument which contains commas to other macro */
#define SPT_SINGLE_ARG(...) __VA_ARGS__


