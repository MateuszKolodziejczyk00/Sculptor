#pragma once

#if DO_CHECKS
#include "Logging/Log.h"
#include "Utility/UtilityMacros.h"
#endif // DO_CHECKS


#if DO_CHECKS

SPT_DEFINE_LOG_CATEGORY(Assertions, true)

#define SPT_CHECK_MSG(Expression, ...)	if(!(Expression)) { SPT_LOG_FATAL(Assertions, "Fatal(" SPT_SOURCE_LOCATION "): " __VA_ARGS__); assert(Expression); }
#define SPT_CHECK(Expression)			if(!(Expression)) { assert(Expression); }

#define SPT_CHECK_NO_ENTRY_MSG(__VA_ARGS__)	SPT_CHECK_MSG(false, __VA_ARGS__)
#define SPT_CHECK_NO_ENTRY()				SPT_CHECK(false)

#else

#define SPT_CHECK_MSG(Expression, __VA_ARGS__)
#define SPT_CHECK(Expression)

#define SPT_CHECK_NO_ENTRY_MSG(__VA_ARGS__)
#define SPT_CHECK_NO_ENTRY()

#endif // DO_CHECKS

#define SPT_STATIC_CHECK_MSG(Expression, __VA_ARGS__)	static_assert(Expression, __VA_ARGS__)
#define SPT_STATIC_CHECK(Expression)					static_assert(Expression)