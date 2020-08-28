/******************************************************************************
 *
 * Copyright (C) 2020 Nikolas Slottke <nikoslottke@gmail.com>
 * Copyright (C) 2019 Marton Borzak <hello@martonborzak.com>
 * Copyright (C) 2019 Christian Riedl <ric@rts.co.at>
 *
 * This file is part of the YIO-Remote software project.
 *
 * YIO-Remote software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * YIO-Remote software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with YIO-Remote software. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *****************************************************************************/

#pragma once

#include <QColor>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <QtMqtt/QMqttClient>

#include "yio-interface/configinterface.h"
#include "yio-interface/entities/entitiesinterface.h"
#include "yio-interface/entities/entityinterface.h"
#include "yio-interface/notificationsinterface.h"
#include "yio-interface/plugininterface.h"
#include "yio-interface/yioapiinterface.h"
#include "yio-plugin/integration.h"
#include "yio-plugin/plugin.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// MQTT FACTORY
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const bool USE_WORKER_THREAD = true;

class MqttPlugin : public Plugin {
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "YIO.PluginInterface" FILE "mqtt.json")

 public:
    MqttPlugin();

    // Plugin interface
 protected:
    Integration* createIntegration(const QVariantMap& config, EntitiesInterface* entities,
                                   NotificationsInterface* notifications, YioAPIInterface* api,
                                   ConfigInterface* configObj) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// MQTT CLASS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Mqtt : public Integration {
    Q_OBJECT

 public:
    Mqtt(const QVariantMap& config, EntitiesInterface* entities, NotificationsInterface* notifications,
         YioAPIInterface* api, ConfigInterface* configObj, Plugin* plugin);

    void sendCommand(const QString& type, const QString& entityId, int command, const QVariant& param) override;
    void sendCustomCommand(const QString& type, const QString& entityId, int command, const QVariant& param) override;

    struct Button {
        Button(QString name, QString topic, QString payload) : name(name), topic(topic), payload(payload) {}
        QString name;
        QString topic;
        QString payload;
    };

 public slots:  // NOLINT open issue: https://github.com/cpplint/cpplint/pull/99
    void connect() override;
    void disconnect() override;
    void enterStandby() override;
    void leaveStandby() override;

    void messageReceived(const QByteArray& message, const QMqttTopicName& topic);

 private:
    QString                        m_ip;
    QMqttClient*                   m_mqtt;
    bool                           m_initialized = false;
    QMap<QString, QList<Button>*>* m_entityButtons;
    QMap<QString, QString>*        m_buttonFeatureMap;
    QTimer*                        m_mqttReconnectTimer;
    void                           handleDevices(const QVariantMap& map);
    void                           handleActivities(const QVariantMap& map);
    void                           initOnce();
    QString                        buttonNameToSupportedFeatures(const QString buttonName);
    QString                        supportedFeatureToButtonName(const QString supportedFeature);
    bool                           supportedFeature(const QString& buttonName, QStringList* supportedFeatures);
    void createButtons(QVariantMap* buttons, bool updateEntity, QString entityId, QString deviceName,
                       QStringList* supportedFeatures, QStringList* customFeatures);
};
