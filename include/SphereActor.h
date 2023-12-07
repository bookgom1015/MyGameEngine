#pragma once

#include "Actor.h"

class MeshComponent;

class SphereActor : public Actor {
public:
	SphereActor(const std::string& name,
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT4 rot = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });
	SphereActor(const std::string& name, const Transform& trans);
	virtual ~SphereActor();

protected:
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

private:
	MeshComponent* mMeshComp;
};