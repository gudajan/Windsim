#ifndef DX11_RENDERER_H
#define DX11_RENDERER_H

#include <d3d11.h>

#include "common.h"
#include "metaTypes.h"
#include "camera.h"
#include "objectManager.h"
#include "transformMachine.h"
#include "logger.h"

// Qt
#include <QWidget> // for WId
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonObject>

#include <deque>

namespace State
{
	enum RendererState { CameraMove, Modifying, Default, ShaderError };
}

class DX11Renderer : public QObject
{
	Q_OBJECT

public:

	DX11Renderer(WId hwnd, int width, int height, QObject* parent = nullptr);
	 ~DX11Renderer();

	bool init(); // Initialize renderer and directx11

	int getWidth(){ return m_width; };
	int getHeight(){ return m_height; };
	Camera* getCamera() { return &m_camera; };
	Logger* getLogger() { return &m_logger; };
	ID3D11Device* getDevice() { return m_device; };

	void drawInfo(const QString& info);

public slots:
	void frame(); // Compute one Frame
	void issueVoxelization();

	// Start/Stop rendering
	void execute(); // Enter Render loop
	void stop(); // Stop Render loop
	void cont(); // Continue Render loop
	void pause(); // Pause Render loop

	// Arbitrary Events
	bool onResize(int width, int height); // Resize viewport
	void onMouseMove(QPoint localPos, QPoint globalPos, int modifiers);
	void onMousePress(QPoint globalPos, int button, int modifiers);
	void onMouseRelease(QPoint globalPos, int button, int modifiers);
	void onKeyPress(int key);
	void onKeyRelease(int key);
	void onWheelUse(QPoint angleDelta);
	void onMouseEnter() { m_containsCursor = true; };
	void onMouseLeave() { m_containsCursor = false; };

	void onAddObject(const QJsonObject& data);
	void onModifyObject(const QJsonObject& data);
	void onRemoveObject(int name);
	void onRemoveAll();
	void onTriggerFunction(const QJsonObject& data);
	void onSelectionChanged(std::unordered_set<int> ids);
	bool reloadShaders(); // Recompile and load all shaders
	void changeSettings(); // Update everything, which depends on ini-values

signals:
	void updateFPS(int fps);
	void selectionChanged(std::unordered_set<int> ids);
	void modify(std::vector<QJsonObject> data);
	void drawText(const QString& str);

private:
	bool createShaders(); // Load all shaders
	void destroy(); // Delete renderer and directx11

	// Called per frame
	void update(double elapsedTime); // Update the scene
	void render(double elapsedTime); // Render the scene

	WId m_windowHandle;

	ID3D11Device* m_device;
	ID3D11DeviceContext* m_context;
	IDXGISwapChain* m_swapChain;
	ID3D11Texture2D* m_depthStencilBuffer;
	ID3D11RenderTargetView* m_renderTargetView;
	ID3D11DepthStencilView* m_depthStencilView;
	ID3D11RasterizerState* m_rasterizerState;

	// Viewport resolution
	int m_width;
	int m_height;

	bool m_containsCursor;
	QPoint m_localCursorPos;
	int m_pressedId; // Id of the object, which was hovered during last mouse press
	State::RendererState m_state;
	bool m_renderingPaused;

	std::deque<float> m_elapsedTimes;
	float m_currentFPS;

	QElapsedTimer m_elapsedTimer;
	QTimer m_renderTimer;
	QTimer m_voxelizationTimer;

	Camera m_camera;
	ObjectManager m_manager;
	TransformMachine m_transformer;
	Logger m_logger;

};

#endif