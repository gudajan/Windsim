#ifndef OBJECT_3D_H
#define OBJECT_3D_H

#include <DirectXMath.h>
#include <Windows.h>

#include <vector>

#include "common.h" // Vertex, Triangle

class ID3D11Buffer;
class ID3D11Device;
class ID3D11DeviceContext;

class Object3D
{
public:
	Object3D();
	Object3D(Object3D&& other);
	virtual ~Object3D();

	virtual HRESULT create(ID3D11Device* device, bool clearClientBuffers = true);
	virtual void release();

	// Depends on shader variables -> must be reimplemented
	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) = 0;

protected:
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	uint32_t m_numIndices;

	std::vector<Vertex> m_vertexData;
	std::vector<Triangle> m_indexData;

};

#endif