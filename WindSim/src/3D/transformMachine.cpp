#include "transformMachine.h"

#include "objectManager.h"
#include "camera.h"
#include "settings.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QLineF>
#include <QLine>

#include <DirectXMath.h>

using namespace State;
using namespace DirectX;

TransformMachine::TransformMachine(ObjectManager* manager, Camera* camera)
	: m_state(Start),
	m_oldActors(),
	m_oldObjWorldPos(0.0f, 0.0f, 0.0f),
	m_oldObjWindowPos(0, 0),
	m_oldCursorPos(0, 0),
	m_oldCursorDir(1, 0, 0),
	m_transformation(),
	m_manager(manager),
	m_camera(camera)
{
}

TransformMachine::~TransformMachine()
{
}

void TransformMachine::handleKeyPress(QKeyEvent* event, QPoint currentMousePos)
{
	switch (event->key())
	{
	case(Qt::Key_T) :
		if (m_state == Start)
		{
			m_state = Translate;
			start(currentMousePos);
		}
		break;
	case(Qt::Key_G) :
		if (m_state == Start)
		{
			m_state = Scale;
			start(currentMousePos);
		}
		break;
	case(Qt::Key_R) :
		if (m_state == Start)
		{
			m_state = Rotate;
			start(currentMousePos);
		}
		break;
	case(Qt::Key_X):
		switch (m_state)
		{
		case(Translate) : m_state = TranslateX; break;
		case(TranslateX) : m_state = Translate; break;
		case(TranslateY) : m_state = TranslateX; break;
		case(TranslateZ) : m_state = TranslateX; break;
		case(Scale) : m_state = ScaleX; break;
		case(ScaleX) : m_state = Scale; break;
		case(ScaleY) : m_state = ScaleX; break;
		case(ScaleZ) : m_state = ScaleX; break;
		case(Rotate) : m_state = RotateX; break;
		case(RotateX) : m_state = Rotate; break;
		case(RotateY) : m_state = RotateX; break;
		case(RotateZ) : m_state = RotateX; break;
		default: return;
		}
	case(Qt::Key_Y) :
		switch (m_state)
		{
		case(Translate) : m_state = TranslateY; break;
		case(TranslateY) : m_state = Translate; break;
		case(TranslateX) : m_state = TranslateY; break;
		case(TranslateZ) : m_state = TranslateY; break;
		case(Scale) : m_state = ScaleY; break;
		case(ScaleY) : m_state = Scale; break;
		case(ScaleX) : m_state = ScaleY; break;
		case(ScaleZ) : m_state = ScaleY; break;
		case(Rotate) : m_state = RotateY; break;
		case(RotateY) : m_state = Rotate; break;
		case(RotateX) : m_state = RotateY; break;
		case(RotateZ) : m_state = RotateY; break;
		default: return;
		}
	case(Qt::Key_Z) :
		switch (m_state)
		{
		case(Translate) : m_state = TranslateZ; break;
		case(TranslateZ) : m_state = Translate; break;
		case(TranslateX) : m_state = TranslateZ; break;
		case(TranslateY) : m_state = TranslateZ; break;
		case(Scale) : m_state = ScaleZ; break;
		case(ScaleZ) : m_state = Scale; break;
		case(ScaleX) : m_state = ScaleZ; break;
		case(ScaleY) : m_state = ScaleZ; break;
		case(Rotate) : m_state = RotateZ; break;
		case(RotateZ) : m_state = Rotate; break;
		case(RotateX) : m_state = RotateZ; break;
		case(RotateY) : m_state = RotateZ; break;
		default: return;
		}
	case(Qt::Key_Escape) :
		if (m_state != Start && m_state != Aborted && m_state != Finished)
		{
			m_state = Aborted;
			abort();
		}
		break;
	case(Qt::Key_Enter):
		if (m_state != Start && m_state != Aborted && m_state != Finished)
		{
			m_state = Aborted;
			//m_state = Finished;
			finish();
		}
		break;
	default: return;
	}
}

