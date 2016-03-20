#ifndef META_TYPES_H
#define META_TYPES_H

#include <QMetaType>
#include <unordered_set>
#include <vector>
#include <QJsonObject>

#include <DirectXMath.h>

Q_DECLARE_METATYPE(std::unordered_set<int>)
Q_DECLARE_METATYPE(std::vector<QJsonObject>)
Q_DECLARE_METATYPE(DirectX::XMUINT3);
Q_DECLARE_METATYPE(DirectX::XMFLOAT3);

#endif