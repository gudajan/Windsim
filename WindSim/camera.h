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

	DirectX::XMFLOAT4X4 getViewMatrix() const {	return m_view; };
	DirectX::XMFLOAT4X4 getProjectionMatrix() const { return m_projection; };

	bool handleControlEvent(QEvent* event);
	void update(double elapsedTime);

	void setProjectionParams(const float fov, const uint32_t width, const uint32_t height, const float nearZ, const float farZ);

	void setFov(const float fov) { m_fov = fov; computeProjectionMatrix(); };
	void setAspectRatio(const uint32_t width, const uint32_t height) { m_aspectRatio = static_cast<float>(width) / static_cast<float>(height); computeProjectionMatrix(); };
	void setNearZ(const float nearZ) { m_nearZ = nearZ; computeProjectionMatrix(); };
	void setFarZ(const float farZ) { m_farZ = farZ; computeProjectionMatrix(); };

	void setType(const CameraType type) { m_type = type; computeViewMatrix(); };

private:
	void handleMousePress(QMouseEvent* event);
	void handleMouseRelease(QMouseEvent* event);
	void handleMouseMove(QMouseEvent* event);
	void handleKeyPress(QKeyEvent* event);
	void handleKeyRelease(QKeyEvent* event);
	void handleWheel(QWheelEvent* event);

	void computeViewMatrix();
	void computeProjectionMatrix();

	DirectX::XMFLOAT4X4 m_view;
	DirectX::XMFLOAT4X4 m_projection;
	float m_fov; // Field of view
	float m_aspectRatio; // Width / height
	float m_nearZ; // Near plane of view frustum
	float m_farZ; // Far plane of view frustum

	CameraType m_type;

	bool m_rotating;
	bool m_translating;
	bool m_viewChanged;

	struct FirstPersonInfo
	{
		DirectX::XMFLOAT3 pos; // The current position of the First Person Camera
		DirectX::XMFLOAT4 rot; // The current rotation of the camera
		float pitch; // The current rotation angle arround the global X Axis
		float yaw; // The current rotation angle arround the global Y Axis
		bool moveForward; // Currently moving forward (Default: W-Key pressed) ?
		bool moveBackward; // S
		bool moveLeft; // A
		bool moveRight; // D
		bool moveUp; // Q
		bool moveDown; // E
	} m_fpi;

	struct ModelViewerInfo
	{
		DirectX::XMFLOAT3 center; // The center, which the ModelView Camera looks at / rotates arround
		DirectX::XMFLOAT4 rot; // The current rotation of the camera
		float pitch; // The current rotation angle arround the x-axis through the rotation center
		float yaw; // The current rotation angle arround the y-axis through the rotation center
		bool flippedYawDir; // If Model (the Camera respectively) is flipped upside down, the rotation direction arround the y-Axis is switched
		float zoom; // Current zoom factor
	} m_mvi;

	QPoint m_gMouseCoord; // Global mouse coordinates
	QPoint m_gMouseCoordLast; // Last known global mouse coordinates

};

#endif