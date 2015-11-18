#ifndef DX11_WIDGET_H
#define DX11_WIDGET_H

#include "common.h"

#include <QWidget>
#include <QThread>
#include <QJsonObject>

class DX11Renderer;

class DX11Widget : public QWidget
{
	Q_OBJECT

public:
	DX11Widget(QWidget * parent = nullptr, Qt::WindowFlags = 0);
	virtual ~DX11Widget();

	inline QPaintEngine* paintEngine() const override { return nullptr; }

	void addObject3D(const QJsonObject& data);
	void removeObject3D(int id);
	void removeAllObject3D();
	void reloadShaders();
	void cleanUp();
	void applySettings();

public slots:
	void logit(const QString& str);

protected:
	void paintEvent(QPaintEvent * event) override;
	void resizeEvent(QResizeEvent * event) override;

	void keyPressEvent(QKeyEvent * event) override;
	void keyReleaseEvent(QKeyEvent * event) override;
	void mouseMoveEvent(QMouseEvent * event) override;
	void mousePressEvent(QMouseEvent * event) override;
	void mouseReleaseEvent(QMouseEvent * event) override;
	void wheelEvent(QWheelEvent * event) override;

signals:
	void stopRendering();
	void resize(int width, int height);
	void controlEvent(QEvent* event);
	void addObject3DTriggered(const QJsonObject& data);
	void removeObject3DTriggered(int id);
	void removeAllObject3DTriggered();
	void reloadShadersTriggered();

private:
	QThread m_renderThread;
	DX11Renderer* m_renderer;
};

#endif