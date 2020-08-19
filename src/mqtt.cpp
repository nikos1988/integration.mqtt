/******************************************************************************
 *
 # Copyright (C) 2020 Markus Zehnder <business@markuszehnder.ch>
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

#include "mqtt.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtDebug>
#include <QMetaType>

#include "mqtt_supportedfeatures.h"
#include "math.h"
#include "yio-interface/entities/blindinterface.h"
#include "yio-interface/entities/climateinterface.h"
#include "yio-interface/entities/lightinterface.h"
#include "yio-interface/entities/mediaplayerinterface.h"
#include "yio-interface/entities/switchinterface.h"
#include "yio-interface/entities/remoteinterface.h"

MqttPlugin::MqttPlugin() : Plugin("mqtt", USE_WORKER_THREAD) {}

Integration *MqttPlugin::createIntegration(const QVariantMap &config, EntitiesInterface *entities,
                                                    NotificationsInterface *notifications, YioAPIInterface *api,
                                                    ConfigInterface *configObj) {
    qCInfo(m_logCategory) << "Creating MQTT integration plugin" << PLUGIN_VERSION;

    return new Mqtt(config, entities, notifications, api, configObj, this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//// MQTT THREAD CLASS
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Mqtt::Mqtt(const QVariantMap &config, EntitiesInterface *entities,
                             NotificationsInterface *notifications, YioAPIInterface *api, ConfigInterface *configObj,
                             Plugin *plugin)
    : Integration(config, entities, notifications, api, configObj, plugin) {
    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == Integration::OBJ_DATA) {
            QVariantMap map = iter.value().toMap();
            m_ip = map.value(Integration::KEY_DATA_IP).toString();
        }
    }
    m_buttons = new QMap<QString, QList<Button>*>();
}

void Mqtt::messageReceived(const QByteArray &message, const QMqttTopicName &topic) {
    //qCInfo(m_logCategory) << "message received on topic: " + topic.name() + " payload: " << QString(message);
    QJsonParseError parseerror;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseerror);
    if (parseerror.error != QJsonParseError::NoError) {
        qCCritical(m_logCategory) << "JSON error:" << parseerror.errorString();
        return;
    }

    QVariantMap map = doc.toVariant().toMap();
    if (map.firstKey() == "devices") {
        qCInfo(m_logCategory) << "converting devices to map";
        QVariantMap devices = map.value("devices").toMap();
        for (QVariantMap::const_iterator device = devices.begin(); device != devices.end(); ++device) {
            QString deviceName = device.key();
            QString entityId = QString("MQTT_DEVICE.").append(deviceName);
            qCInfo(m_logCategory) << "device:" << deviceName;
            QVariantMap buttons = device.value().toMap().value("Buttons").toMap();
            QStringList supportedFeatures = QStringList();
            QStringList customFeatures = QStringList();
            for (QVariantMap::const_iterator button = buttons.begin(); button != buttons.end(); ++button) {
                QString buttonName = button.key();
                QString buttonTopic = button.value().toList()[0].toString();
                QVariant buttonPayload = button.value().toList()[1];
                QString buttonPayloadString = "";
                switch(buttonPayload.userType()) {
                    case QMetaType::QString:
                        buttonPayloadString = buttonPayload.toString();
                    break;
                    case QMetaType::QVariantMap:
                        buttonPayloadString = QString(QJsonDocument::fromVariant(buttonPayload.toMap()).toJson()).replace('\n',"");
                    break;
                }
                Button b = Button(buttonName, buttonTopic, buttonPayloadString);
                if (!m_buttons->contains(entityId)) {
                    m_buttons->insert(entityId, new QList<Button>({b}));
                } else {
                    m_buttons->value(entityId)->append(b);
                }
                //qCInfo(m_logCategory) << "button:" << buttonName << "topic:" << buttonTopic << "payload:" << buttonPayloadString;
                // TODO implement supported features
                customFeatures.append(buttonName);
            }
            qCInfo(m_logCategory) << "adding entity:" << entityId << "with custom features:" << customFeatures;
            addAvailableEntityWithCustomFeatures(entityId, "remote", integrationId(), deviceName, supportedFeatures, customFeatures);

        }
    }

//    QString m = map.value("error").toMap().value("message").toString();
//    if (m.length() > 0) {
//        qCCritical(m_logCategory) << "Message error:" << m;
//    }

//    QString type = map.value("type").toString();
//    int     id   = map.value("id").toInt();

//    if (type == "auth_required") {
//        QString auth = QString("{ \"type\": \"auth\", \"access_token\": \"%1\" }\n").arg(m_token);
//        m_webSocket->sendTextMessage(auth);
//        return;
//    }

//    if (type == "auth_ok") {
//        qCInfo(m_logCategory) << "Authentication successful";
//        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//        // FETCH STATES
//        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//        m_webSocket->sendTextMessage("{\"id\": 2, \"type\": \"get_states\"}\n");
//    }

//    if (type == "auth_invalid") {
//        qCCritical(m_logCategory) << "Invalid authentication";
//        disconnect();
//        // try again after a couple of seconds
//        m_mqttReconnectTimer->start();
//        return;
//    }

//    // FIXME magic number!
//    if (id == 2) {
//        QVariantList list = map.value("result").toList();
//        for (int i = 0; i < list.length(); i++) {
//            QVariantMap result = list.value(i).toMap();

//            // append the list of available entities
//            QString type = result.value("entity_id").toString().split(".")[0];
//            // rename type to match our own naming system
//            if (type == "cover") {
//                type = "blind";
//            } else if (type == "input_boolean") {
//                type = "switch";
//            }
//            // add entity to allAvailableEntities list
//            addAvailableEntity(
//                result.value("entity_id").toString(), type, integrationId(),
//                result.value("attributes").toMap().value("friendly_name").toString(),
//                supportedFeatures(type, result.value("attributes").toMap().value("supported_features").toInt()));

//            // update the entity
//            updateEntity(result.value("entity_id").toString(), result);
//        }

//        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//        // SUBSCRIBE TO EVENTS IN HOME ASSISTANT
//        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//        m_webSocket->sendTextMessage(
//            "{\"id\": 3, \"type\": \"subscribe_events\", \"event_type\": \"state_changed\"}\n");
//    }

//    // FIXME magic number!
//    if (type == "result" && id == 3) {
//        setState(CONNECTED);
//        qCDebug(m_logCategory) << "Subscribed to state changes";

//        // remove notifications that we don't need anymore as the integration is connected
//        m_notifications->remove("Cannot connect to Home Assistant.");

//        m_heartbeatTimer->start();
//    }

//    if (id == m_webSocketId) {
//        qCDebug(m_logCategory) << "Command successful";
//    }

//    // FIXME magic number!
//    if (type == "event" && id == 3) {
//        QVariantMap data     = map.value("event").toMap().value("data").toMap();
//        QVariantMap newState = data.value("new_state").toMap();
//        updateEntity(data.value("entity_id").toString(), newState);
//    }

//    // heartbeat
//    if (type == "pong") {
//        qCDebug(m_logCategory) << "Got heartbeat!";
//        m_heartbeatTimeoutTimer->stop();
//    }
}

void Mqtt::connect() {
    m_userDisconnect = false;
    setState(CONNECTING);
    // reset the reconnnect trial variable
    m_tries = 0;

    initOnce();
    qCInfo(m_logCategory) << "Connecting to MQTT";
    m_mqtt->connectToHost();

    setState(CONNECTED);
}

void Mqtt::initOnce() {
    // initialize QMqttClient here because it does not work in constructor (connection never finishes)
    if (!m_initialized) {
    m_initialized = true;
    qCInfo(m_logCategory) << "creating MQTT client";
    m_mqtt = new QMqttClient(this);
    m_mqtt->setHostname(m_ip);
    m_mqtt->setPort(1883);
    m_mqtt->setClientId("yio-remote-plugin");
    qCInfo(m_logCategory) << "MQTT Broker: " << m_ip  + ":" << 1883;
    QObject::connect(m_mqtt, &QMqttClient::connected, this, [this]() {
       qCInfo(m_logCategory) << "MQTT connected!";
       auto sub = m_mqtt->subscribe(QMqttTopicFilter("mqtt_urc/config/devices"), 0);
       qCInfo(m_logCategory) << "sub state:" << sub->state();
       qint32 pub = m_mqtt->publish(QMqttTopicName("mqtt_urc/config/request"), "{\"RequestConfig\":\"devices\"}");
       qCInfo(m_logCategory) << "pub id:" << pub;
    });
    QObject::connect(m_mqtt, &QMqttClient::disconnected, this, [this]() {
       qCInfo(m_logCategory) << "MQTT disconnected!";
    });
    QObject::connect(m_mqtt, &QMqttClient::stateChanged, this, [this](QMqttClient::ClientState state) {
       qCInfo(m_logCategory) << "MQTT state changed:" << state;
    });
    QObject::connect(m_mqtt, &QMqttClient::messageReceived, this, &Mqtt::messageReceived);

//    QTimer *stateTimer = new QTimer(this);
//    stateTimer->setInterval(2000);
//    QObject::connect(stateTimer,&QTimer::timeout,this,[this]() {
//        qCInfo(m_logCategory) << "client state: " << m_mqtt->state();
//    });
//    stateTimer->start();
    }
    else {
        qCInfo(m_logCategory) << "already initialized";
    }
}

void Mqtt::disconnect() {
    m_userDisconnect = true;

    qCInfo(m_logCategory) << "Disconnecting from HomeAssistant";
    m_mqtt->disconnect();

    setState(DISCONNECTED);
}

void Mqtt::enterStandby() {
    qCDebug(m_logCategory) << "Entering standby";
}

void Mqtt::leaveStandby() {
    qCDebug(m_logCategory) << "Leaving standby";
}

void Mqtt::sendCommand(const QString &type, const QString &entity_id, int command, const QVariant &param) {

}

void Mqtt::sendCustomCommand(const QString &type, const QString &entityId, int command, const QVariant &param) {
    Button button = m_buttons->value(entityId)->at(command);
    qCInfo(m_logCategory) << "sending custom command" << button.topic << button.payload;
    m_mqtt->publish(QMqttTopicName(button.topic), button.payload.toUtf8());
}
