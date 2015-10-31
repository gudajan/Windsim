#ifndef ACTOR_H
#define ACTOR_H

#include "object3D.h"

class Actor
{
public:
	Actor(Object3D& object);
	virtual ~Actor();

	void render(ID3D11Device* device, ID3D11DeviceContext* context);

	void setPos(const DirectX::XMFLOAT3& pos);
	void setRot(const DirectX::XMFLOAT4& rot);
	void setScale(const DirectX::XMFLOAT3& scale);
	void setRender(bool render);
	void toggleRender();
	void setRenderBoundingBox(bool render);
	void toggleRenderBoundingBox();

	const DirectX::XMFLOAT3& getPos() const;
	const DirectX::XMFLOAT4& getRot() const;
	const DirectX::XMFLOAT3& getScale() const;
	DirectX::XMFLOAT4X4 getWorld() const;

protected:
	DirectX::XMFLOAT3 m_pos;
	DirectX::XMFLOAT4 m_rot;
	DirectX::XMFLOAT3 m_scale;
	bool m_render;

private:
	Object3D& m_obj;
};

#endif