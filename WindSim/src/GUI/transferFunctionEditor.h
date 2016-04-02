#ifndef COLOR_GRADIENT_H
#define COLOR_GRADIENT_H

#include "transferFunction.h"

#include <QWidget>
#include <QSpinBox>
#include <QColor>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QJsonObject>

#include <vector>
#include <string>

class Gradient;
class RangeRuler;
class TransferFunctionEditor : public QWidget
{
	Q_OBJECT
public:
	TransferFunctionEditor(QWidget* parent = nullptr);

	void setColorSpinBoxes(QSpinBox* red, QSpinBox* green, QSpinBox* blue, QSpinBox* alpha);
	void setRangeSpinBoxes(QDoubleSpinBox* min, QDoubleSpinBox* max);

	const TransferFunction& getTransferFunction(const QString& metric) const { return m_metricFunctions.at(metric); };
	// Also update GUI if necessary
	void setTransferFunction(const QString& metric, const TransferFunction& txfn) { m_metricFunctions[metric] = txfn; if (metric == m_currentMetric) switchToMetric(metric); };

public slots:
	void switchToMetric(const QString& metric); // Switches GUI to show desired transfer function

signals:
	void transferFunctionChanged();

private slots:
	void changeColor(); // Change color of currently selected control point in current metric
	void changeRange(); // Change range of current metric
	void changeColorPoint(const QColor& col); // Change color spinboxes to the values of the selected control point on the color gradient
	void changeAlphaPoint(const QColor& alpha); // for the alpha gradient

private:
	void blockColorSignals(bool block);

	// GUI
	QSpinBox* m_red;
	QSpinBox* m_green;
	QSpinBox* m_blue;
	QSpinBox* m_alpha;

	QDoubleSpinBox* m_rangeMin;
	QDoubleSpinBox* m_rangeMax;

	RangeRuler* m_rangeRuler;

	Gradient* m_colorGradient;
	Gradient* m_alphaGradient;

	std::map<QString, TransferFunction> m_metricFunctions;
	QString m_currentMetric;
};

class Gradient : public QWidget
{
	Q_OBJECT
public:
	Gradient(QWidget *parent, bool colorPicker = true);

	void setFunction(QGradientStops* fn);

	void changeColor(const QColor& col);

signals:
	void pointChanged(const QColor& col); // Emitted if selected color point changed
	void gradientChanged(); // Emitted if control point moved

protected:
	virtual void paintEvent(QPaintEvent* event) override; // Paint color gradient and control points

	virtual void mouseMoveEvent(QMouseEvent * event) override; // If mouse clicked, move current control point
	// LeftClick -> if in vicinity of control point, select it, otherwise create new control point
	// Rightclick -> if in vicinity of control point, delete it
	virtual void mousePressEvent(QMouseEvent * event) override;
	// DoubleClick left -> if in vicinity of control point, open color picker
	void mouseDoubleClickEvent(QMouseEvent* event) override;

	void mouseReleaseEvent(QMouseEvent * event) override {};

	virtual QPoint toPixelPos(qreal t, const QColor& color); // t is position of control point -> between 0.0 and 1.0, returns pixel position within widget
	qreal toLogicalPos(QPoint p);

	QGradientStops::iterator getPointClicked(QPoint click);

	// Relevant positions on control point move start
	QPoint m_mousePos;
	qreal m_pointPos;

	QGradientStops* m_function;
	QGradientStops::iterator m_currentPoint;

private:
	bool m_colorPicker; // Indicates if color picking is enabled for the gradient
};

class AlphaFunction : public Gradient
{
public:
	AlphaFunction(QWidget* parent = nullptr);

protected:
	void paintEvent(QPaintEvent* event) override;
	void mouseMoveEvent(QMouseEvent * event) override;
	void mousePressEvent(QMouseEvent * event) override;

	QPoint toPixelPos(qreal t, const QColor& color) override;

private:
	int toAlpha(QPoint pos);

	int m_pointAlpha;
};


class RangeRuler : public QWidget
{
	Q_OBJECT
public:
	RangeRuler(QWidget* parent = nullptr);

protected:
	void paintEvent(QPaintEvent* event) override;

public slots:
	void setRangeMin(double min) { m_min = min; repaint(); };
	void setRangeMax(double max) { m_max = max; repaint(); };

private:
	double m_min;
	double m_max;
};


#endif