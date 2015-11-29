#include "transformMachine.h"

#include "objectManager.h"
#include "camera.h"
#include "settings.h"

#include <QKeyEvent>
#include <QMouseEvent>

#include <DirectXMath.h>

using namespace State;
using namespace DirectX;

TransformMachine::TransformMachine(ObjectManager* manager, Camera* camera)
	: m_state(Start),
	m_oldActors(),
	m_oldCursorPos(0, 0),
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
	QPoint moveVector = currentMousePos - m_oldCursorPos;

	switch (m_state)
	{
	case(Translate) :
	case(TranslateX):
	case(TranslateY):
	case(TranslateZ):
		translate(moveVector);
		break;
	case(Scale) : // Move object parallel to camera view plane
	case(ScaleX) :
	case(ScaleY) :
	case(ScaleZ) :
		scale(moveVector);
		break;
	case(Rotate) : // Move object parallel to camera view plane
	case(RotateX) :
	case(RotateY) :
	case(RotateZ) :
		rotate(moveVector);
		break;
	}
}

std::vector<QJsonObject> TransformMachine::getTransformation()
{
	m_state = Start;
	return std::vector<QJsonObject>();
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
}

void TransformMachine::abort()
{

}

void TransformMachine::finish()
{

}

void TransformMachine::translate(QPoint move)
{
	switch (m_state)
	{
	case(Translate) :  // Move object parallel to camera view plane
		// Use camera up and right vector in world space to move the object
		// The transformation is calculated from the old actor position (at the beginning of the transformation) and the mouse move vector (since the beginning of the transformation)
		XMVECTOR up = XMLoadFloat3(&m_camera->getUpVector());
		XMVECTOR right = XMLoadFloat3(&m_camera->getRightVector());

		for (const auto& act : m_oldActors)
		{
			XMVECTOR pos = XMLoadFloat3(&act.second->getPos()); // position prior to the transformation
			pos += right * 0.03 *  move.x();
			pos -= up * 0.03 * move.y();
			XMFLOAT3 position;
			XMStoreFloat3(&position, pos);
			std::shared_ptr<Actor> a = m_manager->getActors()[act.second->getId()];
			a->setPos(position);
			a->computeWorld();
		}

		break;
	}
}

void TransformMachine::scale(QPoint move)
{

}

void TransformMachine::rotate(QPoint move)
{

}