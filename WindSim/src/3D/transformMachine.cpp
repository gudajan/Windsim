#include "transformMachine.h"

#include "objectManager.h"
#include "camera.h"
#include "settings.h"
#include "actor.h"
#include "marker.h"
#include "markerActor.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QLineF>
#include <QLine>

#include <DirectXMath.h>

#include <memory>
#include <string>

using namespace State;
using namespace DirectX;

TransformMachine::TransformMachine(ObjectManager* manager, Camera* camera)
	: m_state(Start),
	m_marker(nullptr),
	m_oldActors(),
	m_oldObjWorldPos(0.0f, 0.0f, 0.0f),
	m_oldObjWindowPos(0, 0),
	m_oldCursorPos(0, 0),
	m_oldCursorDir(1, 0, 0),
	m_oldXZInt(0.0, 0.0, 0.0),
	m_oldXYInt(0.0, 0.0, 0.0),
	m_oldYZInt(0.0, 0.0, 0.0),
	m_transformation(),
	m_manager(manager),
	m_camera(camera)
{
}

void TransformMachine::initDX11(ID3D11Device* device)
{
	// Create transformation marker
	Marker* m = new Marker();
	m_marker = std::shared_ptr<MarkerActor>(new MarkerActor(*m, 0)); // ID is not necessary

	m_marker->setRender(false);
	m_marker->setXAxisRendered(false);
	m_marker->setYAxisRendered(false);
	m_marker->setZAxisRendered(false);
	m_marker->setLarge(true);

	m_manager->addAccessoryObject("transformMarker", std::shared_ptr<Object3D>(m), m_marker);

	m->create(device, true);
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
		case(Translate)  : m_state = TranslateX; switchToX();    break;
		case(TranslateX) : m_state = Translate;  switchToNone(); break;
		case(TranslateY) : m_state = TranslateX; switchToX();    break;
		case(TranslateZ) : m_state = TranslateX; switchToX();    break;
		case(Scale)      : m_state = ScaleX;     switchToX();    break;
		case(ScaleX)     : m_state = Scale;      switchToNone(); break;
		case(ScaleY)     : m_state = ScaleX;     switchToX();    break;
		case(ScaleZ)     : m_state = ScaleX;     switchToX();    break;
		case(Rotate)     : m_state = RotateX;    switchToX();    break;
		case(RotateX)    : m_state = Rotate;     switchToNone(); break;
		case(RotateY)    : m_state = RotateX;    switchToX();    break;
		case(RotateZ)    : m_state = RotateX;    switchToX();    break;
		default: return;
		}
		break;
	case(Qt::Key_Y) :
		switch (m_state)
		{
		case(Translate)  : m_state = TranslateY; switchToY();    break;
		case(TranslateY) : m_state = Translate;  switchToNone(); break;
		case(TranslateX) : m_state = TranslateY; switchToY();    break;
		case(TranslateZ) : m_state = TranslateY; switchToY();    break;
		case(Scale)      : m_state = ScaleY;     switchToY();    break;
		case(ScaleY)     : m_state = Scale;      switchToNone(); break;
		case(ScaleX)     : m_state = ScaleY;     switchToY();    break;
		case(ScaleZ)     : m_state = ScaleY;     switchToY();    break;
		case(Rotate)     : m_state = RotateY;    switchToY();    break;
		case(RotateY)    : m_state = Rotate;     switchToNone(); break;
		case(RotateX)    : m_state = RotateY;    switchToY();    break;
		case(RotateZ)    : m_state = RotateY;    switchToY();    break;
		default: return;
		}
		break;
	case(Qt::Key_Z) :
		switch (m_state)
		{
		case(Translate)  : m_state = TranslateZ; switchToZ();    break;
		case(TranslateZ) : m_state = Translate;  switchToNone(); break;
		case(TranslateX) : m_state = TranslateZ; switchToZ();    break;
		case(TranslateY) : m_state = TranslateZ; switchToZ();    break;
		case(Scale)      : m_state = ScaleZ;     switchToZ();    break;
		case(ScaleZ)     : m_state = Scale;      switchToNone(); break;
		case(ScaleX)     : m_state = ScaleZ;     switchToZ();    break;
		case(ScaleY)     : m_state = ScaleZ;     switchToZ();    break;
		case(Rotate)     : m_state = RotateZ;    switchToZ();    break;
		case(RotateZ)    : m_state = Rotate;     switchToNone(); break;
		case(RotateX)    : m_state = RotateZ;    switchToZ();    break;
		case(RotateY)    : m_state = RotateZ;    switchToZ();    break;
		default: return;
		}
		break;
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
	case(TranslateX) :
	case(TranslateY) :
	case(TranslateZ) :
		translate(currentMousePos, event->modifiers());
		break;
	case(Scale) :
	case(ScaleX) :
	case(ScaleY) :
	case(ScaleZ) :
		scale(currentMousePos, event->modifiers());
		break;
	case(Rotate) :
	case(RotateX) :
	case(RotateY) :
	case(RotateZ) :
		rotate(currentMousePos, event->modifiers());
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
		Actor* a = m_manager->getActor(id)->clone();
		m_oldActors.emplace(id, std::shared_ptr<Actor>(a));
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

	// Calculate intersections with xz, xy and yz plane
	XMVECTOR xzPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	XMVECTOR xyPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	XMVECTOR yzPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	XMVECTOR camPos = XMLoadFloat3(&m_camera->getCamPos());
	XMVECTOR cursorDir = XMLoadFloat3(&m_oldCursorDir);

	XMVECTOR xzInt = XMPlaneIntersectLine(xzPlane, camPos, camPos + cursorDir);
	XMVECTOR xyInt = XMPlaneIntersectLine(xyPlane, camPos, camPos + cursorDir);
	XMVECTOR yzInt = XMPlaneIntersectLine(yzPlane, camPos, camPos + cursorDir);

	XMStoreFloat3(&m_oldXZInt, xzInt);
	XMStoreFloat3(&m_oldXYInt, xyInt);
	XMStoreFloat3(&m_oldYZInt, yzInt);

	// Start rendering the transformation marker
	m_marker->setRender(true);
	m_marker->setPos(m_oldObjWorldPos);
	m_marker->computeWorld();
	switchToNone();
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

	// Stop rendering the transformation marker
	m_marker->setRender(false);
}

