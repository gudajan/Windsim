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

	inline virtual QPaintEngine* paintEngine() const { return nullptr; }

	void addObject3D(const QJsonObject& data);
	void removeObject3D(const QString& name);
	void removeAllObject3D();
	void reloadShaders();
	void cleanUp();
	void applySettings();

public slots:
	virtual void logit(const QString& str);

protected:
	virtual void paintEvent(QPaintEvent * event);
	virtual void resizeEvent(QResizeEvent * event);

	virtual void keyPressEvent(QKeyEvent * event);
	virtual void keyReleaseEvent(QKeyEvent * event);
	virtual void mouseMoveEvent(QMouseEvent * event);
	virtual void mousePressEvent(QMouseEvent * event);
	virtual void mouseReleaseEvent(QMouseEvent * event);
	virtual void wheelEvent(QWheelEvent * event);
	//virtual void leaveEvent(QEvent * event); // Mouse cursur leaves widget
	//virtual void mouseDoubleClickEvent(QMouseEvent * event);
	//virtual void moveEvent(QMoveEvent * event); // When widget was moved to new position

signals:
	void stopRendering();
	void resize(int width, int height);
	void controlEvent(QEvent* event);
	void addObject3DTriggered(const QJsonObject& data);
	void removeObject3DTriggered(const QString& name);
	void removeAllObject3DTriggered();
	void reloadShadersTriggered();

private:
	QThread m_renderThread;
	DX11Renderer* m_renderer;
};

#endif