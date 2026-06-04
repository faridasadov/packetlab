#pragma once

#include <QString>

#include "networkmodel.h"

namespace packetlab {

class ProjectSerializer {
public:
    static bool save(const NetworkModel& model, const QString& filePath, QString* errorMessage);
    static bool load(NetworkModel& model, const QString& filePath, QString* errorMessage);
    static QByteArray serialize(const NetworkModel& model);
    static bool deserialize(NetworkModel& model, const QByteArray& payload, QString* errorMessage);
};

}  // namespace packetlab
