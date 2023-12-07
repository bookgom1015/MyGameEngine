#ifndef __ACTOR_INL__
#define __ACTOR_INL__

constexpr BOOL Actor::IsInitialized() const {
	return bInitialized;
}

constexpr BOOL Actor::IsDead() const {
	return bDead;
}

#endif // __ACTOR_INL__