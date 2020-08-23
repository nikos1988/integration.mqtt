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

void Mqtt::handleDevices(QVariantMap &map)
{
    qCInfo(m_logCategory) << "converting devices to map";
    QVariantMap devices = map.value("devices").toMap();

    // iterate through all devices
    for (QVariantMap::const_iterator device = devices.begin(); device != devices.end(); ++device) {
        QString deviceName = device.key();
        QString entityId = QString("MQTT_DEVICE.").append(deviceName);
        bool updateEntity = false;
        if (m_buttons->contains(entityId)) {
            updateEntity = true;
            m_buttons->value(entityId)->clear();
        }
        qCInfo(m_logCategory) << "device:" << deviceName;
        QVariantMap buttons = device.value().toMap().value("Buttons").toMap();
        QStringList supportedFeatures = QStringList();
        QStringList customFeatures = QStringList();

        // iterate through all buttons
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

            supportedFeature(buttonName, supportedFeatures);
            customFeatures.append(buttonName);
        }
        if (updateEntity) {
            qCInfo(m_logCategory) << "updating entity:" << entityId << "with custom features:" << customFeatures;
            // if the entity is already in the list, skip
            for (int i = 0; i < m_allAvailableEntities.length(); i++) {
                if (m_allAvailableEntities[i].toMap().value(Integration::KEY_ENTITY_ID).toString() == entityId) {
                    QVariantMap entityMap = m_allAvailableEntities[i].toMap();
                    entityMap[Integration::KEY_SUPPORTED_FEATURES] = supportedFeatures;
                    if (customFeatures.size() > 0) {
                        qWarning() << "updating custom features:" << customFeatures;
                        entityMap[Integration::KEY_CUSTOM_FEATURES] = customFeatures;
                    }
                }
            }
        } else {
            qCInfo(m_logCategory) << "adding entity:" << entityId << "with custom features:" << customFeatures;
            addAvailableEntityWithCustomFeatures(entityId, "remote", integrationId(), deviceName, supportedFeatures, customFeatures);
        }
    }
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
        handleDevices(map);
    }
}

