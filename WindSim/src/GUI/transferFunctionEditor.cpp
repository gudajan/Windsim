#include "transferFunctionEditor.h"

#include "staticLogger.h"

#include <QBoxLayout>
#include <QPainter>
#include <QColorDialog>
#include <QPropertyAnimation>
#include <QJsonArray>

#include <limits>

TransferFunctionEditor::TransferFunctionEditor(QWidget* parent)
	: QWidget(parent)
	, m_red(nullptr)
	, m_green(nullptr)
	, m_blue(nullptr)
	, m_alpha(nullptr)
	, m_rangeMin(nullptr)
	, m_rangeMax(nullptr)
	, m_gradient(nullptr)
	, m_metricFunctions()
	, m_currentMetric(Metric::toString(Metric::Magnitude))
{
	m_gradient = new Gradient(this);
	m_gradient->setMinimumHeight(19);

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_gradient);

	connect(m_gradient, SIGNAL(controlPointChanged(const QColor&)), this, SLOT(changeControlPoint(const QColor&)));
	connect(m_gradient, SIGNAL(gradientChanged()), this, SIGNAL(transferFunctionChanged()));

	for (const auto& name : Metric::names)
		m_metricFunctions.emplace(name, TransferFunction());
}

void TransferFunctionEditor::setColorSpinBoxes(QSpinBox* red, QSpinBox* green, QSpinBox* blue, QSpinBox* alpha)
{
	m_red = red;
	m_green = green;
	m_blue = blue;
	m_alpha = alpha;

	connect(m_red, SIGNAL(valueChanged(int)), this, SLOT(changeColor()));
	connect(m_green, SIGNAL(valueChanged(int)), this, SLOT(changeColor()));
	connect(m_blue, SIGNAL(valueChanged(int)), this, SLOT(changeColor()));
	connect(m_alpha, SIGNAL(valueChanged(int)), this, SLOT(changeColor()));
}

void TransferFunctionEditor::setRangeSpinBoxes(QDoubleSpinBox* min, QDoubleSpinBox* max)
{
	m_rangeMin = min;
	m_rangeMax = max;

	m_rangeMin->setValue(0.0);
	m_rangeMax->setValue(1000.0);

	connect(m_rangeMin, SIGNAL(valueChanged(double)), this, SLOT(changeRange()));
	connect(m_rangeMax, SIGNAL(valueChanged(double)), this, SLOT(changeRange()));
}

void TransferFunctionEditor::switchToMetric(const QString& metric)
{
	m_currentMetric = metric;

	TransferFunction& txfn = m_metricFunctions[m_currentMetric];

	m_rangeMin->setValue(txfn.rangeMin);
	m_rangeMax->setValue(txfn.rangeMax);

	m_gradient->setTransferFunction(&txfn); // Selects first control point, which emits signals for updating color spinboxes
}

void TransferFunctionEditor::changeColor()
{
	m_gradient->changeColor(QColor(m_red->value(), m_green->value(), m_blue->value(), m_alpha->value()));
}

void TransferFunctionEditor::changeRange()
{
	m_metricFunctions[m_currentMetric].rangeMin = m_rangeMin->value();
	m_metricFunctions[m_currentMetric].rangeMax = m_rangeMax->value();
	emit transferFunctionChanged();
}

void TransferFunctionEditor::changeControlPoint(const QColor& col)
{
	blockColorSignals(true);

	int d1 = col.red();
	int d2 = col.green();
	int d3 = col.blue();
	int d4 = col.alpha();

	m_red->setValue(col.red());
	m_green->setValue(col.green());
	m_blue->setValue(col.blue());
	m_alpha->setValue(col.alpha());

	blockColorSignals(false);
}

void TransferFunctionEditor::blockColorSignals(bool block)
{
	m_red->blockSignals(block);
	m_green->blockSignals(block);
	m_blue->blockSignals(block);
	m_alpha->blockSignals(block);
}

Gradient::Gradient(QWidget* parent)
	: QWidget(parent)
	, m_mousePos(0, 0)
	, m_function(nullptr)
	, m_currentControlPoint()
{
}

void Gradient::setTransferFunction(TransferFunction* fn)
{
	m_function = fn;

	m_currentControlPoint = fn->controlPoints.begin();
	emit controlPointChanged(m_currentControlPoint->second);
	emit gradientChanged();
	repaint();
}

void Gradient::changeColor(const QColor& col)
{
	m_currentControlPoint->second = col;
	emit gradientChanged();
	repaint();
}