void TransformMachine::finish()
{
	// Iterate all selected (and therefore currently transformed) objects
	for (auto id : m_manager->getSelection())
	{
		std::shared_ptr<Actor> act = m_manager->getActor(id);
		XMFLOAT3 p = act->getPos();
		XMFLOAT3 s = act->getScale();
		XMFLOAT4 r = act->getRot();
		XMVECTOR axis;
		float angle;
		XMQuaternionToAxisAngle(&axis, &angle, XMLoadFloat4(&r));

		QJsonObject pos = { { "x", p.x }, { "y", p.y }, { "z", p.z } };
		QJsonObject scale = { { "x", s.x }, { "y", s.y }, { "z", s.z } };
		QJsonObject rot = { { "ax", XMVectorGetX(axis) }, { "ay", XMVectorGetY(axis) }, { "az", XMVectorGetZ(axis) }, { "angle", radToDeg(angle) } };
		m_transformation.push_back({ { "id", id }, { "Position", pos }, { "Scaling", scale }, { "Rotation", rot } });
	}

	// Stop rendering the transformation marker
	m_marker->setRender(false);
}

void TransformMachine::translate(QPoint currentCursorPos, Qt::KeyboardModifiers mods)
{
	XMVECTOR moveVec;
	if (m_state == Translate)  // Move object parallel to camera view plane
	{
		// The transformation is calculated from the old actor position(at the beginning of the transformation) and the mouse move vector(since the beginning of the transformation)
		// Calculate the two rays through the mouse cursor positions (old and current) in world space. Compute a plane, parallel to the view plane at the averaged position of the translated objects
		// Compute the two intersections of the mouse rays and the move plane and calculate the translation vector from these two intersections
		XMVECTOR up = XMLoadFloat3(&m_camera->getUpVector());
		XMVECTOR right = XMLoadFloat3(&m_camera->getRightVector());
		XMVECTOR ncd = XMLoadFloat3(&m_camera->getCursorDir(currentCursorPos)); // New cursor direction vector
		XMVECTOR ocd = XMLoadFloat3(&m_oldCursorDir); // Old cursor direction vector
		XMVECTOR origin = XMLoadFloat3(&m_camera->getCamPos()); // Origin of the two cursor rays

		XMVECTOR movePlane = XMPlaneFromPointNormal(XMLoadFloat3(&m_oldObjWorldPos), XMVector3Cross(up, right)); // The move plane, which is parallel two the viewing plane

		moveVec = XMPlaneIntersectLine(movePlane, origin, origin + ncd) - XMPlaneIntersectLine(movePlane, origin, origin + ocd); // The translation vector in world space

	}
	else if (m_state == TranslateX || m_state == TranslateY || m_state == TranslateZ) // Move object along global coordinate axes
	{
		// Calculate intersection with corresponding global plane and check the distance in the specified direction (x,y,z) (i.e. check that coordinate of the intersection point)
		// This corresponds to the distanec on the specified axis
		// Refer to intersection point at start of the transformation
		XMVECTOR camPos = XMLoadFloat3(&m_camera->getCamPos());
		XMVECTOR cursorDir = XMLoadFloat3(&m_camera->getCursorDir(currentCursorPos));
		XMVECTOR axis;
		XMVECTOR plane;
		XMVECTOR oldInt;

		if (m_state == TranslateX)
		{
			axis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			plane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)); // XZ plane
			oldInt = XMLoadFloat3(&m_oldXZInt);
		}
		else if (m_state == TranslateY)
		{
			axis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			// Choose plane dependent on the current angle of the camera to this plane
			// If cosine of cursor ray to plane normal is bigger -> plane has a "better" angle to the camera
			if (XMVectorGetX(XMVectorAbs(XMVector3Dot(cursorDir, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)))) > XMVectorGetX(XMVectorAbs(XMVector3Dot(cursorDir, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)))))
			{
				plane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)); // XY plane
				oldInt = XMLoadFloat3(&m_oldXYInt);
			}
			else
			{
				plane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)); // YZ plane
				oldInt = XMLoadFloat3(&m_oldYZInt);
			}

		}
		else if (m_state == TranslateZ)
		{
			axis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			plane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)); // XZ plane
			oldInt = XMLoadFloat3(&m_oldXZInt);
		}
		else
			throw std::runtime_error("Invalid translation state!");


		XMVECTOR newInt= XMPlaneIntersectLine(plane, camPos, camPos + cursorDir);

		if (XMVectorGetIntX(XMVectorIsNaN(oldInt)) || XMVectorGetIntX(XMVectorIsNaN(newInt)))
			return;

		float distance = 0.0f;

		if (m_state == TranslateX)
			distance = XMVectorGetX(newInt) - XMVectorGetX(oldInt);
		else if (m_state == TranslateY)
			distance = XMVectorGetY(newInt) - XMVectorGetY(oldInt);
		else if (m_state == TranslateZ)
			distance = XMVectorGetZ(newInt) - XMVectorGetZ(oldInt);

		moveVec = distance * axis;
	}

	if (mods == Qt::ControlModifier)
	{
		float stepInv = 1.0f / conf.gen.translationStep;
		float step = conf.gen.translationStep;
		moveVec = XMVectorMultiply(moveVec, XMVectorSet(stepInv, stepInv, stepInv, stepInv));
		moveVec = XMVectorRound(moveVec);
		moveVec = XMVectorMultiply(moveVec, XMVectorSet(step, step, step, step));
	}

	for (const auto& act : m_oldActors)
	{
		XMFLOAT3 position;
		XMStoreFloat3(&position, XMLoadFloat3(&act.second->getPos()) + moveVec);
		std::shared_ptr<Actor> a = m_manager->getActor(act.second->getId());
		a->setPos(position);
		a->computeWorld();
	}

	// Update transformation marker
	XMFLOAT3 position;
	XMStoreFloat3(&position, XMLoadFloat3(&m_oldObjWorldPos) + moveVec);
	m_marker->setPos(position);
	m_marker->computeWorld();
}

