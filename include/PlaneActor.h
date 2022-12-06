#pragma once

#include "Actor.h"

class MeshComponent;

class PlaneActor : public Actor {
public:
	PlaneActor(const std::string& name,
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f },
		DirectX::XMFLOAT4 rot = { 0.0f, 0.0f, 0.0f, 1.0f },
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f });
	PlaneActor(const std::string& name, const Transform& trans);
	virtual ~PlaneActor();

protected:
	virtual bool OnInitialzing() override;

	virtual bool ProcessActorInput(const InputState& input) override;
	virtual bool UpdateActor(float delta) override;

private:
	MeshComponent* mMeshComp;
};