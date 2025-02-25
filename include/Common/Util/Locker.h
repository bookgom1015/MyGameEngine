#pragma once

#include <mutex>

template <typename T>
class Locker {
public:
	std::unique_lock<std::mutex> TakeOut(T*& ptr);

	void PutIn(T*& ptr);

private:
	T* mValuable;
	std::mutex mMutex;
};

#include "Locker.inl"