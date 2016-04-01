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

	for (const auto& point : json["controlPoints"].toArray())
	{
		QJsonArray c = point.toObject()["color"].toArray();
		txfn.controlPoints.push_back(QGradientStop(point.toObject()["t"].toDouble(), QColor(c[0].toInt(), c[1].toInt(), c[2].toInt(), c[3].toInt())));
	}

	return txfn;
}

TransferFunction::TransferFunction()
	: controlPoints({ QPair<qreal, QColor>(0.0, QColor(0, 0, 0, 0)), QPair<qreal, QColor>(1.0, QColor(255, 255, 255, 255)) })
	, rangeMin(0.0)
	, rangeMax(1000.0)
{
}

QJsonObject TransferFunction::toJson() const
{
	QJsonObject json;

	json["range"] = QJsonObject{ { "min", rangeMin }, { "max", rangeMax } };

	QJsonArray points;
	for (const auto& point : controlPoints)
	{
		const QColor& c = point.second;
		points.append(QJsonObject{ { "t", point.first }, { "color", QJsonArray{ c.red(), c.green(), c.blue(), c.alpha() } } });
	}
	json["controlPoints"] = points;

	return json;
}

void TransferFunction::getTexR8G8B8A8(std::vector<char>& texture, int resolution) const
{
	texture.resize(resolution * 4);

	QPropertyAnimation function;
	function.setEasingCurve(QEasingCurve::Linear);
	function.setDuration(resolution);

	QPropertyAnimation::KeyValues keyValues;
	keyValues.reserve(controlPoints.size());
	for (const auto& point : controlPoints)
		keyValues.push_back(point); // Convert QColor to QVariant within the pair

	function.setKeyValues(keyValues);

	for (int i = 0; i < resolution; ++i)
	{
		function.setCurrentTime(i);
		QColor c = function.currentValue().value<QColor>();
		texture[i * 4 + 0] = static_cast<char>(c.red());
		texture[i * 4 + 1] = static_cast<char>(c.green());
		texture[i * 4 + 2] = static_cast<char>(c.blue());
		texture[i * 4 + 3] = static_cast<char>(c.alpha());
	}
}

QColor TransferFunction::colorAt(float t) const
{
	QPropertyAnimation function;
	function.setEasingCurve(QEasingCurve::Linear);
	function.setDuration(100);

	QPropertyAnimation::KeyValues keyValues;
	keyValues.reserve(controlPoints.size());
	for (const auto& point : controlPoints)
		keyValues.push_back(point); // Convert QColor to QVariant within the pair

	function.setKeyValues(keyValues);
	function.setCurrentTime(t * 100);

	return function.currentValue().value<QColor>();
}