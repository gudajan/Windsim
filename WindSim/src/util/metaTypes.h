#ifndef META_TYPES_H
#define META_TYPES_H

#include <QMetaType>
#include <unordered_set>
#include <vector>
#include <QJsonObject>

Q_DECLARE_METATYPE(std::unordered_set<int>)
Q_DECLARE_METATYPE(std::vector<QJsonObject>)

#endif