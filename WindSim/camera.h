#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>

//#include <QMouseEvent>
//#include <QKeyEvent>
//#include <QWheelEvent>
#include <QPoint>

class QMouseEvent;
class QKeyEvent;
class QWheelEvent;
class QEvent;

enum CameraType {FirstPerson, ModelView};

class Camera
{
public:
	Camera(const uint32_t width, const uint32_t height, const CameraType type = FirstPerson);
	virtual ~Camera();

	DirectX::XMFLOAT4X4 getViewMatrix() const;
	DirectX::XMFLOAT4X4 getProjectionMatrix() const;

	bool handleControlEvent(QEvent* event);
	void update(double elapsedTime);

	void setFov(const float fov) { m_fov = fov; };
	void setAspectRatio(const uint32_t width, const uint32_t height) { m_aspectRatio =  static_cast<float>(width) / static_cast<float>(height); };
	void setNearZ(const float nearZ) { m_nearZ = nearZ; };
	void setFarZ(const float farZ) { m_farZ = farZ; };

private:
	void handleMousePress(QMouseEvent* event);
	void handleMouseRelease(QMouseEvent* event);
	void handleMouseMove(QMouseEvent* event);
	void handleKeyPress(QKeyEvent* event);
	void handleKeyRelease(QKeyEvent* event);
	void handleWheel(QWheelEvent* event);

	DirectX::XMFLOAT3 m_pos;
	DirectX::XMFLOAT4 m_rot;
	float m_fov; // Field of view
	float m_aspectRatio; // Width / height
	float m_nearZ; // Near plane of view frustum
	float m_farZ; // Far plane of view frustum
	CameraType m_type;

	bool m_rotating;
	bool m_translating;

	float m_angleY;
	float m_angleX;

	struct TranslateKeys
	{
		bool moveForward;
		bool moveBackward;
		bool moveRight;
		bool moveLeft;
		bool moveUp;
		bool moveDown;
	} m_keys;

	QPoint m_gMouseCoord; // Global mouse coordinates
	QPoint m_gMouseCoordLast; // Last known global mouse coordinates

};

#endif