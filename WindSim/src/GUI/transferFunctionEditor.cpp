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
	, m_rangeRuler(new RangeRuler(this))
	, m_colorGradient(new Gradient(this))
	, m_alphaGradient(new AlphaFunction(this))
	, m_metricFunctions()
	, m_currentMetric(Metric::toString(Metric::Magnitude))
{
	m_rangeRuler->setMinimumHeight(25);
	m_colorGradient->setMinimumHeight(19);
	m_alphaGradient->setMinimumHeight(60);

	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	policy.setVerticalStretch(2);
	m_alphaGradient->setSizePolicy(policy);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_rangeRuler);
	layout->addWidget(m_alphaGradient);
	layout->addWidget(m_colorGradient);

	connect(m_colorGradient, SIGNAL(pointChanged(const QColor&)), this, SLOT(changeColorPoint(const QColor&)));
	connect(m_colorGradient, SIGNAL(gradientChanged()), this, SIGNAL(transferFunctionChanged()));
	connect(m_alphaGradient, SIGNAL(pointChanged(const QColor&)), this, SLOT(changeAlphaPoint(const QColor&)));
	connect(m_alphaGradient, SIGNAL(gradientChanged()), this, SIGNAL(transferFunctionChanged()));

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

	connect(m_rangeMin, SIGNAL(valueChanged(double)), m_rangeRuler, SLOT(setRangeMin(double)));
	connect(m_rangeMax, SIGNAL(valueChanged(double)), m_rangeRuler, SLOT(setRangeMax(double)));
}

void TransferFunctionEditor::switchToMetric(const QString& metric)
{
	m_currentMetric = metric;

	TransferFunction& txfn = m_metricFunctions[m_currentMetric];

	m_rangeMin->setValue(txfn.rangeMin);
	m_rangeMax->setValue(txfn.rangeMax);

	m_colorGradient->setFunction(&txfn.colorPoints); // Selects first control point, which emits signals for updating color spinboxes
	m_alphaGradient->setFunction(&txfn.alphaPoints);
}

void TransferFunctionEditor::changeColor()
{
	m_colorGradient->changeColor(QColor(m_red->value(), m_green->value(), m_blue->value(), 255));
	m_alphaGradient->changeColor(QColor(255, 255, 255, m_alpha->value()));
}

void TransferFunctionEditor::changeRange()
{
	m_metricFunctions[m_currentMetric].rangeMin = m_rangeMin->value();
	m_metricFunctions[m_currentMetric].rangeMax = m_rangeMax->value();
	emit transferFunctionChanged();
}

void TransferFunctionEditor::changeColorPoint(const QColor& col)
{
	blockColorSignals(true);

	m_red->setValue(col.red());
	m_green->setValue(col.green());
	m_blue->setValue(col.blue());

	blockColorSignals(false);
}

