#include "meshActor.h"
#include "mesh3D.h"
#include "marker.h"
#include "settings.h"

using namespace DirectX;

MeshActor::MeshActor(Mesh3D& mesh, int id)
	: Actor(ObjectType::Mesh, id),
	m_marker(mesh.getLogger()),
	m_mesh(mesh),
	m_dynamics(mesh),
	m_dynWorld(),
	m_boundingBox(),
	m_flatShading(true),
	m_color(PackedVector::XMCOLOR(conf.mesh.dc.r / 255.0f, conf.mesh.dc.g / 255.0f, conf.mesh.dc.b / 255.0f, 1.0f)), // XMCOLOR constructor multiplies by 255.0f and packs color into one uint32_t
	m_voxelize(true),
	m_calcDynamics(true),
	m_hovered(false),
	m_selected(false),
	m_modified(false)
{
	XMFLOAT3 center;
	XMFLOAT3 extends;
	mesh.getBoundingBox(center, extends);
	setBoundingBox(center, extends);

	XMStoreFloat4x4(&m_dynWorld, XMMatrixIdentity());

	updateInertiaTensor();
}

MeshActor* MeshActor::clone()
{
	return new MeshActor(*this);
}

void MeshActor::setBoundingBox(const XMFLOAT3& center, const XMFLOAT3& extends)
{
	m_boundingBox = BoundingBox(center, extends);
}

void MeshActor::create(ID3D11Device* device)
{
	m_marker.create(device, true);
	m_dynamics.create(device);
}

void MeshActor::release()
{
	m_marker.release();
	m_dynamics.release();
}

void MeshActor::render(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& view, const XMFLOAT4X4& projection, double elapsedTime)
{
	if (m_render)
	{
		if (m_hovered)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.mesh.hc.r / 255.0f, conf.mesh.hc.g / 255.0f, conf.mesh.hc.b / 255.0f, 1.0f));
		else if (m_selected)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.gen.sc.r / 255.0f, conf.gen.sc.g / 255.0f, conf.gen.sc.b / 255.0f, 1.0f));
		else
			m_mesh.setShaderVariables(m_flatShading, m_color);

		if (m_calcDynamics)
		{
			// Calculate dynamic world matrix, including dynamic rotation
			// S(world) * T(-com) * R(dyn) * T(com) * RT(world)
			XMVECTOR com = XMLoadFloat3(&m_dynamics.getCenterOfMass());
			XMVectorSetW(com, 0.0f); // Vector, not a point
			XMVECTOR dynRot = XMLoadFloat4(&m_dynamics.getRotation());
			XMVECTOR worldRot = XMLoadFloat4(&m_rot);

			XMMATRIX dynWorld = XMMatrixScalingFromVector(XMLoadFloat3(&m_scale)); // Perform world space scaling first to avoid shearing and because com already contains world space scaling
			dynWorld *= XMMatrixTranslationFromVector(-com); // Center of mass at origin to perform dynamic rotation arround it
			dynWorld *= XMMatrixRotationQuaternion(dynRot); // Perform dynamic rotation
			dynWorld *= XMMatrixTranslationFromVector(com); // Translate back
			dynWorld *= XMMatrixRotationQuaternion(worldRot); // Transform world rotation
			dynWorld *= XMMatrixTranslationFromVector(XMLoadFloat3(&m_pos)); // Transform world translation

			XMStoreFloat4x4(&m_dynWorld, dynWorld);

			if (m_modified && !conf.dyn.showDynDuringMod)
				m_mesh.render(device, context, m_world, view, projection, elapsedTime);
			else
				m_mesh.render(device, context, m_dynWorld, view, projection, elapsedTime);

		}
		else
		{
			m_mesh.render(device, context, m_world, view, projection, elapsedTime);
		}

		if (m_selected || m_hovered)
		{
			m_marker.setShaderVariables(true, true, true, false);
			m_marker.render(device, context, m_world, view, projection, elapsedTime);
		}
	}

	if (m_calcDynamics)
		m_dynamics.render(device, context, m_rot, m_pos, view, projection);
}

void MeshActor::calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& worldToVoxelTex, const XMUINT3& texResolution, ID3D11ShaderResourceView* velocityField, double elapsedTime)
{
	if (m_calcDynamics)
		// Current state: pass original world matrix instead of dynamic one, so the calculated torque is more appropriate/consitent
		// TODO: Update the voxel grid (and therefore the velcity field) dependently on the dynamic rotation so we get appropriate torque values if passing the dynamic world matrix here
		if (conf.dyn.useDynWorldForCalc)
			m_dynamics.calculate(device, context, m_dynWorld, worldToVoxelTex, texResolution, velocityField, elapsedTime);
		else
			m_dynamics.calculate(device, context, m_world, worldToVoxelTex, texResolution, velocityField, elapsedTime);
}

bool MeshActor::intersect(XMFLOAT3 origin, XMFLOAT3 direction, float& distance) const
{
	// Transform ray from world to object space
	XMVECTOR ori = XMLoadFloat3(&origin);
	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVectorSetW(dir, 0.0f);

	XMMATRIX world = XMLoadFloat4x4(&m_dynWorld);

	XMMATRIX invWorld = XMMatrixInverse(nullptr, world);
	if (XMMatrixIsInfinite(invWorld) || XMMatrixIsNaN(invWorld)) // Matrix not invertible or arithmetric errors occured
		return false;

	ori = XMVector3Transform(ori, invWorld);
	dir = XMVector4Transform(dir, invWorld);
	dir = XMVector3Normalize(dir);

	XMFLOAT3 o;
	XMFLOAT3 d;
	XMStoreFloat3(&o, ori);
	XMStoreFloat3(&d, dir);

	// Check if bounding box is intersected
	BoundingBox transformedBB;
	m_boundingBox.Transform(transformedBB, world);
	float dist = 0;
	if (!m_boundingBox.Intersects(ori, dir, dist))
		return false;

	if (!m_mesh.intersect(o, d, distance))
		return false;

	// Transform distance from object to world space (just scaling has influence on distance)
	XMVECTOR  distVec = XMVector3Transform(dir * distance, XMMatrixScalingFromVector(XMLoadFloat3(&m_scale)));
	XMStoreFloat(&distance, XMVector3Length(distVec));
	return true;
}


void MeshActor::updateInertiaTensor()
{
	XMFLOAT3X3 inertia;
	XMFLOAT3 com;
	m_mesh.calcMassProps(m_density, m_scale, inertia, com);
	m_dynamics.setInertia(inertia);
	m_dynamics.setCenterOfMass(com);
}
