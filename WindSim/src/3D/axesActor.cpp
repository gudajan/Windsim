#include "axesActor.h"

using namespace DirectX;

AxesActor::AxesActor(Axes& axes, int id)
	: Actor(ObjectType::Axes, id),
	m_axes(axes)
{
}

AxesActor* AxesActor::clone()
{
	return new AxesActor(*this);
}

void AxesActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_render)
	{
		m_axes.render(device, context, getWorld(), view, projection, elapsedTime);
	}
}