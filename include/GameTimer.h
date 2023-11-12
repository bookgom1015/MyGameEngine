#pragma once

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
	float TotalTime() const; // in seconds
	float DeltaTime() const; // in seconds

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.

	float GetLimitFrameRate() const;
	void SetLimitFrameRate(LimitFrameRate type);

private:
	LimitFrameRate mLimitFrameRate;

	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;
};