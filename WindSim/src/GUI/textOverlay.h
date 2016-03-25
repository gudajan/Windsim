#ifndef TEXT_OVERLAY_H
#define TEXT_OVERLAY_H

#include <QDialog>
#include <QString>
#include <QTimer>

class TextOverlay : public QDialog
{
	Q_OBJECT
public:
	TextOverlay(QWidget* parent = nullptr);

	void setFps(float fps)
	{
		m_fps = "FPS: " + QString::number(fps, 'f', 2);
	};
	void setText(const QString& str) { m_text = str; };

	void showText(bool showText);

protected:
	void paintEvent(QPaintEvent * event) override;

private slots:
	void moveToParent();

private:
	QString m_text;
	QRect m_textBack;
	QString m_fps;
	QRect m_fpsBack;

	QTimer m_timer;

	bool m_showText;
};

#endif