#include "actor.h"

using namespace DirectX;

Actor::Actor(ObjectType type, int id)
	: m_pos({ 0.0, 0.0, 0.0 }),
	m_rot({ 0.0, 0.0, 0.0, 1.0 }),
	m_scale({ 1.0, 1.0, 1.0 }),
	m_world(),
	m_render(true),
	m_type(type),
	m_id(id)
{
	computeWorld();
}

bool Actor::intersect(DirectX::XMFLOAT3 origin, DirectX::XMFLOAT3 direction, float& distance) const
{
	return true;
}

void Actor::setPos(const DirectX::XMFLOAT3& pos)
{
	m_pos = pos;
}

void Actor::setRot(const DirectX::XMFLOAT4& rot)
{
	m_rot = rot;
}

void Actor::setScale(const DirectX::XMFLOAT3& scale)
{
	m_scale = scale;
}

void Actor::setWorld(const XMFLOAT4X4& matrix)
{
	XMMATRIX newWorld = XMLoadFloat4x4(&matrix);
	XMVECTOR scale;
	XMVECTOR rot;
	XMVECTOR pos;
	XMMatrixDecompose(&scale, &rot, &pos, newWorld);
	XMStoreFloat3(&m_pos, pos);
	XMStoreFloat3(&m_scale, scale);
	XMStoreFloat4(&m_rot, rot);
	m_world = matrix;
}

void Actor::transform(const XMFLOAT4X4& matrix)
{
	XMMATRIX newWorld = XMLoadFloat4x4(&m_world) * XMLoadFloat4x4(&matrix);
	XMVECTOR scale;
	XMVECTOR rot;
	XMVECTOR pos;
	XMMatrixDecompose(&scale, &rot, &pos, newWorld);
	XMStoreFloat3(&m_pos, pos);
	XMStoreFloat3(&m_scale, scale);
	XMStoreFloat4(&m_rot, rot);
	XMStoreFloat4x4(&m_world, newWorld);
}

void Actor::computeWorld()
{
	XMVECTOR p = XMLoadFloat3(&m_pos);
	XMVECTOR r = XMLoadFloat4(&m_rot);
	XMVECTOR s = XMLoadFloat3(&m_scale);
	XMMATRIX m = XMMatrixScalingFromVector(s) * XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(p);
	XMStoreFloat4x4(&m_world, m);
}

void Actor::setRender(bool render)
{
	m_render = render;
}

void Actor::toggleRender()
{
	setRender(!m_render);
}


const XMFLOAT3& Actor::getPos() const
{
	return m_pos;
}

const XMFLOAT4& Actor::getRot() const
{
	return m_rot;
}

const XMFLOAT3& Actor::getScale() const
{
	return m_scale;
}

XMFLOAT4X4 Actor::getWorld() const
{
	return m_world;
}