void TransformMachine::scale(QPoint currentMousePos, Qt::KeyboardModifiers mods)
{
	float scaleFactor = static_cast<float>((currentMousePos - m_oldObjWindowPos).manhattanLength()) / static_cast<float>((m_oldCursorPos - m_oldObjWindowPos).manhattanLength());

	if (mods == Qt::ControlModifier)
	{
		float stepInv = 1.0f / conf.gen.scalingStep;
		float step = conf.gen.scalingStep;
		scaleFactor *= stepInv;
		scaleFactor = std::roundf(scaleFactor);
		scaleFactor *= step;
	}

	XMVECTOR objPos = XMLoadFloat3(&m_oldObjWorldPos);

	for (const auto& act : m_oldActors)
	{
		// Calculate overall scaling wrt the averaged object position
		//===========================================================
		XMVECTOR pos = XMLoadFloat3(&act.second->getPos());
		XMVECTOR scale = XMLoadFloat3(&act.second->getScale());
		std::shared_ptr<Actor> a = m_manager->getActor(act.second->getId());

		// Only one object, or the object positions are all equal:
		// In this case the transformation becomes simpler
		if (XMVector3Equal(pos, objPos))
		{
			// Simply apply local scaling
			XMFLOAT3 newScale;
			if (m_state == Scale)
				XMStoreFloat3(&newScale, scale * scaleFactor);
			else if (m_state == ScaleX)
				XMStoreFloat3(&newScale, scale * XMVectorSet(scaleFactor, 1.0f, 1.0f, 1.0f));
			else if (m_state == ScaleY)
				XMStoreFloat3(&newScale, scale * XMVectorSet(1.0f, scaleFactor, 1.0f, 1.0f));
			else if (m_state == ScaleZ)
				XMStoreFloat3(&newScale, scale * XMVectorSet(1.0f, 1.0f, scaleFactor, 1.0f));
			else
				throw std::runtime_error("Invalid scaling state!");
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
			// Perform scaling, dependent on mouse movement (scaling center is averaged object position (located at the origin))
			if (m_state == Scale)
				newWorld *= XMMatrixScaling(scaleFactor, scaleFactor, scaleFactor);
			else if (m_state == ScaleX)
				newWorld *= XMMatrixScaling(scaleFactor, 1.0f, 1.0f);
			else if (m_state == ScaleY)
				newWorld *= XMMatrixScaling(1.0f, scaleFactor, 1.0f);
			else if (m_state == ScaleZ)
				newWorld *= XMMatrixScaling(1.0f, 1.0f, scaleFactor);
			else
				throw std::runtime_error("Invalid scaling state!");
			newWorld *= XMMatrixTranslationFromVector(objPos); // Move object to its final world position (as objPos is currently at origin -> move about objPos to get to world position)

			XMFLOAT4X4 nw;
			XMStoreFloat4x4(&nw, newWorld);
			a->setWorld(nw);
		}
	}
}

