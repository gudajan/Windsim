#include "camera.h"
#include "common.h"

#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>

using namespace DirectX;

Camera::Camera(const uint32_t width, const uint32_t height, const CameraType type)
	: m_pos({ 0.0, 3.0, -10.0 }),
	m_rot({ 0.0, 0.0, 0.0, 1.0 }),
	m_fov(degToRad(50)),
	m_aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
	m_nearZ(0.1f),
	m_farZ(100.0f),
	m_type(type),
	m_rotating(false),
	m_translating(false),
	m_angleX(0.15 * XM_PI),
	m_angleY(0.0f),
	m_keys({ false, false, false, false, false, false })
{
	// Per default the camera looks down at the origin
	XMStoreFloat4(&m_rot, XMQuaternionRotationRollPitchYaw(m_angleX, m_angleY, 0.0));
}

Camera::~Camera()
{
}

DirectX::XMFLOAT4X4 Camera::getViewMatrix() const
{
	XMFLOAT4X4 view;
	XMVECTOR r = XMLoadFloat4(&m_rot);
	XMVECTOR p = XMLoadFloat3(&m_pos);
	XMStoreFloat4x4(&view, XMMatrixInverse(nullptr, XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(p)));
	return view;
}
DirectX::XMFLOAT4X4 Camera::getProjectionMatrix() const
{
	XMFLOAT4X4 proj;
	XMStoreFloat4x4(&proj, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
	return proj;
}

bool Camera::handleControlEvent(QEvent* event)
{
	switch (event->type())
	{
	case(QEvent::MouseButtonPress) :
		handleMousePress(static_cast<QMouseEvent*>(event));
		break;
	case(QEvent::MouseButtonRelease) :
		handleMouseRelease(static_cast<QMouseEvent*>(event));
		break;
	case(QEvent::MouseMove) :
		handleMouseMove(static_cast<QMouseEvent*>(event));
		break;
	case(QEvent::KeyPress) :
		handleKeyPress(static_cast<QKeyEvent*>(event));
		break;
	case(QEvent::KeyRelease) :
		handleKeyRelease(static_cast<QKeyEvent*>(event));
		break;
	case(QEvent::Wheel) :
		handleWheel(static_cast<QWheelEvent*>(event));
		break;
	default:
		return false;
	}
	return true;
}

void Camera::update(double elapsedTime)
{
	if (m_type == FirstPerson)
	{
		if (m_translating)
		{
			XMVECTOR r = XMLoadFloat4(&m_rot);
			XMVECTOR p = XMLoadFloat3(&m_pos);
			XMVECTOR povAxis = XMVector3Rotate(XMVectorSet(0.0, 0.0, 1.0, 0.0), r) * config.translateSpeed * elapsedTime;
			XMVECTOR rightAxis = XMVector3Rotate(XMVectorSet(1.0, 0.0, 0.0, 0.0), r) * config.translateSpeed * elapsedTime;
			XMVECTOR upAxis = XMVector3Rotate(XMVectorSet(0.0, 1.0, 0.0, 0.0), r) * config.translateSpeed * elapsedTime;

			if (m_keys.moveForward)	p += povAxis;
			if (m_keys.moveBackward) p -= povAxis;
			if (m_keys.moveLeft) p -= rightAxis;
			if (m_keys.moveRight) p += rightAxis;
			if (m_keys.moveUp) p += upAxis;
			if (m_keys.moveDown) p -= upAxis;

			XMStoreFloat3(&m_pos, p);
		}
	}
}

void Camera::handleMousePress(QMouseEvent* event)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = event->globalPos();
	if (m_type == FirstPerson)
	{
		if (event->button() == Qt::LeftButton)
		{
			m_rotating = true;
		}
	}
	else // ModelView
	{
		if (event->button() == Qt::LeftButton)
		{
			m_rotating = true;
		}
		if (event->button() == Qt::RightButton)
		{
			m_translating = true;
		}
	}
}

