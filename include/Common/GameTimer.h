#pragma once

#include <Windows.h>

class GameTimer {
public:
	enum LimitFrameRate {
		E_LimitFrameRateNone,
		E_LimitFrameRate30f,
		E_LimitFrameRate60f,
		E_LimitFrameRate120f,
		E_LimitFrameRate144f,
		E_LimitFrameRate244f
	};

public:
	GameTimer();

public:
	FLOAT TotalTime() const; // in seconds
	FLOAT DeltaTime() const; // in seconds

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

	FLOAT GetLimitFrameRate() const;
	void SetLimitFrameRate(LimitFrameRate type);

private:
	LimitFrameRate mLimitFrameRate = LimitFrameRate::E_LimitFrameRateNone;

	DOUBLE mSecondsPerCount =  0.;
	DOUBLE mDeltaTime		= -1.;

	__int64 mBaseTime		= 0;
	__int64 mPausedTime		= 0;
	__int64 mStopTime		= 0;
	__int64 mPrevTime		= 0;
	__int64 mCurrTime		= 0;

	BOOL mStopped			= FALSE;
};