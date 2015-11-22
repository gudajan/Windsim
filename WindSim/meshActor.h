#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"
#include <DirectXPackedVector.h>

class Mesh;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh& mesh);
	~MeshActor();

	void setFlatShading(bool flat, DirectX::PackedVector::XMCOLOR col) { m_flatShading = flat; m_color = col; };

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

private:
	Mesh& m_mesh;

	bool m_flatShading;
	DirectX::PackedVector::XMCOLOR m_color;
};


#endif