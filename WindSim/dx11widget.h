#ifndef DX11_WIDGET_H
#define DX11_WIDGET_H

#include <QWidget>
#include <QThread>

class DX11Renderer;

class DX11Widget : public QWidget
{
	Q_OBJECT

public:
	DX11Widget(QWidget * parent = nullptr, Qt::WindowFlags = 0);
	virtual ~DX11Widget();

	inline virtual QPaintEngine* paintEngine() const { return nullptr; }

public slots:
	virtual void logit(const QString& str);

protected:
	//virtual void keyPressEvent(QKeyEvent * event);
	//virtual void keyReleaseEvent(QKeyEvent * event);
	//// virtual void leaveEvent(QEvent * event); // Mouse cursur leaves widget
	////virtual void mouseDoubleClickEvent(QMouseEvent * event);
	//virtual void mouseMoveEvent(QMouseEvent * event);
	//virtual void mousePressEvent(QMouseEvent * event);
	//virtual void mouseReleaseEvent(QMouseEvent * event);
	// virtual void moveEvent(QMoveEvent * event); // When widget was moved to new position
	virtual void paintEvent(QPaintEvent * event);
	virtual void resizeEvent(QResizeEvent * event);
	//virtual void wheelEvent(QWheelEvent * event);

signals:
	void stopRendering();
	void resize(int width, int height);

private:
	QThread m_renderThread;
	DX11Renderer* m_renderer;
};

#endif