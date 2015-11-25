#include "actor.h"

using namespace DirectX;

Actor::Actor(ObjectType type)
	: m_pos({ 0.0, 0.0, 0.0 }),
	m_rot({ 0.0, 0.0, 0.0, 1.0 }),
	m_scale({ 1.0, 1.0, 1.0 }),
	m_render(true),
	m_type(type)
{
}

Actor::~Actor()
{
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
	XMVECTOR p = XMLoadFloat3(&m_pos);
	XMVECTOR r = XMLoadFloat4(&m_rot);
	XMVECTOR s = XMLoadFloat3(&m_scale);
	XMFLOAT4X4 mat;
	XMStoreFloat4x4(&mat, XMMatrixScalingFromVector(s) * XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(p));
	return mat;
}