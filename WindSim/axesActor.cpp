#include "axesActor.h"

using namespace DirectX;

AxesActor::AxesActor(Axes& axes)
	: Actor(ObjectType::Axes),
	m_axes(axes)
{
}

AxesActor::~AxesActor()
{
}

void AxesActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection)
{
	if (m_render)
	{
		m_axes.render(device, context, getWorld(), view, projection);
	}
}