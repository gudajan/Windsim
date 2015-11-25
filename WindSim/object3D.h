#ifndef OBJECT_3D_H
#define OBJECT_3D_H

#include <DirectXMath.h>
#include <Windows.h>

#include <vector>

class ID3D11Buffer;
class ID3D11Device;
class ID3D11DeviceContext;

class Object3D
{
public:
	Object3D();
	Object3D(Object3D&& other);
	virtual ~Object3D();

	virtual HRESULT create(ID3D11Device* device, bool clearClientBuffers = false);
	virtual void release();

	// Depends on shader variables -> must be reimplemented
	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& world, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) = 0;

	// Computes intersection position in object space
	// Returns true if intersection found, otherwise false
	// Assumes data is a triangle list (not applicable for lines or points)
	virtual bool intersect(DirectX::XMFLOAT3& origin, DirectX::XMFLOAT3& direction, DirectX::XMFLOAT3& intersection) const;

protected:
	ID3D11Buffer* m_vertexBuffer;
	ID3D11Buffer* m_indexBuffer;
	uint32_t m_numIndices;

	std::vector<float> m_vertexData;
	std::vector<uint32_t> m_indexData;

};

#endif