void Camera::handleMouseRelease(QMouseEvent* event)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = event->globalPos();
	if (m_type == FirstPerson)
	{
		if (event->button() == Qt::LeftButton)
		{
			m_rotating = false;
		}
	}
	else // ModelView
	{
		if (event->button() == Qt::LeftButton)
		{
			m_rotating = false;
		}
		if (event->button() == Qt::RightButton)
		{
			m_translating = false;
		}
	}
}

void Camera::handleMouseMove(QMouseEvent* event)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = event->globalPos();

	QPoint mouseMove = m_gMouseCoord - m_gMouseCoordLast;
	if (m_type == FirstPerson)
	{
		if (m_rotating)
		{
			// Compute additional rotation, caused by mouse movement

			m_angleY += degToRad(mouseMove.rx()) * config.rotatingSensitivity;
			m_angleX += degToRad(mouseMove.ry()) * config.rotatingSensitivity;

			XMStoreFloat4(&m_rot, XMQuaternionRotationRollPitchYaw(m_angleX, m_angleY, 0.0));

			//// Load current rotation quaternion
			//XMVECTOR q = XMLoadFloat4(&m_rot);

			//// Compute current y-Axis respectively the up vector
			//XMVECTOR yAxis = XMVector3Rotate(XMVectorSet(0.0, 1.0, 0.0, 0.0), q); 

			//// Add rotation arround y-Axis to camera rotation
			//XMVECTOR addRot = XMQuaternionRotationAxis(yAxis, degToRad(mouseMove.rx()) * config.rotatingSensitivity);
			//q = XMQuaternionMultiply(q, addRot);
			//// With new rotation compute current x-Axis respectively the right vector (This way the camera always stays "upright")
			//XMVECTOR xAxis = XMVector3Rotate(XMVectorSet(1.0, 0.0, 0.0, 0.0), q);
		
			//// Add rotation arround x-Axis to camera rotation
			//addRot = XMQuaternionRotationAxis(xAxis, degToRad(mouseMove.ry()) * config.rotatingSensitivity);
			//q = XMQuaternionMultiply(q, addRot);

			//// Store rotation quaternion
			//XMStoreFloat4(&m_rot, q);
		}
	}
}

void Camera::handleKeyPress(QKeyEvent* event)
{
	if (m_type == FirstPerson)
	{
		switch (event->key())
		{
		case(Qt::Key_W) :
			m_keys.moveForward = true;
			break;
		case(Qt::Key_S) :
			m_keys.moveBackward = true;
			break;
		case(Qt::Key_A) :
			m_keys.moveLeft = true;
			break;
		case(Qt::Key_D) :
			m_keys.moveRight = true;
			break;
		case(Qt::Key_Q) :
			m_keys.moveUp = true;
			break;
		case(Qt::Key_E) :
			m_keys.moveDown = true;
			break;
		default:
			break;
		}

		m_translating = m_keys.moveForward || m_keys.moveBackward || m_keys.moveLeft || m_keys.moveRight || m_keys.moveUp || m_keys.moveDown ? true : false;
					
	}
}

void Camera::handleKeyRelease(QKeyEvent* event)
{
	if (m_type == FirstPerson)
	{
		switch (event->key())
		{
		case(Qt::Key_W) :
			m_keys.moveForward = false;
			break;
		case(Qt::Key_S) :
			m_keys.moveBackward = false;
			break;
		case(Qt::Key_A) :
			m_keys.moveLeft = false;
			break;
		case(Qt::Key_D) :
			m_keys.moveRight = false;
			break;
		case(Qt::Key_Q) :
			m_keys.moveUp = false;
			break;
		case(Qt::Key_E) :
			m_keys.moveDown = false;
			break;
		default:
			break;
		}

		m_translating = m_keys.moveForward || m_keys.moveBackward || m_keys.moveLeft || m_keys.moveRight || m_keys.moveUp || m_keys.moveDown ? true : false;
	}
}

void Camera::handleWheel(QWheelEvent* event)
{

}