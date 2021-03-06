/*! @file Xorwow.c.h
*  @brief Code for the Xorwow generator common to the host and device
*/
#pragma once
#ifndef PRIVATE_XORWOW_CH
#define PRIVATE_XORWOW_CH

#define Xorwow_M1 4294967087            
#define Xorwow_M2 4294944443             

#define XORWOW_NORM_double 2.328306549295727688e-10
#define XORWOW_NORM_float  2.3283064e-10

#include <hc_math.hpp>

/*! @brief Advance the rng one step and returns z such that 1 <= z <= Xorwow_M1
*/
static unsigned int hcrngXorwowNextState(hcrngXorwowStreamState* currentState) [[hc, cpu]]
{
    unsigned long t;
    t = (currentState->v[0] ^ (currentState->v[0] >> 2));
    currentState->v[0] = currentState->v[1];
    currentState->v[1] = currentState->v[2];
    currentState->v[2] = currentState->v[3];
    currentState->v[3] = currentState->v[4];
    currentState->v[4] = (currentState->v[4] ^ (currentState->v[4] << 4)) ^ (t ^ (t << 1));
    currentState->d += 362437;
    return currentState->d + currentState->v[4];
}



// We use an underscore on the r.h.s. to avoid potential recursion with certain
// preprocessors.
#define IMPLEMENT_GENERATE_FOR_TYPE(fptype) \
	\
	unsigned int hcrngXorwowRandomUnsignedInteger_##fptype(hcrngXorwowStream* stream) [[hc, cpu]] { \
	    return hcrngXorwowNextState(&stream->current); \
	} \
	\
        fptype hcrngXorwowRandomN_##fptype(hcrngXorwowStream* stream1, hcrngXorwowStream* stream2, fptype mu, fptype sigma) [[hc, cpu]] { \
            static fptype z0, z1, i;\
            i++;\
            const fptype two_pi = 2.0 * 3.14159265358979323846;\
            static bool generate;\
            generate =! generate;\
            if (!generate) return z1 * sigma +mu;\
            fptype u1, u2;\
            u1 = hcrngXorwowRandomUnsignedInteger_##fptype(stream1) * XORWOW_NORM_##fptype;\
            u2 = hcrngXorwowRandomUnsignedInteger_##fptype(stream2) * XORWOW_NORM_##fptype;\
            z0 = sqrt(-2.0 * log((float)u1)) * cos(two_pi * (float)u2);\
            z1 = sqrt(-2.0 * log((float)u1)) * sin(two_pi * (float)u2);\
	    return z0 * sigma + mu; \
	} \
	\
        int hcrngXorwowRandomInteger_##fptype(hcrngXorwowStream* stream,  int i, int j) [[hc, cpu]] { \
	    return i + (int)((j - i + 1) * hcrngXorwowRandomUnsignedInteger_##fptype(stream)); \
	} \
	\
        fptype hcrngXorwowRandomU01_##fptype(hcrngXorwowStream* stream) [[hc, cpu]] { \
            return hcrngXorwowRandomUnsignedInteger_##fptype(stream) * XORWOW_NORM_##fptype; \
        }

// On the host, implement everything.
// On the device, implement only what is required to avoid hcuttering memory.
#if defined(HCRNG_SINGLE_PRECISION)  
IMPLEMENT_GENERATE_FOR_TYPE(float)
#endif
#if !defined(HCRNG_SINGLE_PRECISION) 
IMPLEMENT_GENERATE_FOR_TYPE(double)
#endif

// Clean up macros, especially to avoid polluting device code.
#undef IMPLEMENT_GENERATE_FOR_TYPE

#endif // PRIVATE_Xorwow_CH
