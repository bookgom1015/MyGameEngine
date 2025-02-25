#pragma once

#include "Common/Actor/Actor.h"

class MeshComponent;

class BoxActor : public Actor {
public:
	BoxActor(
		const std::string& name,
		DirectX::XMFLOAT3 pos   = { 0.f, 0.f, 0.f },
		DirectX::XMFLOAT4 rot   = { 0.f, 0.f, 0.f, 1.f },
		DirectX::XMFLOAT3 scale = { 1.f, 1.f, 1.f });
	BoxActor(const std::string& name, const Transform& trans);
	virtual ~BoxActor();

protected:
	virtual BOOL OnInitialzing() override;

	virtual BOOL ProcessActorInput(const InputState& input) override;
	virtual BOOL UpdateActor(FLOAT delta) override;

private:
	MeshComponent* mMeshComp;
};