#pragma once

#include "Actor.h"

class MeshComponent;

class RotatingMonkey : public Actor {
public:
	RotatingMonkey(const std::string& name,	float speed,
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT4 rot = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });
	RotatingMonkey(const std::string& name, const Transform& trans);
	virtual ~RotatingMonkey();

protected:
	virtual bool OnInitialzing() override;

	virtual bool ProcessActorInput(const InputState& input) override;
	virtual bool UpdateActor(float delta) override;

private:
	MeshComponent* mMeshComp;

	float mSpeed;
};