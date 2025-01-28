#ifndef __CAMERA_INL__
#define __CAMERA_INL__

FLOAT Camera::FovY() const {
	return mFovY;
}

FLOAT Camera::NearZ() const {
	return mNearZ;
}

FLOAT Camera::FarZ() const {
	return mFarZ;
}

#endif // __CAMERA_INL__