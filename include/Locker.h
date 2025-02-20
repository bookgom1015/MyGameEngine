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

template <typename T>
std::unique_lock<std::mutex> Locker<T>::TakeOut(T*& ptr) {
	std::unique_lock<std::mutex> lock(mMutex);
	ptr = mValuable;
	return lock;
}

template <typename T>
void Locker<T>::PutIn(T*& ptr) {
	std::unique_lock<std::mutex> lock(mMutex);
	mValuable = ptr;
}