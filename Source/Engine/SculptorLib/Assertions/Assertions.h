#pragma once


#if DO_CHECKS

#define SPT_CHECK(Expression) assert(Expression)
#define SPT_CHECK_NO_ENTRY() SPT_CHECK(false)

#else

#define SPT_CHECK(Expression)
#define SPT_CHECK_NO_ENTRY()

#endif // DO_CHECKS