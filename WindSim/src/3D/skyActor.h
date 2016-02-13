#ifndef SKY_ACTOR_H
#define SKY_ACTOR_H

#include "actor.h"
#include "sky.h"

class SkyActor : public Actor
{
public:
	SkyActor(Sky& sky, int id);

	SkyActor* clone() override;
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;

private:
	Sky& m_sky;
};
#endif