#pragma once

#include <d3d12.h>
#include <DirectXCollision.h>

#include "MathHelper.h"

struct MeshGeometry;
struct MaterialData;

extern const int gNumFrameResources;

struct RenderItem {
	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	int ObjCBIndex = -1;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in thw world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 PrevWolrd = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource. Thus, when we modify object data we should set
	// NumFrameDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources << 1;

	MaterialData* Material = nullptr;
	MeshGeometry* Geometry = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	DirectX::BoundingBox AABB;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	bool Pickable = true;
};