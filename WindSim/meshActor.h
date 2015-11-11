#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"

class Mesh;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh& mesh);
	virtual ~MeshActor();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection);

private:
	Mesh& m_mesh;
};


#endif