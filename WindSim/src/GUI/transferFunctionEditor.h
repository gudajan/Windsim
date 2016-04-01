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
	void changeControlPoint(const QColor& col); // Change color spinboxes to the values of the selected control point

private:
	void blockColorSignals(bool block);

	// GUI
	QSpinBox* m_red;
	QSpinBox* m_green;
	QSpinBox* m_blue;
	QSpinBox* m_alpha;

	QDoubleSpinBox* m_rangeMin;
	QDoubleSpinBox* m_rangeMax;

	Gradient* m_gradient;

	std::map<QString, TransferFunction> m_metricFunctions;
	QString m_currentMetric;
};

class Gradient : public QWidget
{
	Q_OBJECT
public:
	Gradient(QWidget *parent);

	void setTransferFunction(TransferFunction* fn);

	void changeColor(const QColor& col);

signals:
	void controlPointChanged(const QColor& col); // Emitted if selected control point changed
	void gradientChanged(); // Emitted if control point moved

protected:
	void paintEvent(QPaintEvent* event) override; // Paint color gradient and control points

	void mouseMoveEvent(QMouseEvent * event) override; // If mouse clicked, move current control point
	// LeftClick -> if in vicinity of control point, select it, otherwise create new control point
	// Rightclick -> if in vicinity of control point, delete it
	void mousePressEvent(QMouseEvent * event) override;
	// DoubleClick left -> if in vicinity of control point, open color picker
	void mouseDoubleClickEvent(QMouseEvent* event) override;

	void mouseReleaseEvent(QMouseEvent * event) override {};

private:
	QPoint toPixelPos(qreal t); // t is position of control point -> between 0.0 and 1.0, returns pixel position within widget
	qreal toLogicalPos(QPoint p);

	QGradientStops::iterator getPointClicked(QPoint click);

	// Relevant positions on control point move start
	QPoint m_mousePos;
	qreal m_pointPos;

	TransferFunction* m_function;
	QGradientStops::iterator m_currentControlPoint;
};


#endif