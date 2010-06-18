/*  This file is part of the KDE project
    Copyright (C) 2010 Rafael Fernández López <ereslibre@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "bluedevildevice.h"
#include "bluedeviladapter.h"

#include <bluezdevice.h>

#include <QtCore/QString>

#define ENSURE_PROPERTIES_FETCHED if (!d->m_propertiesFetched) { \
                                      d->fetchProperties();      \
                                  }

namespace BlueDevil {

class Device::Private
{
public:
    Private(const QString &address, const QString &alias, quint32 deviceClass, const QString &icon,
            bool legacyPairing, const QString &name, bool paired, Device *q);

    bool ensureDeviceCreated();
    void fetchProperties();

    void _k_propertyChanged(const QString &property, const QDBusVariant &value);

    OrgBluezDeviceInterface *m_bluezDeviceInterface;
    Adapter                 *m_adapter;

    // Bluez cached properties
    QString     m_address;
    QString     m_name;
    QString     m_icon;
    quint32     m_deviceClass;
    QStringList m_UUIDs;
    bool        m_paired;
    bool        m_connected;
    bool        m_trusted;
    bool        m_blocked;
    QString     m_alias;
    bool        m_legacyPairing;
    bool        m_propertiesFetched;

    Device *const m_q;
};

Device::Private::Private(const QString &address, const QString &alias, quint32 deviceClass,
                         const QString &icon, bool legacyPairing, const QString &name, bool paired,
                         Device *q)
    : m_bluezDeviceInterface(0)
    , m_address(address)
    , m_name(name)
    , m_icon(icon)
    , m_deviceClass(deviceClass)
    , m_alias(alias)
    , m_paired(paired)
    , m_connected(false)
    , m_trusted(false)
    , m_blocked(false)
    , m_legacyPairing(legacyPairing)
    , m_propertiesFetched(false)
    , m_q(q)
{
}

bool Device::Private::ensureDeviceCreated()
{
    if (!m_bluezDeviceInterface) {
        QDBusObjectPath devicePath = m_adapter->findDevice(m_address);

        if (devicePath.path().isEmpty()) {
            devicePath = m_adapter->createDevice(m_address);
        }

        if (devicePath.path().isEmpty()) {
            return false;
        }

        m_bluezDeviceInterface = new OrgBluezDeviceInterface("org.bluez",
                                                             devicePath.path(),
                                                             QDBusConnection::systemBus(),
                                                             m_q);

        connect(m_bluezDeviceInterface, SIGNAL(DisconnectRequested()), m_q, SIGNAL(disconnectRequested()));
        connect(m_bluezDeviceInterface, SIGNAL(PropertyChanged(QString,QDBusVariant)),
                m_q, SLOT(_k_propertyChanged(QString,QDBusVariant)));
    }
    return true;
}

void Device::Private::fetchProperties()
{
    if (!ensureDeviceCreated()) {
        return;
    }

    QVariantMap properties = m_bluezDeviceInterface->GetProperties().value();

    m_connected = properties["Connected"].toBool();
    m_trusted = properties["Trusted"].toBool();
    m_blocked = properties["Blocked"].toBool();
    const QVariantList UUIDs = properties["UUIDs"].toList();
    Q_FOREACH (const QVariant &UUID, UUIDs) {
        m_UUIDs << UUID.toString();
    }

    m_propertiesFetched = true;
}

void Device::Private::_k_propertyChanged(const QString &property, const QDBusVariant &value)
{
    if (property == "Paired") {
        m_paired = value.variant().toBool();
        emit m_q->pairedChanged(m_paired);
    } else if (property == "Connected") {
        m_connected = value.variant().toBool();
        emit m_q->connectedChanged(m_connected);
    } else if (property == "Trusted") {
        m_trusted = value.variant().toBool();
        emit m_q->trustedChanged(m_trusted);
    } else if (property == "Blocked") {
        m_blocked = value.variant().toBool();
        emit m_q->blockedChanged(m_blocked);
    } else if (property == "Alias") {
        m_alias = value.variant().toString();
        emit m_q->aliasChanged(m_alias);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

Device::Device(const QString &address, const QString &alias, quint32 deviceClass,
               const QString &icon, bool legacyPairing, const QString &name, bool paired,
               Adapter *adapter)
    : QObject(adapter)
    , d(new Private(address, alias, deviceClass, icon, legacyPairing, name, paired, this))
{
    qRegisterMetaType<BlueDevil::QUInt32StringMap>("BlueDevil::QUInt32StringMap");
    qDBusRegisterMetaType<BlueDevil::QUInt32StringMap>();
    d->m_adapter = adapter;
}

Device::~Device()
{
    delete d;
}

bool Device::registerDevice()
{
    return d->ensureDeviceCreated();
}

QString Device::address() const
{
    return d->m_address;
}

QString Device::name() const
{
    return d->m_name;
}

QString Device::icon() const
{
    return d->m_icon;
}

quint32 Device::deviceClass() const
{
    return d->m_deviceClass;
}

QStringList Device::UUIDs() const
{
    ENSURE_PROPERTIES_FETCHED
    return d->m_UUIDs;
}

bool Device::isPaired() const
{
    return d->m_paired;
}

bool Device::isConnected() const
{
    ENSURE_PROPERTIES_FETCHED
    return d->m_connected;
}

bool Device::isTrusted() const
{
    ENSURE_PROPERTIES_FETCHED
    return d->m_trusted;
}

void Device::setTrusted(bool trusted)
{
    if (!d->ensureDeviceCreated()) {
        return;
    }
    d->m_bluezDeviceInterface->SetProperty("Trusted", QDBusVariant(trusted)).waitForFinished();
}

bool Device::isBlocked() const
{
    ENSURE_PROPERTIES_FETCHED
    return d->m_blocked;
}

void Device::setBlocked(bool blocked)
{
    if (!d->ensureDeviceCreated()) {
        return;
    }
    d->m_bluezDeviceInterface->SetProperty("Blocked", QDBusVariant(blocked)).waitForFinished();
}

QString Device::alias() const
{
    return d->m_alias;
}

void Device::setAlias(const QString& alias)
{
    if (!d->ensureDeviceCreated()) {
        return;
    }
    d->m_bluezDeviceInterface->SetProperty("Alias", QDBusVariant(alias)).waitForFinished();
}

Adapter *Device::adapter() const
{
    return d->m_adapter;
}

bool Device::hasLegacyPairing() const
{
    return d->m_legacyPairing;
}

QUInt32StringMap Device::discoverServices(const QString &pattern)
{
    if (!d->ensureDeviceCreated()) {
        return QUInt32StringMap();
    }
    return d->m_bluezDeviceInterface->DiscoverServices(pattern).value();
}

void Device::cancelDiscovery()
{
    if (d->m_bluezDeviceInterface) {
        d->m_bluezDeviceInterface->CancelDiscovery();
    }
}

void Device::disconnect()
{
    if (d->m_bluezDeviceInterface) {
        d->m_bluezDeviceInterface->Disconnect();
    }
}

}

#include "bluedevildevice.moc"