void TransformMachine::handleMousePress(QMouseEvent* event)
{
	switch (event->button())
	{
	case(Qt::LeftButton):
		if (m_state != Start && m_state != Aborted && m_state != Finished)
		{
			m_state = Finished;
			finish();
		}
		break;
	case(Qt::RightButton):
		if (m_state != Start && m_state != Aborted && m_state != Finished)
		{
			m_state = Aborted;
			abort();
		}
		break;
	default: return;
	}
}

void TransformMachine::handleMouseMove(QMouseEvent* event)
{
	QPoint currentMousePos = event->pos();

	switch (m_state)
	{
	case(Translate) :
	case(TranslateX):
	case(TranslateY):
	case(TranslateZ):
		translate(currentMousePos);
		break;
	case(Scale) : // Move object parallel to camera view plane
	case(ScaleX) :
	case(ScaleY) :
	case(ScaleZ) :
		scale(currentMousePos);
		break;
	case(Rotate) : // Move object parallel to camera view plane
	case(RotateX) :
	case(RotateY) :
	case(RotateZ) :
		rotate(currentMousePos);
		break;
	}
}

std::vector<QJsonObject> TransformMachine::getTransformation()
{
	return m_transformation;
}

void TransformMachine::start(QPoint currentMousePos)
{
	// Save selected actors
	for (auto id : m_manager->getSelection())
	{
		// Copy the data of the actor (so it will not be overwritten by our temporary transformations
		MeshActor* a = new MeshActor(*std::dynamic_pointer_cast<MeshActor>(m_manager->getActor(id)));
		m_oldActors.emplace(std::make_pair(id, std::shared_ptr<MeshActor>(a)));
	}

	m_oldCursorPos = currentMousePos;
	m_oldCursorDir = m_camera->getCursorDir(m_oldCursorPos);

	// Calculate the average position of the objects
	// In world space
	XMVECTOR groupPos = XMVectorZero();
	for (const auto& act : m_oldActors)
	{
		groupPos += XMLoadFloat3(&act.second->getPos()); // position prior to the transformation
	}
	groupPos /= m_oldActors.size();
	XMStoreFloat3(&m_oldObjWorldPos, groupPos);

	// In window space
	XMFLOAT4 objPos;
	XMStoreFloat4(&objPos, groupPos);
	objPos.w = 1.0f;
	m_oldObjWindowPos = m_camera->worldToWindow(objPos);
}

void TransformMachine::abort()
{
	// Copy old values to the current actors to restore there original transformation
	for (const auto a : m_oldActors)
	{
		std::shared_ptr<Actor> act = m_manager->getActor(a.second->getId());
		act->setPos(a.second->getPos());
		act->setScale(a.second->getScale());
		act->setRot(a.second->getRot());
		act->computeWorld();
	}
}

void TransformMachine::finish()
{
	// Iterate all selected (and therefore currently transformed) objects
	for (auto id : m_manager->getSelection())
	{
		std::shared_ptr<MeshActor> act = std::dynamic_pointer_cast<MeshActor>(m_manager->getActor(id));
		XMFLOAT3 p = act->getPos();
		XMFLOAT3 s = act->getScale();
		XMFLOAT4 r = act->getRot();
		XMVECTOR axis;
		float angle;
		XMQuaternionToAxisAngle(&axis, &angle, XMLoadFloat4(&r));
		XMFLOAT3 ax;
		XMStoreFloat3(&ax, XMVector3Normalize(axis));
		float al;
		float be;
		float ga;
		toEuler(ax, angle, al, be, ga);

		QJsonObject pos = { { "x", p.x }, { "y", p.y }, { "z", p.z } };
		QJsonObject scale = { { "x", s.x }, { "y", s.y }, { "z", s.z } };
		QJsonObject rot = { { "al", radToDeg(al) }, { "be", radToDeg(be) }, { "ga", radToDeg(ga) } };
		m_transformation.push_back({ { "id", id }, { "Position", pos }, { "Scaling", scale }, { "Rotation", rot } });
	}
}

