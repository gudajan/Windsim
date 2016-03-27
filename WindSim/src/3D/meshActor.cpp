#include "meshActor.h"
#include "mesh3D.h"
#include "marker.h"
#include "settings.h"

using namespace DirectX;

MeshActor::MeshActor(Mesh3D& mesh, int id)
	: Actor(ObjectType::Mesh, id),
	m_marker(mesh.getRenderer()),
	m_mesh(mesh),
	m_dynamics(mesh),
	m_dynRenderWorld(),
	m_dynCalcWorld(),
	m_boundingBox(),
	m_flatShading(true),
	m_color(PackedVector::XMCOLOR(conf.mesh.dc.r / 255.0f, conf.mesh.dc.g / 255.0f, conf.mesh.dc.b / 255.0f, 1.0f)), // XMCOLOR constructor multiplies by 255.0f and packs color into one uint32_t
	m_voxelize(true),
	m_calcDynamics(true),
	m_simRunning(true),
	m_showAccelArrow(false),
	m_hovered(false),
	m_selected(false),
	m_modified(false)
{
	XMFLOAT3 center;
	XMFLOAT3 extends;
	mesh.getBoundingBox(center, extends);
	setBoundingBox(center, extends);

	XMStoreFloat4x4(&m_dynRenderWorld, XMMatrixIdentity());
	XMStoreFloat4x4(&m_dynCalcWorld, XMMatrixIdentity());

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
	if (m_calcDynamics && m_simRunning)
	{
		m_dynamics.render(device, context, m_rot, m_pos, view, projection, elapsedTime, m_showAccelArrow);

		// Calculate dynamic world matrix, including dynamic rotation
		// S(world) * T(-com) * R(dyn) * T(com) * RT(world)
		XMVECTOR com = XMLoadFloat3(&m_dynamics.getCenterOfMass());
		XMVectorSetW(com, 0.0f); // Vector, not a point
		XMVECTOR dynRenderRot = XMLoadFloat4(&m_dynamics.getRenderRotation());
		XMVECTOR dynCalcRot = XMLoadFloat4(&m_dynamics.getCalcRotation());
		XMVECTOR worldRot = XMLoadFloat4(&m_rot);

		XMMATRIX dynRenderWorld = XMMatrixScalingFromVector(XMLoadFloat3(&m_scale)); // Perform world space scaling first to avoid shearing and because com already contains world space scaling
		dynRenderWorld *= XMMatrixTranslationFromVector(-com); // Center of mass at origin to perform dynamic rotation arround it
		XMMATRIX dynCalcWorld = dynRenderWorld;
		dynRenderWorld *= XMMatrixRotationQuaternion(dynRenderRot); // Perform dynamic rotation
		dynCalcWorld *= XMMatrixRotationQuaternion(dynCalcRot); // Perform dynamic rotation
		XMMATRIX temp = XMMatrixTranslationFromVector(com); // Translate back
		temp *= XMMatrixRotationQuaternion(worldRot); // Transform world rotation
		temp *= XMMatrixTranslationFromVector(XMLoadFloat3(&m_pos)); // Transform world translation
		dynRenderWorld *= temp;
		dynCalcWorld *= temp;

		XMStoreFloat4x4(&m_dynRenderWorld, dynRenderWorld);
		XMStoreFloat4x4(&m_dynCalcWorld, dynCalcWorld);
	}

	if (m_render)
	{
		if (m_hovered)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.mesh.hc.r / 255.0f, conf.mesh.hc.g / 255.0f, conf.mesh.hc.b / 255.0f, 1.0f));
		else if (m_selected)
			m_mesh.setShaderVariables(m_flatShading, PackedVector::XMCOLOR(conf.gen.sc.r / 255.0f, conf.gen.sc.g / 255.0f, conf.gen.sc.b / 255.0f, 1.0f));
		else
			m_mesh.setShaderVariables(m_flatShading, m_color);


		if (!m_calcDynamics || (m_modified && !conf.dyn.showDynDuringMod))
			m_mesh.render(device, context, m_world, view, projection, elapsedTime);
		else
			m_mesh.render(device, context, m_dynRenderWorld, view, projection, elapsedTime);

		if (m_selected || m_hovered)
		{
			m_marker.setShaderVariables(true, true, true, false);
			m_marker.render(device, context, m_world, view, projection, elapsedTime);
		}
	}
}

void MeshActor::calculateDynamics(ID3D11Device* device, ID3D11DeviceContext* context, const XMFLOAT4X4& worldToVoxelTex, const XMUINT3& texResolution, const XMFLOAT3& voxelSize, ID3D11ShaderResourceView* velocityField, double elapsedTime)
{
	if (m_calcDynamics)
		if (conf.dyn.useDynWorldForCalc) // Pass the world matrix, which was used for the last grid update
			m_dynamics.calculate(device, context, m_dynCalcWorld, worldToVoxelTex, texResolution, voxelSize, velocityField, elapsedTime);
		else // Pass original world matrix
			m_dynamics.calculate(device, context, m_world, worldToVoxelTex, texResolution, voxelSize, velocityField, elapsedTime);
}

const XMFLOAT3 MeshActor::getAngularVelocity() const
{
	XMFLOAT3 angVel;
	XMStoreFloat3(&angVel, XMVector3Rotate(XMLoadFloat3(&m_dynamics.getAngularVelocity()), XMLoadFloat4(&m_rot)));
	return angVel;
}

const XMFLOAT3 MeshActor::getCenterOfMass() const
{
	XMFLOAT3 com;
	XMStoreFloat3(&com, XMVector3Rotate(XMLoadFloat3(&m_dynamics.getCenterOfMass()), XMLoadFloat4(&m_rot)));
	return com;
}

bool MeshActor::intersect(XMFLOAT3 origin, XMFLOAT3 direction, float& distance) const
{
	// Transform ray from world to object space
	XMVECTOR ori = XMLoadFloat3(&origin);
	XMVECTOR dir = XMLoadFloat3(&direction);
	dir = XMVectorSetW(dir, 0.0f);

	XMMATRIX world = XMLoadFloat4x4(&m_dynRenderWorld);

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
	m_dynamics.reset();
}
