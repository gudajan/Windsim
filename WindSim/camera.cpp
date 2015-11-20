#include "camera.h"
#include "common.h"
#include "settings.h"

#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>

using namespace DirectX;

Camera::Camera(const uint32_t width, const uint32_t height)
	: m_view(),
	m_projection(),
	m_fov(degToRad(60.0f)),
	m_aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
	m_nearZ(0.1f),
	m_farZ(1000.0f),
	m_rotating(false),
	m_translating(false),
	m_viewChanged(false),
	m_fpi({{ 0.0f, 0.0f, -conf.cam.defaultDist }, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, false, false, false, false, false, false }),
	m_mvi({{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, false, conf.cam.defaultDist })
{
	// Compute initial transformation matrices
	computeViewMatrix();
	computeProjectionMatrix();
}

Camera::~Camera()
{
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
	if (conf.cam.type == FirstPerson)
	{
		if (m_translating)
		{
			const XMVECTOR rot = XMLoadFloat4(&m_fpi.rot);
			const XMVECTOR view = XMVector3Rotate(XMVectorSet(0.0, 0.0, 1.0, 0.0), rot) * conf.cam.fp.translationSpeed * elapsedTime; // View direction
			const XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0, 0.0, 0.0, 0.0), rot) * conf.cam.fp.translationSpeed * elapsedTime;
			const XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0, 1.0, 0.0, 0.0), rot) * conf.cam.fp.translationSpeed * elapsedTime;

			XMVECTOR p = XMLoadFloat3(&m_fpi.pos);

			if (m_fpi.moveForward)	p += view;
			if (m_fpi.moveBackward) p -= view;
			if (m_fpi.moveLeft) p -= right;
			if (m_fpi.moveRight) p += right;
			if (m_fpi.moveUp) p += up;
			if (m_fpi.moveDown) p -= up;

			XMStoreFloat3(&m_fpi.pos, p);

			m_viewChanged = true;
		}
	}

	if (m_viewChanged)
	{
		m_viewChanged = false;
		computeViewMatrix();
	}
}

void Camera::setProjectionParams(const float fov, const uint32_t width, const uint32_t height, const float nearZ, const float farZ)
{
	m_fov = fov;
	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	m_nearZ = nearZ;
	m_farZ = farZ;
	computeProjectionMatrix();
}

void Camera::computeViewMatrix()
{
	XMVECTOR pos;
	XMVECTOR rot;
	if (conf.cam.type == FirstPerson)
	{
		rot = XMLoadFloat4(&m_fpi.rot);
		pos = XMLoadFloat3(&m_fpi.pos);
	}
	else // ModelView
	{
		rot = XMLoadFloat4(&m_mvi.rot);
		// For the ModelView camera, the position must be recalculated in every case, so do it here once:
		// Translate position depending on current zoom, rotation and rotation center
		XMVECTOR view = XMVector3Rotate(XMVectorSet(0.0, 0.0, 1.0, 0.0), rot); // View direction
		XMVECTOR center = XMLoadFloat3(&m_mvi.center); // Rotation center
		pos = center - m_mvi.zoom * view;
	}

	// Accumulate to view matrix
	XMStoreFloat4x4(&m_view, XMMatrixInverse(nullptr, XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos)));
}