void TransferFunctionEditor::changeAlphaPoint(const QColor& col)
{
	blockColorSignals(true);

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

Gradient::Gradient(QWidget* parent, bool colorPicker)
	: QWidget(parent)
	, m_mousePos(0, 0)
	, m_function(nullptr)
	, m_currentPoint()
	, m_colorPicker(colorPicker)
{
}

void Gradient::setFunction(QGradientStops* fn)
{
	m_function = fn;

	m_currentPoint = fn->begin();
	emit pointChanged(m_currentPoint->second);
	emit gradientChanged();
	repaint();
}

void Gradient::changeColor(const QColor& col)
{
	m_currentPoint->second = col;
	emit gradientChanged();
	repaint();
}

void Gradient::paintEvent(QPaintEvent* event)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QLinearGradient g(0,0, rect().width(), 0);
	g.setInterpolationMode(QGradient::ComponentInterpolation); // NOT DOCUMENTED ON WEBSITE!!!: Perform component-wise interpolation instead of using premultiplied alpha

	for (const auto& point : *m_function)
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
		QPoint pos = toPixelPos(it->first, it->second);
		QRect bounds(pos - pointSize * 0.5, pos + pointSize * 0.5);
		if (it == m_currentPoint)
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
		// Move current control point only if not start or end point
		if (m_currentPoint == m_function->begin() || m_currentPoint == std::next(m_function->begin()))
			return;
		m_currentPoint->first = std::max(0.0, std::min(m_pointPos + toLogicalPos(event->pos() - m_mousePos), 1.0));
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
		if (point != m_function->end() && m_currentPoint != point)
		{
			m_currentPoint = point;
			emit pointChanged(m_currentPoint->second);
		}
		// If not clicked on any point -> create new one and make it current
		else if(point == m_function->end())
		{
			// Not in vicinity, create new control point
			qreal t = toLogicalPos(event->pos());
			m_function->push_back(QGradientStop(t, TransferFunction::interpolate(*m_function, t))); // Does NOT change the gradient, because new color is interpolated
			m_currentPoint = std::prev(m_function->end());
			emit pointChanged(m_currentPoint->second);
		}
		// Otherwise, clicked on current control point -> do nothing else

		// Set variables for possible move events
		m_mousePos = event->pos();
		m_pointPos = m_currentPoint->first;
	}
	else if (event->button() == Qt::RightButton)
	{
		QGradientStops::iterator point = getPointClicked(event->pos());
		// If clicked on any point, which is not the first and second one (first -> start point, second -> end point of gradient -> not deletable)
		if (point != m_function->end() && point != m_function->begin() && point != std::next(m_function->begin()))
		{
			// If clicked on current point -> change current point to first one, because clicked point will be erased
			if (m_currentPoint == point)
			{
				m_currentPoint = m_function->begin();
				emit pointChanged(m_currentPoint->second);
			}
			// Erase operation invalidates iterators -> Store value and find it after erase
			QGradientStop tmp = *m_currentPoint;
			m_function->erase(point);
			m_currentPoint = std::find(m_function->begin(), m_function->end(), tmp);
			emit gradientChanged(); // Gradient may have changed by erasing a control point
		}
	}

	repaint();
}

void Gradient::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (!m_colorPicker) return;

	if (event->button() == Qt::LeftButton)
	{
		// Should always hit current point, as first click of doubleClick generated point if none existed
		QGradientStops::iterator point = getPointClicked(event->pos());
		if (point != m_currentPoint)
			StaticLogger::logit("WARNING: Double clicked point not equal to current control point!");

		QColor col = QColorDialog::getColor(m_currentPoint->second, this); // Open color dialog
		if (col != m_currentPoint->second && col.isValid())
		{
			m_currentPoint->second = col;
			emit pointChanged(col);
			emit gradientChanged(); // Color of control point changed
		}
	}

	repaint();
}

QPoint Gradient::toPixelPos(qreal t, const QColor& color)
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
		if ((click - toPixelPos(point->first, point->second)).manhattanLength() < 15)
		{
			return point;
		}
	}
	return m_function->end();
}

AlphaFunction::AlphaFunction(QWidget* parent)
	: Gradient(parent, false)
{
}

void AlphaFunction::paintEvent(QPaintEvent* event)
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
	p.setBrush(QBrush(pm));
	p.setPen(Qt::NoPen);
	p.drawRect(rect());

	// Draw gradient for alpha visualization
	QLinearGradient g(0, 0, rect().width(), 0);
	g.setInterpolationMode(QGradient::ComponentInterpolation); // NOT DOCUMENTED ON WEBSITE!!!: Perform component-wise interpolation instead of using premultiplied alpha

	for (const auto& point : *m_function)
		g.setColorAt(point.first, point.second);

	p.setBrush(g);
	p.setPen(Qt::NoPen);
	p.drawRect(rect());

	// Draw alpha function as connected dots

	// Draw lines
	std::vector<QPoint> points;
	points.reserve(m_function->size());
	for (QGradientStops::iterator it = m_function->begin(); it != m_function->end(); ++it)
		points.push_back(toPixelPos(it->first, it->second));

	std::sort(points.begin(), points.end(), [](const QPoint& a, const QPoint& b) { return a.x() < b.x(); });
	p.setPen(QPen(QColor(0, 0, 0, 255), 2));
	p.drawPolyline(points.data(), points.size());

	// Draw points
	QBrush pointBrush(QColor(0, 0, 0, 255)); // Black
	QPen pointPen(QColor(255, 255, 255, 191), 1);
	p.setPen(pointPen);

	QPoint pointSize(11, 11);
	for (QGradientStops::iterator it = m_function->begin(); it != m_function->end(); ++it)
	{
		QPoint pos = toPixelPos(it->first, it->second);
		QRect bounds(pos - pointSize * 0.5, pos + pointSize * 0.5);
		if (it == m_currentPoint)
			p.setBrush(QBrush(QColor(120, 0, 0, 255)));
		else
			p.setBrush(pointBrush);
		p.drawEllipse(bounds);

		// Mark first two undeletable control points
		if (it == m_function->begin() || it == std::next(m_function->begin()))
		{
			bounds = QRect(pos - pointSize * 0.2, pos + pointSize * 0.2);
			p.setBrush(QBrush(QColor(130, 100, 100, 255)));
			p.setPen(Qt::NoPen);
			p.drawEllipse(bounds);
			p.setPen(pointPen);
		}
	}

	if (!isEnabled())
		p.fillRect(rect(), QColor(255, 255, 255, 128));
}

