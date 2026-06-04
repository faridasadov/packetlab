#include "projectserializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace packetlab {

namespace {

QString deviceTypeToString(DeviceType type) {
    switch (type) {
    case DeviceType::Pc:
        return "pc";
    case DeviceType::Switch:
        return "switch";
    case DeviceType::Router:
        return "router";
    }

    return "pc";
}

DeviceType deviceTypeFromString(const QString& value) {
    if (value == "switch") {
        return DeviceType::Switch;
    }
    if (value == "router") {
        return DeviceType::Router;
    }
    return DeviceType::Pc;
}

}  // namespace

bool ProjectSerializer::save(const NetworkModel& model, const QString& filePath, QString* errorMessage) {
    const QByteArray payload = serialize(model);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(payload);
    return true;
}

bool ProjectSerializer::load(NetworkModel& model, const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    return deserialize(model, file.readAll(), errorMessage);
}

QByteArray ProjectSerializer::serialize(const NetworkModel& model) {
    QJsonArray devices;
    for (const auto& device : model.devices()) {
        QJsonObject item;
        item["id"] = device.id();
        item["type"] = deviceTypeToString(device.type());
        item["name"] = device.name();
        item["x"] = device.position().x();
        item["y"] = device.position().y();
        item["ipAddress"] = device.ipAddress();
        item["subnetMask"] = device.subnetMask();
        item["defaultGateway"] = device.defaultGateway();
        QJsonArray interfaces;
        for (const auto& iface : device.interfaces()) {
            QJsonObject ifaceObject;
            ifaceObject["name"] = iface.name();
            ifaceObject["ipAddress"] = iface.ipAddress();
            ifaceObject["subnetMask"] = iface.subnetMask();
            interfaces.push_back(ifaceObject);
        }
        item["interfaces"] = interfaces;
        devices.push_back(item);
    }

    QJsonArray links;
    for (const auto& link : model.links()) {
        QJsonObject item;
        item["id"] = link.id();
        item["leftDeviceId"] = link.leftDeviceId();
        item["leftInterfaceIndex"] = link.leftInterfaceIndex();
        item["rightDeviceId"] = link.rightDeviceId();
        item["rightInterfaceIndex"] = link.rightInterfaceIndex();
        links.push_back(item);
    }

    QJsonObject root;
    root["format"] = "packetlab/project";
    root["version"] = 1;
    root["devices"] = devices;
    root["links"] = links;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ProjectSerializer::deserialize(NetworkModel& model, const QByteArray& payload, QString* errorMessage) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = parseError.errorString();
        }
        return false;
    }

    const auto root = document.object();
    const auto devices = root.value("devices").toArray();
    const auto links = root.value("links").toArray();

    model.clear();

    for (const auto& value : devices) {
        const auto object = value.toObject();
        const auto type = deviceTypeFromString(object.value("type").toString());
        auto& device = model.addDevice(type, object.value("name").toString(type == DeviceType::Pc ? "PC" : type == DeviceType::Switch ? "SW" : "R"));
        device.setName(object.value("name").toString(device.name()));
        device.setPosition(QPointF(object.value("x").toDouble(), object.value("y").toDouble()));
        device.setIpAddress(object.value("ipAddress").toString());
        device.setSubnetMask(object.value("subnetMask").toString("255.255.255.0"));
        device.setDefaultGateway(object.value("defaultGateway").toString());
        const auto interfaces = object.value("interfaces").toArray();
        for (int i = 0; i < interfaces.size(); ++i) {
            const auto ifaceObject = interfaces.at(i).toObject();
            if (auto* iface = device.interfaceAt(i)) {
                iface->setName(ifaceObject.value("name").toString(iface->name()));
                iface->setIpAddress(ifaceObject.value("ipAddress").toString());
                iface->setSubnetMask(ifaceObject.value("subnetMask").toString("255.255.255.0"));
            }
        }
    }

    for (const auto& value : links) {
        const auto object = value.toObject();
        model.addLink(
            object.value("leftDeviceId").toInt(),
            object.value("leftInterfaceIndex").toInt(),
            object.value("rightDeviceId").toInt(),
            object.value("rightInterfaceIndex").toInt());
    }

    return true;
}

}  // namespace packetlab