void Camera::handleMousePress(QMouseEvent* event)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = event->globalPos();

	if (conf.cam.type == FirstPerson)
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
			if (m_mvi.pitch > XM_PI * 0.5 && m_mvi.pitch < XM_PI * 1.5)
			{
				m_mvi.flippedYawDir = true;
			}
			else
			{
				m_mvi.flippedYawDir = false;
			}
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

	if (conf.cam.type == FirstPerson)
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
	if (conf.cam.type == FirstPerson)
	{
		if (m_rotating)
		{
			// Compute additional rotation, caused by mouse movement
			m_fpi.yaw = normalizeRad(m_fpi.yaw + degToRad(mouseMove.x()) * conf.cam.fp.rotationSpeed);
			m_fpi.pitch = normalizeRad(m_fpi.pitch + degToRad(mouseMove.y()) * conf.cam.fp.rotationSpeed);

			// Recompute overall rotation of camera
			XMStoreFloat4(&m_fpi.rot, XMQuaternionRotationRollPitchYaw(m_fpi.pitch, m_fpi.yaw, 0.0)); // Switch because of DirectX coordinate system

			m_viewChanged = true;
		}
	}
	else // ModelView
	{
		if (m_rotating)
		{
			// Compute additional rotation, caused by mouse movement
			m_mvi.pitch = normalizeRad(m_mvi.pitch + degToRad(mouseMove.y()) * conf.cam.mv.rotationSpeed);

			if (m_mvi.flippedYawDir)
			{
				m_mvi.yaw = normalizeRad(m_mvi.yaw - degToRad(mouseMove.x()) * conf.cam.mv.rotationSpeed);
			}
			else
			{
				m_mvi.yaw = normalizeRad(m_mvi.yaw + degToRad(mouseMove.x()) * conf.cam.mv.rotationSpeed);
			}

			XMStoreFloat4(&m_mvi.rot, XMQuaternionRotationRollPitchYaw(m_mvi.pitch, m_mvi.yaw, 0.0)); // Switch because of DirectX coordinate system

			m_viewChanged = true;
		}

		if (m_translating)
		{
			// Move the rotation center about in the plane, which is parallel to the view plane
			// Speed of translation is dependent on the current zoom factor
			const XMVECTOR rot = XMLoadFloat4(&m_mvi.rot);
			const XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0, 0.0, 0.0, 0.0), rot) * m_mvi.zoom * conf.cam.mv.translationSpeed * mouseMove.x();
			const XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0, 1.0, 0.0, 0.0), rot) * m_mvi.zoom * conf.cam.mv.translationSpeed * mouseMove.y();

			// Invert the translation to make it appear we are moving the object and not the camera
			XMStoreFloat3(&m_mvi.center, XMLoadFloat3(&m_mvi.center) - right + up);

			m_viewChanged = true;
		}
	}
}

void Camera::handleKeyPress(QKeyEvent* event)
{
	if (conf.cam.type == FirstPerson)
	{
		switch (event->key())
		{
		case(Qt::Key_W) :
			m_fpi.moveForward = true;
			break;
		case(Qt::Key_S) :
			m_fpi.moveBackward = true;
			break;
		case(Qt::Key_A) :
			m_fpi.moveLeft = true;
			break;
		case(Qt::Key_D) :
			m_fpi.moveRight = true;
			break;
		case(Qt::Key_Q) :
			m_fpi.moveUp = true;
			break;
		case(Qt::Key_E) :
			m_fpi.moveDown = true;
			break;
		default:
			break;
		}

		m_translating = m_fpi.moveForward || m_fpi.moveBackward || m_fpi.moveLeft || m_fpi.moveRight || m_fpi.moveUp || m_fpi.moveDown ? true : false;
	}
}

void Camera::handleKeyRelease(QKeyEvent* event)
{
	if (conf.cam.type == FirstPerson)
	{
		switch (event->key())
		{
		case(Qt::Key_W) :
			m_fpi.moveForward = false;
			break;
		case(Qt::Key_S) :
			m_fpi.moveBackward = false;
			break;
		case(Qt::Key_A) :
			m_fpi.moveLeft = false;
			break;
		case(Qt::Key_D) :
			m_fpi.moveRight = false;
			break;
		case(Qt::Key_Q) :
			m_fpi.moveUp = false;
			break;
		case(Qt::Key_E) :
			m_fpi.moveDown = false;
			break;
		default:
			break;
		}

		m_translating = m_fpi.moveForward || m_fpi.moveBackward || m_fpi.moveLeft || m_fpi.moveRight || m_fpi.moveUp || m_fpi.moveDown ? true : false;
	}
}

void Camera::handleWheel(QWheelEvent* event)
{
	if (conf.cam.type == ModelView)
	{
		// angleDelta returns amount of vertical scrolling in 8th of degrees
		// ---> degrees = angleDelta().ry() / 8
		// It is negative if wheel was scrolled towards the user
		// One wheel tick usually is 15 degrees -> angleDelta() returns 120 for one tick
		m_mvi.zoom -= event->angleDelta().ry() / 120.0f * conf.cam.mv.zoomSpeed * m_mvi.zoom; // Amount off zoom change is dependent on current zoom/distance to the model

		m_mvi.zoom = std::max(0.01f, m_mvi.zoom); // cap minimal zoom, so we always can change zoom with the mouse wheel

		// Recompute camera position
		m_viewChanged = true;
	}
}

void Camera::computeProjectionMatrix()
{
	XMStoreFloat4x4(&m_projection, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
}