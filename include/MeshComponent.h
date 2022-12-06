#pragma once

#include "Component.h"
#include "Mesh.h"
#include <wrl.h>

class MeshComponent : public Component {
public:
	MeshComponent(Actor* owner);
	virtual ~MeshComponent();

public:
	virtual bool ProcessInput(const InputState& input) override;
	virtual bool Update(float delta) override;
	virtual bool OnUpdateWorldTransform() override;

	bool LoadMesh(const std::string& file);

	void SetVisibility(bool visible);

private:
	void* mModel;
};