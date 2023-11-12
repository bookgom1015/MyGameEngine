#include "GameTimer.h"

#include <Windows.h>

namespace {
	const float FrameTime30f = 1.0f / 30.0f;
	const float FrameTime60f = 1.0f / 60.0f;
	const float FrameTime120f = 1.0f / 120.0f;
	const float FrameTime144f = 1.0f / 144.0f;
	const float FrameTime244f = 1.0f / 244.0f;
}

GameTimer::GameTimer()
	: mLimitFrameRate(LimitFrameRate::E_LimitFrameRateNone),
	mSecondsPerCount(0.0),
	mDeltaTime(-1.0),
	mBaseTime(0),
	mPausedTime(0),
	mPrevTime(0),
	mCurrTime(0),
	mStopped(false) {
	__int64 countsPerSec;
	QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&countsPerSec));
	mSecondsPerCount = 1.0 / static_cast<double>(countsPerSec);
}

// Returns the total time elapsed since Reset() was called, NOT counting any
// time when the clock is stopped.
float GameTimer::TotalTime() const {
	// If we are stopped, do not count the time that has passed since we stopped.
	// Moreover, if we previously already had a pause, the distance 
	// mStopTime - mBaseTime includes paused time, which we do not want to count.
	// To correct this, we can subtract the paused time from mStopTime:  
	//
	//                     |<--paused time-->|
	// ----*---------------*-----------------*------------*------------*------> time
	//  mBaseTime       mStopTime        startTime     mStopTime    mCurrTime
	if (mStopped)
		return static_cast<float>(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
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
		return static_cast<float>(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
}

float GameTimer::DeltaTime() const {
	return static_cast<float>(mDeltaTime);
}

void GameTimer::Reset() {
	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
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
		mStopped = false;
	}
}

void GameTimer::Stop() {
	if (!mStopped) {
		__int64 currTime;
		QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));

		mStopTime = currTime;
		mStopped = true;
	}
}

void GameTimer::Tick() {
	if (mStopped) {
		mDeltaTime = 0.0;
		return;
	}

	__int64 currTime;
	QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currTime));
	mCurrTime = currTime;

	double delta = (mCurrTime - mPrevTime)* mSecondsPerCount;

	if (delta >= GetLimitFrameRate()) {
		// Time difference between this frame and the previous.
		mDeltaTime = delta;

		// Prepare for next frame.
		mPrevTime = mCurrTime;
	}	

	// Force nonnegative.  The DXSDK's CDXUTTimer mentions that if the 
	// processor goes into a power save mode or we get shuffled to another
	// processor, then mDeltaTime can be negative.
	if (delta < 0.0)
		mDeltaTime = 0.0;
}

float GameTimer::GetLimitFrameRate() const {
	switch (mLimitFrameRate) {
	case E_LimitFrameRateNone:
		return 0.0f;
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
		return 0.0f;
	}
}

void GameTimer::SetLimitFrameRate(LimitFrameRate type) {
	mLimitFrameRate = type;
}