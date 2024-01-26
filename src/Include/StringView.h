#ifndef PASCAL_STRING_VIEW_H
#define PASCAL_STRING_VIEW_H


#include "Common.h"

struct StringView 
{
    const U8 *Str;
    USize Len;
};


#define STRVIEW_FMT "%.*s"
#define STRVIEW_FMT_ARG(StrView) (int)(StrView).Len, (StrView).Str

#define STRVIEW_INIT(pU8Str, USizeLen) (StringView) {.Str = pU8Str, .Len = USizeLen}
#define STRVIEW_INIT_CSTR(CStr, USizeLen) (StringView) {.Str = (const U8 *)(CStr), .Len = USizeLen}
#define STRVIEW_INIT_STRLIT(Lit) (static StringView) {.Str = Lit, .Len = sizeof Lit - 1}

#endif /* PASCAL_STRING_VIEW_H */

