/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band Library
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2014 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for the
    Rubber Band Library obtained by agreement with the copyright
    holders, you may redistribute and/or modify it under the terms
    described in that licence.

    If you wish to distribute code using the Rubber Band Library
    under terms other than those of the GNU General Public License,
    you must obtain a valid commercial licence before doing so.
*/

#include "VectorOpsComplex.h"

#include "system/sysutils.h"

#include <cassert>

#if defined USE_POMMIER_MATHFUN
#if defined __ARMEL__ || defined __aarch64__
#include "pommier/neon_mathfun.h"
#else
#include "pommier/sse_mathfun.h"
#endif
#endif

namespace Rubbers {


#if defined USE_POMMIER_MATHFUN

#if defined __ARMEL__ || defined __aarch64__
typedef union {
  float f[4];
  int i[4];
  v4sf  v;
} V4SF;
#else
typedef ALIGN16_BEG union {
  float f[4];
  int i[4];
  v4sf  v;
} ALIGN16_END V4SF;
#endif

void
v_polar_to_cartesian_pommier(float *const real,
                             float *const imag,
                             const float *const mag,
                             const float *const phase,
                             const int count){
    int i = 0;

    for (int i = 0; i + 4 < count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        fmag = *(__m128*)(mag+i);
        fphase = *(__m128*)(phase+i);
	sincos_ps(fphase, &fim, &fre);
        fim = _mm_mul_ps(fim, fmag);
        fre = _mm_mul_ps(fre,fmag);
        *(__m128*)(real + i ) = fre;
        *(__m128*)(imag + i ) = fim;
    }
    while (i < count) {
        float re, im;
        c_phasor(&re, &im, phase[i]);
        real[i] = re * mag[i];
        imag[i] = im * mag[i];
        ++i;
    }
}    
void
v_polar_interleaved_to_cartesian_inplace_pommier(float *const srcdst,
                                                 const int count){
    int i;
    int idx = 0, tidx = 0;
    for (i = 0; i + 4 < count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        for (int j = 0; j < 3; ++j) {
            fmag[j] = srcdst[idx++];
            fphase[j] = srcdst[idx++];
        }
	sincos_ps(fphase, &fim, &fre);
        for (int j = 0; j < 3; ++j) {
            srcdst[tidx++] = fre[j] * fmag[j];
            srcdst[tidx++] = fim[j] * fmag[j];
        }
    }
    while (i < count) {
        float real, imag;
        float mag = srcdst[idx++];
        float phase = srcdst[idx++];
        c_phasor(&real, &imag, phase);
        srcdst[tidx++] = real * mag;
        srcdst[tidx++] = imag * mag;
        ++i;
    }
}    
void
v_polar_to_cartesian_interleaved_pommier(float *const dst,
                                         const float *const mag,
                                         const float *const phase,
                                         const int count){
    int i;
    for (i = 0; i + 4 <= count; i += 4) {
	__m128 fmag, fphase, fre, fim;
        fmag = *(__m128*)(mag+i);
        fphase = *(__m128*)(phase+i);
	sincos_ps(fphase, &fim, &fre);
        fim = _mm_mul_ps(fim, fmag);
        fre = _mm_mul_ps(fre,fmag);
        *(__m128*)(dst+(2*i))   = _mm_unpacklo_ps(fre,fim);
        *(__m128*)(dst+(2*i+4)) = _mm_unpackhi_ps(fre,fim);
    }
    while (i < count) {
        float real, imag;
        c_phasor(&real, &imag, phase[i]);
        dst[2*i+0] = real * mag[i];
        dst[2*i+1] = imag * mag[i];
        ++i;
    }
}    
#endif
}
