#include "projectserializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace packetlab {

namespace {

DeviceType deviceTypeFromString(const QString& value) {
    if (value == "laptop")    return DeviceType::Laptop;
    if (value == "server")    return DeviceType::Server;
    if (value == "ipphone")   return DeviceType::IpPhone;
    if (value == "ipcamera")  return DeviceType::IpCamera;
    if (value == "wirelessap")return DeviceType::WirelessAp;
    if (value == "switch")    return DeviceType::Switch;
    if (value == "switchl3")  return DeviceType::SwitchL3;
    if (value == "router")    return DeviceType::Router;
    if (value == "firewall")  return DeviceType::Firewall;
    return DeviceType::Pc;
}

QString baseNameForType(DeviceType type) {
    switch (type) {
    case DeviceType::Pc:        return "PC";
    case DeviceType::Laptop:    return "LAP";
    case DeviceType::Server:    return "SRV";
    case DeviceType::IpPhone:   return "PH";
    case DeviceType::IpCamera:  return "CAM";
    case DeviceType::WirelessAp:return "AP";
    case DeviceType::Switch:    return "SW";
    case DeviceType::SwitchL3:  return "L3";
    case DeviceType::Router:    return "R";
    case DeviceType::Firewall:  return "FW";
    }
    return "PC";
}

} // namespace

bool ProjectSerializer::save(const NetworkModel& model, const QString& filePath, QString* errorMessage) {
    const QByteArray payload = serialize(model);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    file.write(payload);
    return true;
}

bool ProjectSerializer::load(NetworkModel& model, const QString& filePath, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return deserialize(model, file.readAll(), errorMessage);
}

QByteArray ProjectSerializer::serialize(const NetworkModel& model) {
    QJsonArray devices;
    for (const auto& device : model.devices()) {
        QJsonObject item;
        item["id"]             = device.id();
        item["type"]           = device.typeId();
        item["name"]           = device.name();
        item["x"]              = device.position().x();
        item["y"]              = device.position().y();
        item["ipAddress"]      = device.ipAddress();
        item["subnetMask"]     = device.subnetMask();
        item["defaultGateway"] = device.defaultGateway();

        QJsonArray interfaces;
        for (const auto& iface : device.interfaces()) {
            QJsonObject o;
            o["name"]       = iface.name();
            o["ipAddress"]  = iface.ipAddress();
            o["subnetMask"] = iface.subnetMask();
            interfaces.push_back(o);
        }
        item["interfaces"] = interfaces;

        QJsonArray routes;
        for (const auto& route : device.routingTable()) {
            QJsonObject r;
            r["destination"] = route.destination;
            r["mask"]        = route.mask;
            r["nextHop"]     = route.nextHop;
            r["metric"]      = route.metric;
            routes.push_back(r);
        }
        item["routingTable"] = routes;

        devices.push_back(item);
    }

    QJsonArray links;
    for (const auto& link : model.links()) {
        QJsonObject item;
        item["id"]                 = link.id();
        item["leftDeviceId"]       = link.leftDeviceId();
        item["leftInterfaceIndex"] = link.leftInterfaceIndex();
        item["rightDeviceId"]      = link.rightDeviceId();
        item["rightInterfaceIndex"]= link.rightInterfaceIndex();
        links.push_back(item);
    }

    QJsonObject root;
    root["format"]  = "packetlab/project";
    root["version"] = 1;
    root["devices"] = devices;
    root["links"]   = links;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

bool ProjectSerializer::deserialize(NetworkModel& model, const QByteArray& payload, QString* errorMessage) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) *errorMessage = parseError.errorString();
        return false;
    }

    const auto root    = document.object();
    const auto devices = root.value("devices").toArray();
    const auto links   = root.value("links").toArray();
    model.clear();

    for (const auto& value : devices) {
        const auto object = value.toObject();
        const auto type   = deviceTypeFromString(object.value("type").toString());
        auto& device = model.addDevice(type, baseNameForType(type));
        device.setName(object.value("name").toString(device.name()));
        device.setPosition(QPointF(object.value("x").toDouble(), object.value("y").toDouble()));
        device.setIpAddress(object.value("ipAddress").toString());
        device.setSubnetMask(object.value("subnetMask").toString("255.255.255.0"));
        device.setDefaultGateway(object.value("defaultGateway").toString());

        const auto interfaces = object.value("interfaces").toArray();
        for (int i = 0; i < interfaces.size(); ++i) {
            const auto ifaceObj = interfaces.at(i).toObject();
            if (auto* iface = device.interfaceAt(i)) {
                iface->setName(ifaceObj.value("name").toString(iface->name()));
                iface->setIpAddress(ifaceObj.value("ipAddress").toString());
                iface->setSubnetMask(ifaceObj.value("subnetMask").toString("255.255.255.0"));
            }
        }

        const auto routes = object.value("routingTable").toArray();
        for (const auto& rv : routes) {
            const auto ro = rv.toObject();
            RouteEntry entry;
            entry.destination = ro.value("destination").toString();
            entry.mask        = ro.value("mask").toString();
            entry.nextHop     = ro.value("nextHop").toString();
            entry.metric      = ro.value("metric").toInt(1);
            if (!entry.destination.isEmpty() && !entry.mask.isEmpty())
                device.routingTable().push_back(entry);
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

} // namespace packetlab
