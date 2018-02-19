// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

// Headers for CppUnitTest
#include "CppUnitTest.h"

#include "../Dictionary/Dictionary.h"

#ifndef ASSERT
#define ASSERT(Condition)   \
    if (!(Condition)) {     \
        __debugbreak();     \
    }
#endif

#include "../Dictionary/HistogramInline.h"

// vim:set ts=8 sw=4 sts=4 tw=80 expandtab                                     :
