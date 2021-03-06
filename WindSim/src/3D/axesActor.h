#ifndef AXES_ACTOR_H
#define AXES_ACTOR_H

#include "actor.h"
#include "axes.h"

class AxesActor : public Actor
{
public:
	AxesActor(Axes& axes, int id, const std::string& name);

	AxesActor* clone() override;

	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;

	Axes* getObject() override { return &m_axes; };

private:
	Axes& m_axes;
};
#endif