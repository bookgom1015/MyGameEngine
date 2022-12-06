#ifndef __ACTOR_INL__
#define __ACTOR_INL__

constexpr bool Actor::IsInitialized() const {
	return bInitialized;
}

constexpr bool Actor::IsDead() const {
	return bDead;
}

#endif // __ACTOR_INL__