void AlphaFunction::mouseMoveEvent(QMouseEvent* event)
{
	if (event->buttons() == Qt::LeftButton)
	{
		// Move current control point (horizantally only if not start or end point)
		if (m_currentPoint != m_function->begin() && m_currentPoint != std::next(m_function->begin()))
			m_currentPoint->first = std::max(0.0, std::min(m_pointPos + toLogicalPos(event->pos() - m_mousePos), 1.0));
		m_currentPoint->second = QColor(255, 255, 255, std::max(0, std::min(m_pointAlpha + toAlpha(event->pos() - m_mousePos), 255)));
		emit pointChanged(m_currentPoint->second);
		emit gradientChanged();
	}

	repaint();
}

void AlphaFunction::mousePressEvent(QMouseEvent* event)
{
	Gradient::mousePressEvent(event);

	if (event->button() == Qt::LeftButton)
		m_pointAlpha = m_currentPoint->second.alpha();
}

QPoint AlphaFunction::toPixelPos(qreal t, const QColor& color)
{
	return QPoint(rect().width() * t, rect().height() * (1.0 - color.alphaF()));
}

int AlphaFunction::toAlpha(QPoint pos)
{
	return - pos.y() / static_cast<float>(rect().height()) * 255;
}

RangeRuler::RangeRuler(QWidget* parent)
	: QWidget(parent)
	, m_min(0.0)
	, m_max(1000.0)
{
}

void RangeRuler::paintEvent(QPaintEvent* event)
{
	int w = width();

	float range = m_max - m_min;

	float rangeToPixel = w / range;

	int nBig = 5;
	int nSmall = 3;

	float distBig = range / (nBig - 1);
	float distSmall = distBig / (nSmall + 1);

	int lineHeightBig = 10;
	int lineHeightSmall = 5;

	QPainter p(this);
	QFontMetrics fm(font());

	std::vector<QLine> lines(nBig + (nBig - 1) * nSmall);
	for (int i = 0; i < nBig; ++i)
	{
		float x = i * distBig;
		QString text = QString::number(x + m_min, 'f', 2);
		int pixel = x * rangeToPixel;
		if (i == nBig - 1) // Fake last number and line, so the last line always is drawn at the last pixel and shows the correct number despite different pixel position
		{
			pixel = w - 1;
			text = QString::number(m_max, 'f', 2);
		}

		lines[i * (nSmall + 1)] = QLine(pixel, height(), pixel, height() - lineHeightBig);
		QRect r(QPoint(pixel - fm.width(text), height() - lineHeightBig - 2 - fm.height()), QPoint(pixel + fm.width(text), height() - lineHeightBig - 2));
		if (i == 0)
			p.drawText(r, Qt::AlignRight, text);
		else if (i == nBig - 1)
			p.drawText(r, Qt::AlignLeft, text);
		else
			p.drawText(r, Qt::AlignCenter, text);
		if (i >= nBig - 1) // Last big line is last overall line -> skip following small lines
			break;
		for (int j = 1; j <= nSmall; ++j)
		{
			x = i * distBig + j * distSmall;
			pixel = x * rangeToPixel;
			lines[i * (nSmall + 1) + j] = QLine(pixel, height(), pixel, height() - lineHeightSmall);
		}
	}

	p.drawLines(lines.data(), lines.size());
}