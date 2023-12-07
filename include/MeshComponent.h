#pragma once

#include "Component.h"
#include "Mesh.h"

#include <wrl.h>

class MeshComponent : public Component {
public:
	MeshComponent(Actor* owner);
	virtual ~MeshComponent();

public:
	virtual BOOL ProcessInput(const InputState& input) override;
	virtual BOOL Update(FLOAT delta) override;
	virtual BOOL OnUpdateWorldTransform() override;

	BOOL LoadMesh(const std::string& file);

	void SetVisibility(BOOL visible);
	void SetPickable(BOOL pickable);

private:
	void* mModel;
};