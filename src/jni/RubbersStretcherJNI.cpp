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
#include "rubbers/RubbersStretcher.h"

#include "system/Allocators.h"
#include <jni.h>

using namespace Rubbers;
extern "C" {

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    dispose
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_dispose
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    reset
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_reset
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setTimeRatio
 * Signature: (D)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setTimeRatio
  (JNIEnv *, jobject, jdouble);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setPitchScale
 * Signature: (D)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setPitchScale
  (JNIEnv *, jobject, jdouble);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    getChannelCount
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_getChannelCount
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    getTimeRatio
 * Signature: ()D
 */
JNIEXPORT jdouble JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_getTimeRatio
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    getPitchScale
 * Signature: ()D
 */
JNIEXPORT jdouble JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_getPitchScale
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    getLatency
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_getLatency
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setTransientsOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setTransientsOption
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setDetectorOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setDetectorOption
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setPhaseOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setPhaseOption
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setFormantOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setFormantOption
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setPitchOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setPitchOption
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setExpectedInputDuration
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setExpectedInputDuration
  (JNIEnv *, jobject, jlong);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    setMaxProcessSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_setMaxProcessSize
  (JNIEnv *, jobject, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    getSamplesRequired
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_getSamplesRequired
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    study
 * Signature: ([[FZ)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_study
  (JNIEnv *, jobject, jobjectArray, jint, jint, jboolean);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    process
 * Signature: ([[FZ)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_process
  (JNIEnv *, jobject, jobjectArray, jint, jint, jboolean);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    available
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_available
  (JNIEnv *, jobject);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    retrieve
 * Signature: (I)[[F
 */
JNIEXPORT jint JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_retrieve
  (JNIEnv *, jobject, jobjectArray, jint, jint);

/*
 * Class:     com_breakfastquay_rubbers_RubbersStretcher
 * Method:    initialise
 * Signature: (IIIDD)V
 */
JNIEXPORT void JNICALL Java_com_breakfastquay_rubbers_RubbersStretcher_initialise
  (JNIEnv *, jobject, jint, jint, jint, jdouble, jdouble);

}

RubbersStretcher *
getStretcher(JNIEnv *env, jobject obj)
{
    auto c = env->GetObjectClass(obj);
    auto fid = env->GetFieldID(c, "handle", "J");
    auto handle = env->GetLongField(obj, fid);
    return (RubbersStretcher *)handle;
}

void
setStretcher(JNIEnv *env, jobject obj, RubbersStretcher *stretcher)
{
    auto c = env->GetObjectClass(obj);
    auto fid = env->GetFieldID(c, "handle", "J");
    auto handle = (jlong)stretcher;
    env->SetLongField(obj, fid, handle);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_initialise(
        JNIEnv *env, 
        jobject obj, 
        jint sampleRate, 
        jint channels, 
        jint options, 
        jdouble initialTimeRatio, 
        jdouble initialPitchScale
        )
{
    setStretcher(env, obj, new RubbersStretcher
                 (sampleRate, channels, options, initialTimeRatio, initialPitchScale));
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_dispose(JNIEnv *env, jobject obj)
{
    delete getStretcher(env, obj);
    setStretcher(env, obj, 0);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_reset(JNIEnv *env, jobject obj)
{
    getStretcher(env, obj)->reset();
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setTimeRatio(JNIEnv *env, jobject obj, jdouble ratio)
{
    getStretcher(env, obj)->setTimeRatio(ratio);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setPitchScale(JNIEnv *env, jobject obj, jdouble scale)
{
    getStretcher(env, obj)->setPitchScale(scale);
}

JNIEXPORT jint JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_getChannelCount(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->getChannelCount();
}

JNIEXPORT jdouble JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_getTimeRatio(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->getTimeRatio();
}

JNIEXPORT jdouble JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_getPitchScale(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->getPitchScale();
}

JNIEXPORT jint JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_getLatency(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->getLatency();
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setTransientsOption(JNIEnv *env, jobject obj, jint options)
{
    getStretcher(env, obj)->setTransientsOption(options);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setDetectorOption(JNIEnv *env, jobject obj, jint options)
{
    getStretcher(env, obj)->setDetectorOption(options);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setPhaseOption(JNIEnv *env, jobject obj, jint options)
{
    getStretcher(env, obj)->setPhaseOption(options);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setFormantOption(JNIEnv *env, jobject obj, jint options)
{
    getStretcher(env, obj)->setFormantOption(options);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setPitchOption(JNIEnv *env, jobject obj, jint options)
{
    getStretcher(env, obj)->setPitchOption(options);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setExpectedInputDuration(JNIEnv *env, jobject obj, jlong duration)
{
    getStretcher(env, obj)->setExpectedInputDuration(duration);
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_setMaxProcessSize(JNIEnv *env, jobject obj, jint size)
{
    getStretcher(env, obj)->setMaxProcessSize(size);
}

JNIEXPORT jint JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_getSamplesRequired(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->getSamplesRequired();
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_study(JNIEnv *env, jobject obj, jobjectArray data, jint offset, jint n, jboolean flush)
{
    auto channels = env->GetArrayLength(data);
    auto arr = allocate<float *>(channels);
    auto input = allocate<float *>(channels);
    for (auto c = 0; c < channels; ++c) {
        jfloatArray cdata = (jfloatArray)env->GetObjectArrayElement(data, c);
        arr[c] = env->GetFloatArrayElements(cdata, 0);
        input[c] = arr[c] + offset;
    }
    getStretcher(env, obj)->study(input, n, flush);
    for (auto c = 0; c < channels; ++c) {
        jfloatArray cdata = (jfloatArray)env->GetObjectArrayElement(data, c);
        env->ReleaseFloatArrayElements(cdata, arr[c], 0);
    }
}

JNIEXPORT void JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_process(JNIEnv *env, jobject obj, jobjectArray data, jint offset, jint n, jboolean flush)
{
    auto channels = env->GetArrayLength(data);
    auto arr = allocate<float *>(channels);
    auto input = allocate<float *>(channels);
    for (auto c = 0; c < channels; ++c) {
        jfloatArray cdata = (jfloatArray)env->GetObjectArrayElement(data, c);
        arr[c] = env->GetFloatArrayElements(cdata, 0);
        input[c] = arr[c] + offset;
    }
    getStretcher(env, obj)->process(input, n, flush);
    for (auto c = 0; c < channels; ++c) {
        jfloatArray cdata = (jfloatArray)env->GetObjectArrayElement(data, c);
        env->ReleaseFloatArrayElements(cdata, arr[c], 0);
    }
    deallocate(input);
    deallocate(arr);
}

JNIEXPORT jint JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_available(JNIEnv *env, jobject obj)
{
    return getStretcher(env, obj)->available();
}

JNIEXPORT jint JNICALL
Java_com_breakfastquay_rubbers_RubbersStretcher_retrieve(JNIEnv *env, jobject obj, jobjectArray output, jint offset, jint n)
{
    auto stretcher = getStretcher(env, obj);
    auto channels = stretcher->getChannelCount();
    
    auto outbuf = allocate_channels<float>(channels, n);
    auto retrieved = stretcher->retrieve(outbuf, n);

    for (auto c = size_t{0}; c < channels; ++c) {
        jfloatArray cdata = (jfloatArray)env->GetObjectArrayElement(output, c);
        env->SetFloatArrayRegion(cdata, offset, retrieved, outbuf[c]);
    }
    
    deallocate_channels(outbuf, channels);
    return retrieved;
}

