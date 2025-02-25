#pragma once

#include "Common/Actor/Actor.h"

class MeshComponent;

class RotatingMonkey : public Actor {
public:
	RotatingMonkey(
		const std::string& name, FLOAT speed,
		DirectX::XMFLOAT3 pos   = { 0.f, 0.f, 0.f },
		DirectX::XMFLOAT4 rot   = { 0.f, 0.f, 0.f, 1.f },
		DirectX::XMFLOAT3 scale = { 1.f, 1.f, 1.f });
	RotatingMonkey(const std::string& name, const Transform& trans);
	virtual ~RotatingMonkey();

protected:
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

private:
	MeshComponent* mMeshComp;

	FLOAT mSpeed;
};