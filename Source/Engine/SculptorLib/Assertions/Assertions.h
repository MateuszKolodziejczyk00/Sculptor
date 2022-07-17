#pragma once


#if DO_CHECKS

#define CHECK(Expression) assert(Expression)
#define CHECK_NO_ENTRY() CHECK(false)

#else

#define CHECK(Expression)
#define CHECK_NO_ENTRY()

#endif // DO_CHECKS