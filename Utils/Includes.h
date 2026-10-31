#pragma once
#include "windows.h"
#include <process.h>
#include <iostream>
#include "xor.h"
using namespace std;
#pragma once
#include <vector>
#include <algorithm>
#include <xcharconv.h>


typedef unsigned __int64 ull;
typedef unsigned char   uint8;
#define _BYTE  uint8
typedef ull             uint64;
#define BYTEn(x, n)   (*((_BYTE*)&(x)+n))
#define BYTE6(x)   BYTEn(x,  6)
#define BYTE1(x)   BYTEn(x,  1)
#define _QWORD uint64

#define SAFE_PTR(ptr) (!IsBadReadPtr(ptr, sizeof(ptr)) && (ptr) != nullptr)
#define SAFE_PTR_SIZE(ptr, size) (!IsBadReadPtr(ptr, size) && (ptr) != nullptr)

#define FLOAT_NON_FRACTIONAL    8388608.0f
#define DegreesToRadians(deg) ((deg) * (3.14159265358979323846f / 180.0f))
