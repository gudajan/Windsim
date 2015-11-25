#ifndef AXES_ACTOR_H
#define AXES_ACTOR_H

#include "actor.h"
#include "axes.h"

class AxesActor : public Actor
{
public:
	AxesActor(Axes& axes);
	~AxesActor();

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection) override;

private:
	Axes& m_axes;
};
#endif