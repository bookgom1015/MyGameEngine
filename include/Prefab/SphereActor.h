#pragma once

#include "Common/Actor/Actor.h"

class MeshComponent;

class SphereActor : public Actor {
public:
	SphereActor(const std::string& name,
		DirectX::XMFLOAT3 pos   = { 0.f, 0.f, 0.f },
		DirectX::XMFLOAT4 rot   = { 0.f, 0.f, 0.f, 1.f },
		DirectX::XMFLOAT3 scale = { 1.f, 1.f, 1.f });
	SphereActor(const std::string& name, const Transform& trans);
	virtual ~SphereActor();

protected:
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

private:
	MeshComponent* mMeshComp;
};