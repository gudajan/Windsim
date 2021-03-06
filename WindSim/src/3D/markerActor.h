#ifndef SKY_ACTOR_H
#define SKY_ACTOR_H

#include "actor.h"
#include "marker.h"


class MarkerActor : public Actor
{
public:
	MarkerActor(Marker& marker, int id, const std::string& name);

	MarkerActor* clone() override;
	void render(ID3D11Device* device, ID3D11DeviceContext* context, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& projection, double elapsedTime) override;

	Marker* getObject() override { return &m_marker; };

	void setXAxisRendered(bool render) { m_renderX = render; };
	void setYAxisRendered(bool render) { m_renderY = render; };
	void setZAxisRendered(bool render) { m_renderZ = render; };
	void setLarge(bool large) { m_renderLarge = large; };

private:
	Marker& m_marker;

	// Indicate which axes of the markers are currently rendered
	bool m_renderX;
	bool m_renderY;
	bool m_renderZ;

	bool m_renderLarge; // Indicates if the axes are drawn until infinity
};
#endif