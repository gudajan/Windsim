#ifndef TRANSFER_FUNCTION_H
#define TRANSFER_FUNCTION_H

#include <QString>
#include <QGradientStops>
#include <QJsonObject>

#include <vector>

namespace Metric
{
	enum Volume { Magnitude = 0, Vorticity };
	extern const std::vector<QString> names;

	QString toString(Volume metric);
	Volume toMetric(const QString& metric);
};

struct TransferFunction
{
	static TransferFunction fromJson(const QJsonObject& json);
	static QColor interpolate(const QGradientStops& points, float t);

	TransferFunction();

	QJsonObject toJson() const;
	void getTexR8G8B8A8(std::vector<char>& texture, int resolution) const;

	QColor colorAt(float t) const;

	QGradientStops alphaPoints;
	QGradientStops colorPoints;
	qreal rangeMin;
	qreal rangeMax;
};


#endif
