#ifndef ACTOR_H
#define ACTOR_H

#include "common.h"
#include <DirectXMath.h>

class ID3D11Device;
class ID3D11DeviceContext;

class Actor
{
public:
	Actor(ObjectType type);
	virtual ~Actor();

	virtual void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) = 0;

	virtual bool intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, DirectX::XMFLOAT3& intersection) const { return false; };

	virtual void setPos(const DirectX::XMFLOAT3& pos);
	virtual void setRot(const DirectX::XMFLOAT4& rot);
	virtual void setScale(const DirectX::XMFLOAT3& scale);
	virtual void setRender(bool render);
	virtual void toggleRender();

	virtual const DirectX::XMFLOAT3& getPos() const;
	virtual const DirectX::XMFLOAT4& getRot() const;
	virtual const DirectX::XMFLOAT3& getScale() const;
	virtual DirectX::XMFLOAT4X4 getWorld() const;
	ObjectType getType() const { return m_type; };

protected:
	DirectX::XMFLOAT3 m_pos;
	DirectX::XMFLOAT4 m_rot;
	DirectX::XMFLOAT3 m_scale;
	bool m_render;
	ObjectType m_type;
};

#endif