void Gradient::paintEvent(QPaintEvent* event)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	// Paint checker pattern to visualize transparency
	int checkerSize = 5;
	QPixmap pm(checkerSize * 2, checkerSize * 2);
	QPainter pmp(&pm);
	pmp.fillRect(0, 0, checkerSize, checkerSize, Qt::lightGray);
	pmp.fillRect(checkerSize, checkerSize, checkerSize, checkerSize, Qt::lightGray);
	pmp.fillRect(0, checkerSize, checkerSize, checkerSize, Qt::darkGray);
	pmp.fillRect(checkerSize, 0, checkerSize, checkerSize, Qt::darkGray);
	pmp.end();
	QPalette pal = palette();
	pal.setBrush(backgroundRole(), QBrush(pm));
	setAutoFillBackground(true);
	setPalette(pal);

	QLinearGradient g(0,0, rect().width(), 0);
	g.setInterpolationMode(QGradient::ComponentInterpolation); // NOT DOCUMENTED ON WEBSITE!!!: Perform component-wise interpolation instead of using premultiplied alpha

	for (const auto& point : m_function->controlPoints)
		g.setColorAt(point.first, point.second);

	p.setBrush(g);
	p.setPen(Qt::NoPen);

	p.drawRect(rect());

	QPen pointPen(QColor(255, 255, 255, 191), 1);
	QBrush pointBrush(QColor(191, 191, 191, 197));
	p.setPen(pointPen);

	QPoint pointSize(11, 11);
	for (QGradientStops::iterator it = m_function->begin(); it != m_function->end(); ++it)
	{
		QPoint pos = toPixelPos(it->first);
		QRect bounds(pos - pointSize * 0.5, pos + pointSize * 0.5);
		if (it == m_currentControlPoint)
			p.setBrush(QBrush(QColor(255, 191, 191, 197)));
		else
			p.setBrush(pointBrush);
		p.drawEllipse(bounds);

		// Mark first two undeletable control points
		if (it == m_function->begin() || it == std::next(m_function->begin()))
		{
			bounds = QRect(pos - pointSize * 0.2, pos + pointSize * 0.2);
			p.setBrush(QBrush(QColor(127, 12, 12, 230)));
			p.setPen(Qt::NoPen);
			p.drawEllipse(bounds);
			p.setPen(pointPen);
		}
	}

	if (!isEnabled())
		p.fillRect(rect(), QColor(255, 255, 255, 128));
}

void Gradient::mouseMoveEvent(QMouseEvent * event)
{
	if (event->buttons() == Qt::LeftButton)
	{
		// Move current control point
		m_currentControlPoint->first = std::max(0.0, std::min(m_pointPos + toLogicalPos(event->pos() - m_mousePos), 1.0));
		emit gradientChanged();
	}

	repaint();
}

void Gradient::mousePressEvent(QMouseEvent * event)
{
	if (event->button() == Qt::LeftButton)
	{
		QGradientStops::iterator point = getPointClicked(event->pos());
		// If clicked on point and its different to current one -> Set current control point
		if (point != m_function->end() && m_currentControlPoint != point)
		{
			m_currentControlPoint = point;
			emit controlPointChanged(m_currentControlPoint->second);
		}
		// If not clicked on any point -> create new one and make it current
		else if(point == m_function->end())
		{
			// Not in vicinity, create new control point
			qreal t = toLogicalPos(event->pos());
			m_function->push_back(QGradientStop(t, m_function->colorAt(t))); // Does NOT change the gradient, because new color is interpolated
			m_currentControlPoint = std::prev(m_function->end());
			emit controlPointChanged(m_currentControlPoint->second);
		}
		// Otherwise, clicked on current control point -> do nothing else

		// Set variables for possible move events
		m_mousePos = event->pos();
		m_pointPos = m_currentControlPoint->first;
	}
	else if (event->button() == Qt::RightButton)
	{
		QGradientStops::iterator point = getPointClicked(event->pos());
		// If clicked on any point, which is not the first and second one (first -> start point, second -> end point of gradient -> not deletable)
		if (point != m_function->end() && point != m_function->begin() && point != std::next(m_function->begin()))
		{
			// If clicked on current point -> change current point to first one, because clicked point will be erased
			if (m_currentControlPoint == point)
			{
				m_currentControlPoint = m_function->begin();
				emit controlPointChanged(m_currentControlPoint->second);
			}
			// Erase operation invalidates iterators -> Store value and find it after erase
			QGradientStop tmp = *m_currentControlPoint;
			m_function->erase(point);
			m_currentControlPoint = std::find(m_function->begin(), m_function->end(), tmp);
			emit gradientChanged(); // Gradient may have changed by erasing a control point
		}
	}

	repaint();
}

void Gradient::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		// Should always hit current point, as first click of doubleClick generated point if none existed
		QGradientStops::iterator point = getPointClicked(event->pos());
		if (point != m_currentControlPoint)
			StaticLogger::logit("WARNING: Double clicked point not equal to current control point!");

		QColor col = QColorDialog::getColor(m_currentControlPoint->second, this, QString(), QColorDialog::ShowAlphaChannel); // Open color dialog
		if (col != m_currentControlPoint->second && col.isValid())
		{
			m_currentControlPoint->second = col;
			emit controlPointChanged(col);
			emit gradientChanged(); // Color of control point changed
		}
	}

	repaint();
}

QPoint Gradient::toPixelPos(qreal t)
{
	return QPoint(rect().width() * t, rect().height() * 0.5);
}

qreal Gradient::toLogicalPos(QPoint p)
{
	return p.x() / static_cast<qreal>(rect().width());
}

QGradientStops::iterator Gradient::getPointClicked(QPoint click)
{
	for (QGradientStops::iterator point = m_function->begin(); point != m_function->end(); ++point)
	{
		if ((click - toPixelPos(point->first)).manhattanLength() < 10)
		{
			return point;
		}
	}
	return m_function->end();
}
