#include <windows.h>
#include <assert.h>

#define M_STR_HELPER(x) #x
#define M_STR(x) M_STR_HELPER(x)
#define ensure_internal(Category, expression) \
       (\
              (!!(expression)) || ([](){ \
                     static bool alreadyCalled = false; \
                     if (!alreadyCalled) \
                     { \
                           alreadyCalled = true; \
                           MessageBox(nullptr, TEXT(#expression) TEXT("\r\n") _CRT_WIDE(__FILE__) TEXT(", (line ") M_STR(__LINE__) TEXT(")"), TEXT(#Category), MB_OK); \
                     }}(), false)\
       )
#define ensure(expression) ensure_internal(Log, expression)

#define check_internal(Category, expression) \
       (\
              (!!(expression)) || (MessageBox(nullptr, TEXT(#expression) TEXT("\r\n") _CRT_WIDE(__FILE__) TEXT(", (line ") M_STR(__LINE__) TEXT(")"), TEXT(#Category), MB_OK), false)\
       )
#define check(expression) check_internal(Log, expression)