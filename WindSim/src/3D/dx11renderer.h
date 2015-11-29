#ifndef DX11_RENDERER_H
#define DX11_RENDERER_H

#include "common.h"
#include "camera.h"
#include "objectManager.h"
#include "transformMachine.h"

// Qt
#include <QWidget> // for WId
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonObject>

// DirectX
#include <d3d11.h>
#include <d3dx11effect.h>

namespace State
{
	enum RendererState { CameraMove, Modifying, Default };
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
	void mouseEnter() { m_containsCursor = true; };
	void mouseLeave() { m_containsCursor = false; };
	Camera* getCamera() { return &m_camera; };

public slots:
	void frame(); // Compute one Frame

	// Start/Stop rendering
	void execute(); // Enter Render loop
	void stop(); // Stop Render loop

	// Arbitrary Events
	void onResize(int width, int height); // Resize viewport
	void onMouseMove(QMouseEvent* event);
	void onMousePress(QMouseEvent* event);
	void onMouseRelease(QMouseEvent* event);
	void onKeyPress(QKeyEvent* event);
	void onKeyRelease(QKeyEvent* event);


	void onAddObject(const QJsonObject& data);
	void onModifyObject(const QJsonObject& data);
	void onRemoveObject(int name);
	void onRemoveAll();
	void onSelectionChanged(std::unordered_set<int> ids);
	bool reloadShaders(); // Recompile and load all shaders

signals:
	void logit(const QString& str);
	void selectionChanged(std::unordered_set<int> ids);
	void modify(std::vector<QJsonObject> data);

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

	QElapsedTimer m_elapsedTimer;
	QTimer m_renderTimer;

	Camera m_camera;
	ObjectManager m_manager;
	TransformMachine m_transformer;

};

#endif