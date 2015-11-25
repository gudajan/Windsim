#include "skyActor.h"

using namespace DirectX;

SkyActor::SkyActor(Sky& sky)
	: Actor(ObjectType::Sky),
	m_sky(sky)
{
}

SkyActor::~SkyActor()
{
}

void SkyActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (m_render)
	{
		// Do not use world matrix -> Positon, rotation, scaling is ignored
		XMFLOAT4X4 id;
		XMStoreFloat4x4(&id, XMMatrixIdentity());
		m_sky.render(device, context, id, view, projection);
	}
}