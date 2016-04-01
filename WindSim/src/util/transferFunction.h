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

	TransferFunction();

	QJsonObject toJson() const;
	void getTexR8G8B8A8(std::vector<char>& texture, int resolution) const;

	QColor colorAt(float t) const;

	// Helper functions to facilitate controlpoint access
	QGradientStops::iterator begin() { return controlPoints.begin(); };
	QGradientStops::iterator end() { return controlPoints.end(); };
	void push_back(const QGradientStop& point) { controlPoints.push_back(point); };
	QGradientStops::iterator erase(QGradientStops::iterator it) { return controlPoints.erase(it); };

	QGradientStops controlPoints;
	qreal rangeMin;
	qreal rangeMax;
};


#endif
