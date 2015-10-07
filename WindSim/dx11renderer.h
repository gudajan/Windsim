#ifndef DX11_RENDERER_H
#define DX11_RENDERER_H

// Qt
#include <QWidget> // for WId
#include <QObject>

// DirectX
#include <d3d11.h>
#include <d3dx11effect.h>

class DX11Renderer : public QObject
{
	Q_OBJECT

public:

	DX11Renderer(WId hwnd, int width, int height);
	virtual ~DX11Renderer();

	virtual bool onInit(); // Initialize renderer and directx11
	virtual void onUpdate(); // Update the scene
	virtual void onRender(); // Render the scene
	virtual void onDestroy(); // Delete renderer and directx11

	int getWidth(){ return m_width; };
	int getHeight(){ return m_height; };

public slots:
	virtual void execute(); // Render loop
	virtual void onResize(int width, int height); // Resize viewport
	virtual void stop(); // Stop rendering

signals:
	void logit(const QString& str);

private:
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

	bool m_stopped; // Indicates if rendering should be stopped
};

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

#endif