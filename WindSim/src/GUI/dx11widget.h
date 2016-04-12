#ifndef DX11_WIDGET_H
#define DX11_WIDGET_H

#include "common.h"
#include "..\3D\dx11renderer.h"
#include "textOverlay.h"

#include <QWidget>
#include <QThread>
#include <QJsonObject>
#include <QPointer>

class DX11Widget : public QWidget
{
	Q_OBJECT

public:
	DX11Widget(QWidget * parent = nullptr, Qt::WindowFlags = 0);
	virtual ~DX11Widget();

	inline QPaintEngine* paintEngine() const override { return nullptr; }

	void reloadShaders();
	void cleanUp();
	void applySettings();

	DX11Renderer* getRenderer() { return m_renderer; };

public slots:
	void logit(const QString& str);
	void drawText(const QString& str);
	void drawFps(float fps);

protected:
	void paintEvent(QPaintEvent * event) override;
	void resizeEvent(QResizeEvent * event) override;

	void keyPressEvent(QKeyEvent * event) override;
	void keyReleaseEvent(QKeyEvent * event) override;
	void mouseMoveEvent(QMouseEvent * event) override;
	void mousePressEvent(QMouseEvent * event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent * event) override;
	void wheelEvent(QWheelEvent * event) override;

	void enterEvent(QEvent* event) override;
	void leaveEvent(QEvent* event) override;

signals:
	void stopRendering();
	void resize(int width, int height);
	void mouseMove(QPoint localPos, QPoint globalPos, int modifiers);
	void mousePress(QPoint globalPos, int button, int modifiers);
	void mouseDoubleClick(QPoint globalPos, int button, int modifiers);
	void mouseRelease(QPoint globalPos, int button, int modifiers);
	void keyPress(int key);
	void keyRelease(int key);
	void wheelUse(QPoint angleDelta);
	void mouseEnter();
	void mouseLeave();

	void reloadShadersTriggered();
	void settingsChanged();

private slots:
	void enableInfoPrint() { m_printInfo = true; };

private:
	QThread m_renderThread;
	QPointer<DX11Renderer> m_renderer;
	TextOverlay* m_overlay;

	bool m_printInfo;

};

#endif