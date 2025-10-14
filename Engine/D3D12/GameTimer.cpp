#include <windows.h>
#include "GameTimer.h"

GameTimer::GameTimer() : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseTime(0),
	mPausedTime(0), mPrevTime(0), mCurrTime(0), mStopped(false)
{
	__int64 CountsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&CountsPerSec);
	mSecondsPerCount = 1.0 / (double)CountsPerSec;

}

float GameTimer::TotalTime() const
{
	if (mStopped)
	{
		return (float)(((mStopTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
	else
	{
		return (float)(((mCurrTime - mPausedTime) - mBaseTime) * mSecondsPerCount);
	}
}

float GameTimer::DeltaTime() const
{
	return (float)mDeltaTime;
}

void GameTimer::Reset()
{
	__int64 currTime;
	QueryPerformanceFrequency((LARGE_INTEGER*)&currTime);

	mBaseTime = currTime;
	mPrevTime = currTime;
	mStopTime = 0;
	mStopped = false;
}

void GameTimer::Start()
{
	__int64 startTime;
	QueryPerformanceFrequency((LARGE_INTEGER*)&startTime);

	if (mStopped)
	{
		mPausedTime += (startTime - mStopTime);

		mPrevTime = startTime;

		mStopTime = 0;
		mStopped = false;
	}
}

void GameTimer::Stop()
{
	if (!mStopped)
	{
		__int64 currTime;
		QueryPerformanceFrequency((LARGE_INTEGER*)&currTime);

		mStopped = currTime;
		mStopped = true;
	}
}

void GameTimer::Tick()
{
	if (mStopped)
	{
		mDeltaTime = 0.0;
		return;
	}

	__int64 CurTime;
	QueryPerformanceFrequency((LARGE_INTEGER*)&CurTime);
	mCurrTime = CurTime;

	mDeltaTime = (mCurrTime - mPrevTime) * mSecondsPerCount;

	mPrevTime = mCurrTime;

	if (mDeltaTime < 0.0)
	{
		mDeltaTime = 0.0;
	}
}
