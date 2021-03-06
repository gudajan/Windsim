#include "camera.h"
#include "common.h"
#include "settings.h"

using namespace DirectX;

Camera::Camera(const uint32_t width, const uint32_t height)
	: m_view(),
	m_projection(),
	m_fov(degToRad(60.0f)),
	m_aspectRatio(static_cast<float>(width) / static_cast<float>(height)),
	m_width(width),
	m_height(height),
	m_nearZ(0.1f),
	m_farZ(1000.0f),
	m_rotating(false),
	m_translating(false),
	m_viewChanged(false),
	m_fpi({{ 0.0f, 0.0f, -conf.cam.defaultDist }, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, false, false, false, false, false, false }),
	m_mvi({{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, 0.0f, 0.0f, false, conf.cam.defaultDist }),
	m_gMouseCoord(),
	m_gMouseCoordLast()
{
	// Compute initial transformation matrices
	computeViewMatrix();
	computeProjectionMatrix();
}

Camera::~Camera()
{
}


XMFLOAT3 Camera::getCursorDir(QPoint windowCoord)
{
	// Transform cursor position from window space [0 - width/height] to screenspace [-1, 1] to viewspace to worldspace
	QPointF ssCursor = windowToScreen(windowCoord);
	XMVECTOR direction = XMVectorSet(ssCursor.x(), ssCursor.y(), 1.0f, 0.0f);

	// Transform direction from screen space to view/camera space (inverse projection)
	XMMATRIX proj = XMLoadFloat4x4(&m_projection);
	direction = XMVector3Transform(direction, XMMatrixInverse(nullptr, proj));

	// Transform from view/camera space to world space
	XMMATRIX view = XMLoadFloat4x4(&m_view);
	direction = XMVector4Transform(direction, XMMatrixInverse(nullptr, view));

	direction = XMVector3Normalize(direction);

	XMFLOAT3 d;
	XMStoreFloat3(&d, direction);

	return d;
}

XMFLOAT3 Camera::getCamPos()
{
	XMVECTOR origin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // At the camera in camera space
	XMMATRIX view = XMLoadFloat4x4(&m_view); // From world to view/camera space

	// Transform origin from view/camera space to world space (inverse view)
	origin = XMVector3Transform(origin, XMMatrixInverse(nullptr, view));

	XMFLOAT3 o;
	XMStoreFloat3(&o, origin);

	return o;
}

XMFLOAT3 Camera::getUpVector()
{
	const XMVECTOR rot = conf.cam.type == ModelView ? XMLoadFloat4(&m_mvi.rot) : XMLoadFloat4(&m_fpi.rot);
	const XMVECTOR up = XMVector3Normalize(XMVector3Rotate(XMVectorSet(0.0, 1.0, 0.0, 0.0), rot));
	XMFLOAT3 u;
	XMStoreFloat3(&u, up);
	return u;
}

XMFLOAT3 Camera::getRightVector()
{
	const XMVECTOR rot = conf.cam.type == ModelView ? XMLoadFloat4(&m_mvi.rot) : XMLoadFloat4(&m_fpi.rot);
	const XMVECTOR right = XMVector3Normalize(XMVector3Rotate(XMVectorSet(1.0, 0.0, 0.0, 0.0), rot));
	XMFLOAT3 r;
	XMStoreFloat3(&r, right);
	return r;
}

QPoint Camera::worldToWindow(XMFLOAT4 v)
{
	XMVECTOR vec = XMLoadFloat4(&v);
	vec = XMVector4Transform(vec, XMLoadFloat4x4(&m_view)); // World -> view
	vec = XMVector4Transform(vec, XMLoadFloat4x4(&m_projection)); // view -> projection
	vec /= XMVectorGetW(vec);// homogenous division: projection -> screen [-1, 1]

	QPointF screen(XMVectorGetX(vec), XMVectorGetY(vec)); // Ignore z and w coordinate as unimportand for window coordinates
	return screenToWindow(screen);
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
	m_width = width;
	m_height = height;
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

void Camera::handleMousePress(QPoint globalPos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = globalPos;

	if (conf.cam.type == FirstPerson)
	{
		if (button == Qt::LeftButton)
		{
			m_rotating = true;
		}
	}
	else // ModelView
	{
		if (button == Qt::LeftButton && modifiers == Qt::NoModifier)
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
		if (button == Qt::RightButton && modifiers == Qt::NoModifier)
		{
			m_translating = true;
		}
	}
}

void Camera::handleMouseRelease(QPoint globalPos, Qt::MouseButton button)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = globalPos;

	if (conf.cam.type == FirstPerson)
	{
		if (button == Qt::LeftButton)
		{
			m_rotating = false;
		}
	}
	else // ModelView
	{
		if (button == Qt::LeftButton)
		{
			m_rotating = false;
		}
		else if (button == Qt::RightButton)
		{
			m_translating = false;
		}
	}
}

void Camera::handleMouseMove(QPoint globalPos)
{
	m_gMouseCoordLast = m_gMouseCoord;
	m_gMouseCoord = globalPos;

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

void Camera::handleKeyPress(Qt::Key key)
{
	if (conf.cam.type == FirstPerson)
	{
		switch (key)
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

void Camera::handleKeyRelease(Qt::Key key)
{
	if (conf.cam.type == FirstPerson)
	{
		switch (key)
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

void Camera::handleWheel(QPoint angleDelta)
{
	if (conf.cam.type == ModelView)
	{
		// angleDelta is amount of vertical scrolling in 8th of degrees
		// ---> degrees = angleDelta.ry() / 8
		// It is negative if wheel was scrolled towards the user
		// One wheel tick usually is 15 degrees -> angleDelta() returns 120 for one tick
		m_mvi.zoom -= angleDelta.ry() / 120.0f * conf.cam.mv.zoomSpeed * m_mvi.zoom; // Amount off zoom change is dependent on current zoom/distance to the model

		m_mvi.zoom = std::max(0.01f, m_mvi.zoom); // cap minimal zoom, so we always can change zoom with the mouse wheel

		// Recompute camera position
		m_viewChanged = true;
	}
}

void Camera::computeProjectionMatrix()
{
	XMStoreFloat4x4(&m_projection, XMMatrixPerspectiveFovLH(m_fov, m_aspectRatio, m_nearZ, m_farZ));
}

QPointF Camera::windowToScreen(QPoint p)
{
	// In Qt window space (0, 0) is upper left
	// In screen space (-1.0, -1.0) is lower left
	// => -y
	return QPointF(static_cast<float>(p.x()) / static_cast<float>(m_width) * 2.0f - 1.0f, - (static_cast<float>(p.y()) / static_cast<float>(m_height)* 2.0f - 1.0f));
}

QPoint Camera::screenToWindow(QPointF p)
{
	return QPoint((p.x() + 1.0f) * 0.5f * m_width, -((p.y() - 1.0f) * 0.5f * m_height));
}