QString Mqtt::buttonNameToSupportedFeatures(QString buttonName) {
    QString name = buttonName.replace('_',' ').toUpper();
    if (name == "PLAY")
        return "PLAY";
    if (name == "PAUSE")
        return "PAUSE";
    if (name == "PLAY PAUSE TOGGLE")
        return "PLAYTOGGLE";
    if (name == "STOP")
        return "STOP";
    if (name == "FORWARD")
        return "FORWARD";
    if (name == "REVERSE")
        return "BACKWARD";
    if (name == "NEXT")
        return "NEXT";
    if (name == "PREVIOUS")
        return "PREVIOUS";
    if (name == "INFO")
        return "INFO";
    if (name == "MY RECORDINGS")
        return "RECORDINGS";
    if (name == "RECORD")
        return "RECORD";
    if (name == "LIVE")
        return "LIVE";
    if (name == "DIGIT 0")
        return "DIGIT_0";
    if (name == "DIGIT 1")
        return "DIGIT_1";
    if (name == "DIGIT 2")
        return "DIGIT_2";
    if (name == "DIGIT 3")
        return "DIGIT_3";
    if (name == "DIGIT 4")
        return "DIGIT_4";
    if (name == "DIGIT 5")
        return "DIGIT_5";
    if (name == "DIGIT 6")
        return "DIGIT_6";
    if (name == "DIGIT 7")
        return "DIGIT_7";
    if (name == "DIGIT 8")
        return "DIGIT_8";
    if (name == "DIGIT 9")
        return "DIGIT_9";
    if (name == "DIGIT 10")
        return "DIGIT_10";
    if (name == "DIGIT 10PLUS")
        return "DIGIT_10plus";
    if (name == "DIGIT 11")
        return "DIGIT_11";
    if (name == "DIGIT 12")
        return "DIGIT_12";
    if (name == "DIGIT SEPARATOR")
        return "DIGIT_SEPARATOR";
    if (name == "DIGIT ENTER")
        return "DIGIT_ENTER";
    if (name == "CURSOR UP")
        return "CURSOR_UP";
    if (name == "CURSOR DOWN")
        return "CURSOR_DOWN";
    if (name == "CURSOR LEFT")
        return "CURSOR_LEFT";
    if (name == "CURSOR RIGHT")
        return "CURSOR_RIGHT";
    if (name == "CURSOR ENTER")
        return "CURSOR_OK";
    if (name == "BACK")
        return "BACK";
    if (name == "MENU HOME")
        return "HOME";
    if (name == "MENU")
        return "MENU";
    if (name == "EXIT")
        return "EXIT";
    if (name == "APP")
        return "APP";
    if (name == "POWEROFF")
        return "POWER_OFF";
    if (name == "POWERON")
        return "POWER_ON";
    if (name == "POWERTOGGLE")
        return "POWER_TOGGLE";
    if (name == "CHANNEL UP")
        return "CHANNEL_UP";
    if (name == "CHANNEL DOWN")
        return "CHANNEL_DOWN";
    if (name == "CHANNEL SEARCH")
        return "CHANNEL_SEARCH";
    if (name == "FAVORITE")
        return "FAVORITE";
    if (name == "GUIDE")
        return "GUIDE";
    if (name == "FUNCTION RED")
        return "FUNCTION_RED";
    if (name == "FUNCTION GREEN")
        return "FUNCTION_GREEN";
    if (name == "FUNCTION YELLOW")
        return "FUNCTION_YELLOW";
    if (name == "FUNCTION BLUE")
        return "FUNCTION_BLUE";
    if (name == "FUNCTION ORANGE")
        return "FUNCTION_ORANGE";
    if (name == "FORMAT 16 9")
        return "FORMAT_16_9";
    if (name == "FORMAT 4 3")
        return "FORMAT_4_3";
    if (name == "FORMAT AUTO")
        return "FORMAT_AUTO";
    if (name == "VOLUME UP")
        return "VOLUME_UP";
    if (name == "VOLUME DOWN")
        return "VOLUME_DOWN";
    if (name == "MUTE TOGGLE")
        return "MUTE_TOGGLE";
    if (name == "SOURCE")
        return "SOURCE";
    if (name == "INPUT TUNER 1")
        return "INPUT_TUNER_1";
    if (name == "INPUT TUNER 2")
        return "INPUT_TUNER_2";
    if (name == "INPUT TUNER Y")
        return "INPUT_TUNER_X";
    if (name == "INPUT HDMI 1")
        return "INPUT_HDMI_1";
    if (name == "INPUT HDMI 2")
        return "INPUT_HDMI_2";
    if (name == "INPUT HDMI X")
        return "INPUT_HDMI_X";
    if (name == "INPUT X 1")
        return "INPUT_X_1";
    if (name == "INPUT X 2")
        return "INPUT_X_2";
    if (name == "OUTPUT HDMI 1")
        return "OUTPUT_HDMI_1";
    if (name == "OUTPUT HDMI 2")
        return "OUTPUT_HDMI_2";
    if (name == "OUTPUT DVI 1")
        return "OUTPUT_DVI_1";
    if (name == "OUTPUT AUDIO X")
        return "OUTPUT_AUDIO_X";
    if (name == "OUTPUT X")
        return "OUTPUT_X";
    if (name == "NETFLIX")
        return "SERVICE_NETFLIX";
    if (name == "HULU")
        return "SERVICE_HULU";

    return "";
}

bool Mqtt::supportedFeature(QString &buttonName, QStringList &supportedFeatures) {
    QString feature = buttonNameToSupportedFeatures(buttonName);
    if (feature != "") {
        supportedFeatures.append(feature);
        return true;
    } else {
        return false;
    }
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