void TransformMachine::translate(QPoint currentCursorPos)
{
	switch (m_state)
	{
	case(Translate) :  // Move object parallel to camera view plane
		// The transformation is calculated from the old actor position(at the beginning of the transformation) and the mouse move vector(since the beginning of the transformation)
		//
		// Option 1:
		// Use camera up and right vector in world space together with screen space coordinates of the mouse move to move the object
		// Additionally we have to use a small magic number to adjust the amount of translation to the mouse cursor movement. This should differ dependent on the distance to the translated object.
		// Option 2 (more exact):
		// Calculate the two rays through the mouse cursor positions (old and current) in world space. Compute a plane, parallel to the view plane at the averaged position of the translated objects
		// Compute the two intersections of the mouse rays and the move plane and calculate the translation vector from these two intersections

		//// Option 1:
		//XMVECTOR up = XMLoadFloat3(&m_camera->getUpVector());
		//XMVECTOR right = XMLoadFloat3(&m_camera->getRightVector());
		//QPoint move = currentCursorPos - m_oldCursorPos;
		//float magicNumber = 0.03;
		//for (const auto& act : m_oldActors)
		//{
		//	XMVECTOR pos = XMLoadFloat3(&act.second->getPos());
		//	pos += right * move.x() * magicNumber;
		//	pos += up * move.y() * magicNumber;
		//	XMFLOAT3 position;
		//	XMStoreFloat3(&position, pos);
		//	std::shared_ptr<Actor> a = m_manager->getActors()[act.second->getId()];
		//	a->setPos(position);
		//	a->computeWorld();
		//}


		// Option 2:
		XMVECTOR up = XMLoadFloat3(&m_camera->getUpVector());
		XMVECTOR right = XMLoadFloat3(&m_camera->getRightVector());
		XMVECTOR ncd = XMLoadFloat3(&m_camera->getCursorDir(currentCursorPos)); // New cursor direction vector
		XMVECTOR ocd = XMLoadFloat3(&m_oldCursorDir); // Old cursor direction vector
		XMVECTOR origin = XMLoadFloat3(&m_camera->getCamPos()); // Origin of the two cursor rays

		XMVECTOR movePlane = XMPlaneFromPointNormal(XMLoadFloat3(&m_oldObjWorldPos), XMVector3Cross(up, right)); // The move plane, which is parallel two the viewing plane

		XMVECTOR moveVec = XMPlaneIntersectLine(movePlane, origin, origin + ncd) - XMPlaneIntersectLine(movePlane, origin, origin + ocd); // The translation vector in world space

		for (const auto& act : m_oldActors)
		{
			XMFLOAT3 position;
			XMStoreFloat3(&position, XMLoadFloat3(&act.second->getPos()) + moveVec);
			std::shared_ptr<Actor> a = m_manager->getActors()[act.second->getId()];
			a->setPos(position);
			a->computeWorld();
		}

		break;
	}
}

