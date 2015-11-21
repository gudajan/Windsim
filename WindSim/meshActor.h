#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H

#include "actor.h"

class Mesh;

class MeshActor : public Actor
{
public:
	MeshActor(Mesh& mesh);
	~MeshActor();

	void setFlatShading(bool flat) { m_flatShading = flat; };

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

private:
	Mesh& m_mesh;

	bool m_flatShading;
};


#endif