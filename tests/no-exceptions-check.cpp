#include <quadtree-index.h>

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#error Expected compiler exception handling to be disabled
#endif
#if defined(_MSVC_STL_VERSION) && _HAS_EXCEPTIONS
#error Expected MSVC STL exception support to be disabled
#endif