void TransformMachine::scale(QPoint currentMousePos)
{
	switch (m_state)
	{
	case(Scale):

		float scaleFactor = static_cast<float>((currentMousePos - m_oldObjWindowPos).manhattanLength()) / static_cast<float>((m_oldCursorPos - m_oldObjWindowPos).manhattanLength());

		for (const auto& act : m_oldActors)
		{
			// Calculate overall scaling wrt the averaged object position
			//===========================================================
			XMVECTOR pos = XMLoadFloat3(&act.second->getPos());
			XMVECTOR scale = XMLoadFloat3(&act.second->getScale());
			XMVECTOR objPos = XMLoadFloat3(&m_oldObjWorldPos);
			std::shared_ptr<Actor> a = m_manager->getActors()[act.second->getId()];

			// Only one object, or the object positions are all equal:
			// In this case the transformation becomes simpler
			if (XMVector3Equal(pos, objPos))
			{
				// Simply apply local scaling
				XMFLOAT3 newScale;
				XMStoreFloat3(&newScale, scale * scaleFactor);
				a->setScale(newScale);
				a->computeWorld();
			}
			else
			{
				// In this case the scaling center is different to the origin of the object space
				// -> The translation of the object changes too
				// -> We have to decompose the transformation and have to apply its different parts in the correct order
				XMVECTOR rot = XMLoadFloat4(&act.second->getRot());

				// Build new world matrix
				XMMATRIX newWorld = XMMatrixScalingFromVector(scale); // Local scaling (from oldWorld)
				newWorld *= XMMatrixRotationQuaternion(rot); // Local rotation (from oldWorld)
				newWorld *= XMMatrixTranslationFromVector((pos - objPos)); // Move transformation center to averaged object position (includes oldWorld translation)
				newWorld *= XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor); // Perform scaling, dependent on mouse movement (scaling center is averaged object position (located at the origin))
				newWorld *= XMMatrixTranslationFromVector(objPos); // Move object to its final world position (as objPos is currently at origin -> move about objPos to get to world position)

				XMFLOAT4X4 nw;
				XMStoreFloat4x4(&nw, newWorld);
				a->setWorld(nw);
			}
		}
		break;
	}
}

void TransformMachine::rotate(QPoint currentMousePos)
{
	switch (m_state)
	{
	case(Rotate) :

		// Angle between the lines from the averaged object position to current and old cursor positions
		float angle = degToRad(QLineF(QLine(m_oldObjWindowPos, currentMousePos)).angleTo(QLineF(QLine(m_oldObjWindowPos, m_oldCursorPos))));

		//XMVECTOR axis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  // Rotation plane is parallel to view plane
		XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&m_camera->getCamPos()) - XMLoadFloat3(&m_oldObjWorldPos));

		for (const auto& act : m_oldActors)
		{
			// Calculate overall rotation wrt the averaged object position
			//===========================================================
			XMVECTOR pos = XMLoadFloat3(&act.second->getPos());
			XMVECTOR rot = XMLoadFloat4(&act.second->getRot());
			XMVECTOR objPos = XMLoadFloat3(&m_oldObjWorldPos);
			std::shared_ptr<Actor> a = m_manager->getActors()[act.second->getId()];

			// Only one object, or the object positions are all equal:
			// In this case the transformation becomes simpler
			if (XMVector3Equal(pos, objPos))
			{
				// Simply apply local rotation
				XMFLOAT4 newRot;
				XMStoreFloat4(&newRot, XMQuaternionMultiply(rot, XMQuaternionRotationNormal(axis, angle)));
				a->setRot(newRot);
				a->computeWorld();
			}
			else
			{
				// In this case the rotation axis does not run through the origin of the object space
				// -> The translation of the object changes too
				// -> We have to decompose the transformation and have to apply its different parts in the correct order
				XMVECTOR scale = XMLoadFloat3(&act.second->getScale());

				// Build new world matrix
				XMMATRIX newWorld = XMMatrixScalingFromVector(scale); // Local scaling (from oldWorld)
				newWorld *= XMMatrixRotationQuaternion(rot); // Local rotation (from oldWorld)
				newWorld *= XMMatrixTranslationFromVector((pos - objPos)); // Move transformation center to averaged object position (includes oldWorld translation)
				newWorld *= XMMatrixRotationNormal(axis, angle); // Perform rotation, dependent on mouse movement (rotation axis runs from camera position through averaged object position (located at the origin))
				newWorld *= XMMatrixTranslationFromVector(objPos); // Move object to its final world position (as objPos is currently at origin -> move about objPos to get to world position)

				XMFLOAT4X4 nw;
				XMStoreFloat4x4(&nw, newWorld);
				a->setWorld(nw);
			}
		}
		break;
	}
}