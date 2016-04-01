#include "transferFunction.h"

#include <QJsonArray>
#include <QPropertyAnimation>

namespace Metric
{
	const std::vector<QString> names{ "Magnitude", "Vorticity" };

	QString toString(Volume metric)
	{
		return names[metric];
	}

	Volume toMetric(const QString& metric)
	{
		for (int i = 0; i < names.size(); ++i)
		{
			if (names[i] == metric)
				return Volume(i);
		}
		return Magnitude;
	}
}

TransferFunction TransferFunction::fromJson(const QJsonObject& json)
{
	TransferFunction txfn;

	txfn.rangeMin = json["range"].toObject()["min"].toDouble();
	txfn.rangeMax = json["range"].toObject()["max"].toDouble();

	for (const auto& point : json["colorPoints"].toArray())
	{
		QJsonArray c = point.toObject()["color"].toArray();
		txfn.colorPoints.push_back(QGradientStop(point.toObject()["t"].toDouble(), QColor(c[0].toInt(), c[1].toInt(), c[2].toInt(), 255)));
	}
	for (const auto& point : json["alphaPoints"].toArray())
		txfn.alphaPoints.push_back(QGradientStop(point.toObject()["t"].toDouble(), QColor(255, 255, 255, point.toObject()["alpha"].toInt())));

	return txfn;
}

QColor TransferFunction::interpolate(const QGradientStops& points, float t)
{
	int resolution = 256;

	QPropertyAnimation function;
	function.setEasingCurve(QEasingCurve::Linear);
	function.setDuration(resolution);

	QPropertyAnimation::KeyValues keyValues;
	keyValues.reserve(points.size());
	for (const auto& point : points)
		keyValues.push_back(point); // Convert QColor to QVariant within the pair
	function.setKeyValues(keyValues);


	function.setCurrentTime(t * resolution);

	return function.currentValue().value<QColor>();
}

TransferFunction::TransferFunction()
	: colorPoints({ QPair<qreal, QColor>(0.0, QColor(0, 0, 0, 255)), QPair<qreal, QColor>(1.0, QColor(255, 255, 255, 255)) })
	, alphaPoints({ QPair<qreal, QColor>(0.0, QColor(255, 255, 255, 0)), QPair<qreal, QColor>(1.0, QColor(255, 255, 255, 255)) })
	, rangeMin(0.0)
	, rangeMax(1000.0)
{
}

QJsonObject TransferFunction::toJson() const
{
	QJsonObject json;

	json["range"] = QJsonObject{ { "min", rangeMin }, { "max", rangeMax } };

	QJsonArray points;
	for (const auto& point : colorPoints)
	{
		const QColor& c = point.second;
		points.append(QJsonObject{ { "t", point.first }, { "color", QJsonArray{ c.red(), c.green(), c.blue() } } });
	}
	json["colorPoints"] = points;

	points = QJsonArray();
	for (const auto& point : alphaPoints)
		points.append(QJsonObject{ { "t", point.first }, { "alpha", point.second.alpha() } });
	json["alphaPoints"] = points;

	return json;
}

void TransferFunction::getTexR8G8B8A8(std::vector<char>& texture, int resolution) const
{
	texture.resize(resolution * 4);

	QPropertyAnimation colorFunction;
	colorFunction.setEasingCurve(QEasingCurve::Linear);
	colorFunction.setDuration(resolution);

	QPropertyAnimation alphaFunction;
	alphaFunction.setEasingCurve(QEasingCurve::Linear);
	alphaFunction.setDuration(resolution);

	QPropertyAnimation::KeyValues keyValues;
	keyValues.reserve(colorPoints.size());
	for (const auto& point : colorPoints)
		keyValues.push_back(point); // Convert QColor to QVariant within the pair

	colorFunction.setKeyValues(keyValues);

	keyValues.clear();
	keyValues.reserve(alphaPoints.size());
	for (const auto& point : alphaPoints)
		keyValues.push_back(point);

	alphaFunction.setKeyValues(keyValues);

	for (int i = 0; i < resolution; ++i)
	{
		colorFunction.setCurrentTime(i);
		alphaFunction.setCurrentTime(i);
		QColor c = colorFunction.currentValue().value<QColor>();
		texture[i * 4 + 0] = static_cast<char>(c.red());
		texture[i * 4 + 1] = static_cast<char>(c.green());
		texture[i * 4 + 2] = static_cast<char>(c.blue());
		texture[i * 4 + 3] = static_cast<char>(alphaFunction.currentValue().value<QColor>().alpha());
	}
}

QColor TransferFunction::colorAt(float t) const
{
	int resolution = 256;

	QPropertyAnimation colorFunction;
	colorFunction.setEasingCurve(QEasingCurve::Linear);
	colorFunction.setDuration(resolution);

	QPropertyAnimation alphaFunction;
	alphaFunction.setEasingCurve(QEasingCurve::Linear);
	alphaFunction.setDuration(resolution);

	QPropertyAnimation::KeyValues keyValues;
	keyValues.reserve(colorPoints.size());
	for (const auto& point : colorPoints)
		keyValues.push_back(point); // Convert QColor to QVariant within the pair

	colorFunction.setKeyValues(keyValues);

	keyValues.clear();
	keyValues.reserve(alphaPoints.size());
	for (const auto& point : alphaPoints)
		keyValues.push_back(point);

	alphaFunction.setKeyValues(keyValues);

	colorFunction.setCurrentTime(t * resolution);
	alphaFunction.setCurrentTime(t * resolution);

	QColor c(colorFunction.currentValue().value<QColor>().rgb());
	c.setAlpha(alphaFunction.currentValue().value<QColor>().alpha());

	return c;
}