void TransformMachine::rotate(QPoint currentMousePos, Qt::KeyboardModifiers mods)
{
	// Angle between the lines from the averaged object position to current and old cursor positions
	float angle = QLineF(QLine(m_oldObjWindowPos, currentMousePos)).angleTo(QLineF(QLine(m_oldObjWindowPos, m_oldCursorPos)));

	if (mods == Qt::ControlModifier)
	{
		float stepInv = 1.0f / conf.gen.rotationStep;
		float step = conf.gen.rotationStep;
		angle *= stepInv;
		angle = std::roundf(angle);
		angle *= step;
	}
	angle = degToRad(angle);

	XMVECTOR axis;
	if (m_state == Rotate)
		axis = XMVector3Normalize(XMLoadFloat3(&m_camera->getCamPos()) - XMLoadFloat3(&m_oldObjWorldPos)); // Perpendicular to view plane
	else if (m_state == RotateX)
		axis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	else if (m_state == RotateY)
		axis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	else if (m_state == RotateZ)
		axis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	else
		throw std::runtime_error("Invalid rotating state!");

	for (const auto& act : m_oldActors)
	{
		// Calculate overall rotation wrt the averaged object position
		//===========================================================
		XMVECTOR pos = XMLoadFloat3(&act.second->getPos());
		XMVECTOR rot = XMLoadFloat4(&act.second->getRot());
		XMVECTOR objPos = XMLoadFloat3(&m_oldObjWorldPos);
		std::shared_ptr<Actor> a = m_manager->getActor(act.second->getId());

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
}

void TransformMachine::switchToNone()
{
	m_marker->setXAxisRendered(false);
	m_marker->setYAxisRendered(false);
	m_marker->setZAxisRendered(false);
}

void TransformMachine::switchToX()
{
	m_marker->setXAxisRendered(true);
	m_marker->setYAxisRendered(false);
	m_marker->setZAxisRendered(false);
}

void TransformMachine::switchToY()
{
	m_marker->setXAxisRendered(false);
	m_marker->setYAxisRendered(true);
	m_marker->setZAxisRendered(false);
}

void TransformMachine::switchToZ()
{
	m_marker->setXAxisRendered(false);
	m_marker->setYAxisRendered(false);
	m_marker->setZAxisRendered(true);
}

void TransformMachine::toEuler(DirectX::XMFLOAT4 q, float& al, float& be, float& ga)
{
	// The angle sequence phi1, phi2, phi3
	float theta1;
	float theta2;
	float theta3;

	// Angle sequence in directx11 is z -> x -> y
	float q0 = q.w;
	float q1 = q.z;
	float q2 = q.x;
	float q3 = q.y;

	double sq0 = q0 * q0;
	double sq1 = q1*q1;
	double sq2 = q2*q2;
	double sq3 = q3*q3;
	double unit = sq1 + sq2 + sq3 + sq0;
	double test = q0*q2 + q1*q3;

	if (test > 0.49999999 * unit) { // singularity at north pole
		theta1 = 2 * std::atan2(q1, q0);
		theta2 = DirectX::XM_PI / 2;
		theta3 = 0;
		return;
	}
	if (test < -0.49999999 * unit) { // singularity at south pole
		theta1 = 2 * std::atan2(q1, q0);
		theta2 = -DirectX::XM_PI / 2;
		theta3 = 0;
		return;
	}

	theta1 = std::atan2(2 * q0*q1 - 2 * q2*q3, 1 - 2 * sq1 - 2 * sq2);
	theta2 = std::asin(2 * test / unit);
	theta3 = std::atan2(2 * q0*q3 - 2 * q1*q2, 1 - 2 * sq2 - 2 * sq3);

	al = theta2; // pitch is x in DirectX
	be = theta3; // yaw is y in directX
	ga = theta1; // roll is z in directX
}