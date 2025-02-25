#include "Common/GameTimer.h"

#include <Windows.h>

namespace {
	const FLOAT FrameTime30f  = 1.f / 30.f;
	const FLOAT FrameTime60f  = 1.f / 60.f;
	const FLOAT FrameTime120f = 1.f / 120.f;
	const FLOAT FrameTime144f = 1.f / 144.f;
	const FLOAT FrameTime244f = 1.f / 244.f;
}

GameTimer::GameTimer() {
	__int64 countsPerSec;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
	mSecondsPerCount = 1. / static_cast<double>(countsPerSec);
}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
FLOAT GameTimer::TotalTime() const {
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime
	if (mStopped)
		return static_cast<FLOAT>(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	// The distance mCurrTime - mBaseTime includes paused time,
	// which we do not want to count.  To correct this, we can subtract 
	// the paused time from mCurrTime:  
	//
	//  (mCurrTime - mPausedTime) - mBaseTime 
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mCurrTime	
	else
		return static_cast<FLOAT>(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
}

FLOAT GameTimer::DeltaTime() const {
	return static_cast<FLOAT>(mDeltaTime);
}

void GameTimer::Reset() {
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = FALSE;
}

void GameTimer::Start() {
	__int64 startTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&startTime));

	// Accumulate the time elapsed between stop and start pairs.
	//
	//                     |<-------d------->|
	// ----*---------------*-----------------*------------> time
	//  mBaseTime       mStopTime        startTime     

	if (mStopped) {
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;
		mStopTime = 0;
		mStopped = FALSE;
	}
}

void GameTimer::Stop() {
	if (!mStopped) {
		__int64 currTime;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

		mStopTime = currTime;
		mStopped = TRUE;
	}
}

void GameTimer::Tick() {
	if (mStopped) {
		mDeltaTime = 0.;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
	mCurrTime = currTime;

	const DOUBLE delta = (mCurrTime - mPrevTime) * mSecondsPerCount;

	if (delta >= GetLimitFrameRate()) {
		// Time difference between this frame and the previous.
		mDeltaTime = delta;

		// Prepare for next frame.
		mPrevTime = mCurrTime;
	}	

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if (delta < 0.)
		mDeltaTime = 0.;
}

FLOAT GameTimer::GetLimitFrameRate() const {
	switch (mLimitFrameRate) {
	case E_LimitFrameRateNone:
		return 0.f;
	case E_LimitFrameRate30f:
		return FrameTime30f;
	case E_LimitFrameRate60f:
		return FrameTime60f;
	case E_LimitFrameRate120f:
		return FrameTime120f;
	case E_LimitFrameRate144f:
		return FrameTime144f;
	case E_LimitFrameRate244f:
		return FrameTime244f;
	default:
		return 0.f;
	}
}

void GameTimer::SetLimitFrameRate(LimitFrameRate type) {
	mLimitFrameRate = type;
}