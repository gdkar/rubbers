#pragma once

#include <complex>
#include <ctgmath>
#include <cmath>
#include <valarray>
#include <algorithm>
#include <memory>
#include "system/VectorOps.h"
#include "system/VectorOpsComplex.h"

template<typename T>
inline constexpr T roundup ( T x )
{
  x--;x|=x>>1;x|=x>>2;x|=x>>4;
  x|=(x>>((sizeof(x)>1)?8:0));
  x|=(x>>((sizeof(x)>2)?16:0));
  x|=(x>>((sizeof(x)>4)?32:0));
  return x+1;
}
