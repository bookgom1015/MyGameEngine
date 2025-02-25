#ifndef __LOCKER_INL__
#define __LOCKER_INL__

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

#endif // __LOCKER_INL__