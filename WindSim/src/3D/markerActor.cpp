#include "markerActor.h"
#include "marker.h"

using namespace DirectX;

MarkerActor::MarkerActor(Marker& marker, int id, const std::string& name)
	: Actor(ObjectType::Marker, id, name),
	m_marker(marker),
	m_renderX(true),
	m_renderY(true),
	m_renderZ(true),
	m_renderLarge(false)
{
}

MarkerActor* MarkerActor::clone()
{
	return new MarkerActor(*this);
}

void MarkerActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_render)
	{
		m_marker.setShaderVariables(m_renderX, m_renderY, m_renderZ, m_renderLarge);
		m_marker.render(device, context, m_world, view, projection, elapsedTime);
	}
}