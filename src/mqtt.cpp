/******************************************************************************
 *
 * Copyright (C) 2020 Nikolas Slottke <nikoslottke@gmail.com>
 * Copyright (C) 2020 Markus Zehnder <business@markuszehnder.ch>
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
#include <QMetaType>
#include <QtDebug>

#include "math.h"
#include "yio-interface/entities/blindinterface.h"
#include "yio-interface/entities/climateinterface.h"
#include "yio-interface/entities/lightinterface.h"
#include "yio-interface/entities/mediaplayerinterface.h"
#include "yio-interface/entities/remoteinterface.h"
#include "yio-interface/entities/switchinterface.h"

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

Mqtt::Mqtt(const QVariantMap &config, EntitiesInterface *entities, NotificationsInterface *notifications,
           YioAPIInterface *api, ConfigInterface *configObj, Plugin *plugin)
    : Integration(config, entities, notifications, api, configObj, plugin) {
    for (QVariantMap::const_iterator iter = config.begin(); iter != config.end(); ++iter) {
        if (iter.key() == Integration::OBJ_DATA) {
            QVariantMap map = iter.value().toMap();
            m_ip = map.value(Integration::KEY_DATA_IP).toString();
        }
    }
    m_mqttReconnectTimer = new QTimer(this);
    m_entityButtons = new QMap<QString, QList<Button> *>();
    m_buttonFeatureMap = new QMap<QString, QString>();
    m_buttonFeatureMap->insert("PLAY", "PLAY");
    m_buttonFeatureMap->insert("PAUSE", "PAUSE");
    m_buttonFeatureMap->insert("PLAY_PAUSE_TOGGLE", "PLAYTOGGLE");
    m_buttonFeatureMap->insert("STOP", "STOP");
    m_buttonFeatureMap->insert("FORWARD", "FORWARD");
    m_buttonFeatureMap->insert("REVERSE", "BACKWARD");
    m_buttonFeatureMap->insert("NEXT", "NEXT");
    m_buttonFeatureMap->insert("PREVIOUS", "PREVIOUS");
    m_buttonFeatureMap->insert("INFO", "INFO");
    m_buttonFeatureMap->insert("MY_RECORDINGS", "RECORDINGS");
    m_buttonFeatureMap->insert("RECORD", "RECORD");
    m_buttonFeatureMap->insert("LIVE", "LIVE");
    m_buttonFeatureMap->insert("DIGIT_0", "DIGIT_0");
    m_buttonFeatureMap->insert("DIGIT_1", "DIGIT_1");
    m_buttonFeatureMap->insert("DIGIT_2", "DIGIT_2");
    m_buttonFeatureMap->insert("DIGIT_3", "DIGIT_3");
    m_buttonFeatureMap->insert("DIGIT_4", "DIGIT_4");
    m_buttonFeatureMap->insert("DIGIT_5", "DIGIT_5");
    m_buttonFeatureMap->insert("DIGIT_6", "DIGIT_6");
    m_buttonFeatureMap->insert("DIGIT_7", "DIGIT_7");
    m_buttonFeatureMap->insert("DIGIT_8", "DIGIT_8");
    m_buttonFeatureMap->insert("DIGIT_9", "DIGIT_9");
    m_buttonFeatureMap->insert("DIGIT_10", "DIGIT_10");
    m_buttonFeatureMap->insert("DIGIT_10PLUS", "DIGIT_10plus");
    m_buttonFeatureMap->insert("DIGIT_11", "DIGIT_11");
    m_buttonFeatureMap->insert("DIGIT_12", "DIGIT_12");
    m_buttonFeatureMap->insert("DIGIT_SEPARATOR", "DIGIT_SEPARATOR");
    m_buttonFeatureMap->insert("DIGIT_ENTER", "DIGIT_ENTER");
    m_buttonFeatureMap->insert("CURSOR_UP", "CURSOR_UP");
    m_buttonFeatureMap->insert("CURSOR_DOWN", "CURSOR_DOWN");
    m_buttonFeatureMap->insert("CURSOR_LEFT", "CURSOR_LEFT");
    m_buttonFeatureMap->insert("CURSOR_RIGHT", "CURSOR_RIGHT");
    m_buttonFeatureMap->insert("CURSOR_ENTER", "CURSOR_OK");
    m_buttonFeatureMap->insert("BACK", "BACK");
    m_buttonFeatureMap->insert("MENU_HOME", "HOME");
    m_buttonFeatureMap->insert("MENU", "MENU");
    m_buttonFeatureMap->insert("EXIT", "EXIT");
    m_buttonFeatureMap->insert("APP", "APP");
    m_buttonFeatureMap->insert("POWEROFF", "POWER_OFF");
    m_buttonFeatureMap->insert("POWERON", "POWER_ON");
    m_buttonFeatureMap->insert("POWERTOGGLE", "POWER_TOGGLE");
    m_buttonFeatureMap->insert("CHANNEL_UP", "CHANNEL_UP");
    m_buttonFeatureMap->insert("CHANNEL_DOWN", "CHANNEL_DOWN");
    m_buttonFeatureMap->insert("CHANNEL_SEARCH", "CHANNEL_SEARCH");
    m_buttonFeatureMap->insert("FAVORITE", "FAVORITE");
    m_buttonFeatureMap->insert("GUIDE", "GUIDE");
    m_buttonFeatureMap->insert("FUNCTION_RED", "FUNCTION_RED");
    m_buttonFeatureMap->insert("FUNCTION_GREEN", "FUNCTION_GREEN");
    m_buttonFeatureMap->insert("FUNCTION_YELLOW", "FUNCTION_YELLOW");
    m_buttonFeatureMap->insert("FUNCTION_BLUE", "FUNCTION_BLUE");
    m_buttonFeatureMap->insert("FUNCTION_ORANGE", "FUNCTION_ORANGE");
    m_buttonFeatureMap->insert("FORMAT_16_9", "FORMAT_16_9");
    m_buttonFeatureMap->insert("FORMAT_4_3", "FORMAT_4_3");
    m_buttonFeatureMap->insert("FORMAT_AUTO", "FORMAT_AUTO");
    m_buttonFeatureMap->insert("VOLUME_UP", "VOLUME_UP");
    m_buttonFeatureMap->insert("VOLUME_DOWN", "VOLUME_DOWN");
    m_buttonFeatureMap->insert("MUTE_TOGGLE", "MUTE_TOGGLE");
    m_buttonFeatureMap->insert("SOURCE", "SOURCE");
    m_buttonFeatureMap->insert("INPUT_TUNER_1", "INPUT_TUNER_1");
    m_buttonFeatureMap->insert("INPUT_TUNER_2", "INPUT_TUNER_2");
    m_buttonFeatureMap->insert("INPUT_TUNER_Y", "INPUT_TUNER_X");
    m_buttonFeatureMap->insert("INPUT_HDMI_1", "INPUT_HDMI_1");
    m_buttonFeatureMap->insert("INPUT_HDMI_2", "INPUT_HDMI_2");
    m_buttonFeatureMap->insert("INPUT_HDMI_X", "INPUT_HDMI_X");
    m_buttonFeatureMap->insert("INPUT_X_1", "INPUT_X_1");
    m_buttonFeatureMap->insert("INPUT_X_2", "INPUT_X_2");
    m_buttonFeatureMap->insert("OUTPUT_HDMI_1", "OUTPUT_HDMI_1");
    m_buttonFeatureMap->insert("OUTPUT_HDMI_2", "OUTPUT_HDMI_2");
    m_buttonFeatureMap->insert("OUTPUT_DVI_1", "OUTPUT_DVI_1");
    m_buttonFeatureMap->insert("OUTPUT_AUDIO_X", "OUTPUT_AUDIO_X");
    m_buttonFeatureMap->insert("OUTPUT_X", "OUTPUT_X");
    m_buttonFeatureMap->insert("NETFLIX", "SERVICE_NETFLIX");
    m_buttonFeatureMap->insert("HULU", "SERVICE_HULU");
}

void Mqtt::createButtons(QVariantMap *buttons, bool updateEntity, QString entityId, QString deviceName,
                         QStringList *supportedFeatures, QStringList *customFeatures) {
    // iterate through all buttons
    for (QVariantMap::const_iterator button = buttons->begin(); button != buttons->end(); ++button) {
        QString buttonName = button.key();
        QString buttonTopic = entityId.startsWith("MQTT_DEVICE") ? button.value().toList()[0].toString()
                                                                 : button.value().toList()[1].toString();
        QVariant buttonPayload =
            entityId.startsWith("MQTT_DEVICE") ? button.value().toList()[1] : button.value().toList()[2];
        QString buttonPayloadString = "";
        switch (buttonPayload.userType()) {
            case QMetaType::QString:
                buttonPayloadString = buttonPayload.toString();
                break;
            case QMetaType::QVariantMap:
                buttonPayloadString =
                    QString(QJsonDocument::fromVariant(buttonPayload.toMap()).toJson()).replace('\n', "");
                break;
        }
        Button b = Button(buttonName, buttonTopic, buttonPayloadString);
        if (!m_entityButtons->contains(entityId)) {
            m_entityButtons->insert(entityId, new QList<Button>({b}));
        } else {
            m_entityButtons->value(entityId)->append(b);
        }

        supportedFeature(buttonName, supportedFeatures);
        customFeatures->append(buttonName);
    }
    if (updateEntity) {
        qCInfo(m_logCategory) << "updating entity:" << entityId << "with custom features:" << *customFeatures;
        // if the entity is already in the list, skip
        for (int i = 0; i < m_allAvailableEntities.length(); i++) {
            if (m_allAvailableEntities[i].toMap().value(Integration::KEY_ENTITY_ID).toString() == entityId) {
                QVariantMap entityMap = m_allAvailableEntities[i].toMap();
                entityMap[Integration::KEY_SUPPORTED_FEATURES] = *supportedFeatures;
                if (customFeatures->size() > 0) {
                    entityMap[Integration::KEY_CUSTOM_FEATURES] = *customFeatures;
                }
            }
        }
    } else {
        qCInfo(m_logCategory) << "adding entity:" << entityId << "with custom features:" << *customFeatures;
        addAvailableEntityWithCustomFeatures(entityId, "remote", integrationId(), deviceName, *supportedFeatures,
                                             *customFeatures);
    }
}

void Mqtt::handleDevices(const QVariantMap &map) {
    qCInfo(m_logCategory) << "converting devices to map";
    QVariantMap devices = map.value("devices").toMap();

    // iterate through all devices
    for (QVariantMap::const_iterator device = devices.begin(); device != devices.end(); ++device) {
        QString deviceName = device.key();
        QString entityId = QString("MQTT_DEVICE.").append(deviceName);
        bool    updateEntity = false;
        if (m_entityButtons->contains(entityId)) {
            updateEntity = true;
            m_entityButtons->value(entityId)->clear();
        }
        QStringList supportedFeatures = QStringList();
        QStringList customFeatures = QStringList();
        qCInfo(m_logCategory) << "device:" << deviceName;
        QVariantMap buttons = device.value().toMap().value("Buttons").toMap();
        createButtons(&buttons, updateEntity, entityId, deviceName, &supportedFeatures, &customFeatures);
    }
}

void Mqtt::handleActivities(const QVariantMap &map) {
    qCInfo(m_logCategory) << "converting activites to map";
    QVariantMap activities = map.value("activities").toMap();

    // iterate through all activites
    for (QVariantMap::const_iterator activity = activities.begin(); activity != activities.end(); ++activity) {
        QString activityName = activity.key();
        QString entityId = QString("MQTT_ACTIVITY.").append(activityName);
        bool    updateEntity = false;
        if (m_entityButtons->contains(entityId)) {
            updateEntity = true;
            m_entityButtons->value(entityId)->clear();
        }
        qCInfo(m_logCategory) << "activity:" << activityName;
        QStringList supportedFeatures = QStringList();
        QStringList customFeatures = QStringList();

        // activation and deactivation buttons
        QVariant activation = activity.value().toMap().value("activation").toList()[0];
        QVariant deactivation = activity.value().toMap().value("deactivation").toList()[0];
        QString  activationPayloadString = QString(QJsonDocument::fromVariant(activation).toJson()).replace('\n', "");
        QString  deactivationPayloadString =
            QString(QJsonDocument::fromVariant(deactivation).toJson()).replace('\n', "");
        qCInfo(m_logCategory) << "activation payload:" << activationPayloadString;
        qCInfo(m_logCategory) << "deactivation payload:" << deactivationPayloadString;
        Button activationButton = Button("POWERON", "mqtt_urc/activity", activationPayloadString);
        if (!m_entityButtons->contains(entityId)) {
            m_entityButtons->insert(entityId, new QList<Button>({activationButton}));
        } else {
            m_entityButtons->value(entityId)->append(activationButton);
        }
        Button deactivationButton = Button("POWEROFF", "mqtt_urc/activity", deactivationPayloadString);
        if (!m_entityButtons->contains(entityId)) {
            m_entityButtons->insert(entityId, new QList<Button>({deactivationButton}));
        } else {
            m_entityButtons->value(entityId)->append(deactivationButton);
        }
        customFeatures.append("POWER_ON");
        customFeatures.append("POWER_OFF");
        supportedFeatures.append("POWER_ON");
        supportedFeatures.append("POWER_OFF");

        // iterate through all buttons
        QVariantMap buttons = activity.value().toMap().value("buttons").toMap();
        createButtons(&buttons, updateEntity, entityId, activityName, &supportedFeatures, &customFeatures);
    }
}

void Mqtt::messageReceived(const QByteArray &message, const QMqttTopicName &topic) {
    qCInfo(m_logCategory) << "message received on topic: " + topic.name();  // + " payload: " << QString(message);

    if (topic.name() == "mqtt_urc/config/current_activity") {
        QString currentActivity = QString("MQTT_ACTIVITY.").append(QString(message));
        qCInfo(m_logCategory) << "current activity" << currentActivity;
        for (auto const &act : m_entityButtons->keys()) {
            if (act.startsWith("MQTT_ACTIVITY")) {
                if (act != currentActivity) {
                    if (m_entities->getEntityInterface(act) != nullptr && m_entities->getEntityInterface(act)->isOn()) {
                        qCInfo(m_logCategory) << "set state offline for activity" << act;
                        m_entities->getEntityInterface(act)->setState(RemoteDef::States::OFFLINE);
                    }
                } else {
                    if (m_entities->getEntityInterface(act) != nullptr &&
                        m_entities->getEntityInterface(act)->state() != RemoteDef::States::ONLINE) {
                        qCInfo(m_logCategory) << "set state online for activity" << act;
                        m_entities->getEntityInterface(act)->setState(RemoteDef::States::ONLINE);
                    }
                }
            }
        }
        return;
    }

    QJsonParseError parseerror;
    QJsonDocument   doc = QJsonDocument::fromJson(message, &parseerror);
    if (parseerror.error != QJsonParseError::NoError) {
        qCCritical(m_logCategory) << "JSON error:" << parseerror.errorString();
        return;
    }

    QVariantMap map = doc.toVariant().toMap();
    if (map.firstKey() == "devices" && topic.name() == "mqtt_urc/config/devices") {
        handleDevices(map);
    } else if (map.firstKey() == "activities" && topic.name() == "mqtt_urc/config/activities") {
        handleActivities(map);
    }
}

QString Mqtt::buttonNameToSupportedFeatures(QString buttonName) {
    QString name = buttonName.replace(' ', '_').toUpper();

    if (m_buttonFeatureMap->contains(name)) {
        return m_buttonFeatureMap->value(name);
    } else {
        return "";
    }
}

QString Mqtt::supportedFeatureToButtonName(QString supportedFeature) {
    for (auto const &key : m_buttonFeatureMap->keys()) {
        if (m_buttonFeatureMap->value(key) == supportedFeature) {
            return key;
        }
    }
    return "";
}

bool Mqtt::supportedFeature(const QString &buttonName, QStringList *supportedFeatures) {
    QString feature = buttonNameToSupportedFeatures(buttonName);
    if (feature != "") {
        supportedFeatures->append(feature);
        return true;
    } else {
        return false;
    }
}

void Mqtt::connect() {
    setState(CONNECTING);
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
        qCInfo(m_logCategory) << "MQTT Broker: " << m_ip + ":" << 1883;
        QObject::connect(m_mqtt, &QMqttClient::connected, this, [this]() {
            qCInfo(m_logCategory) << "MQTT connected!";
            m_mqttReconnectTimer->stop();
            m_mqtt->subscribe(QMqttTopicFilter("mqtt_urc/config/devices"), 0);
            m_mqtt->subscribe(QMqttTopicFilter("mqtt_urc/config/activities"), 0);
            m_mqtt->subscribe(QMqttTopicFilter("mqtt_urc/config/current_activity"), 0);

            QTimer *deviceRequestTimer = new QTimer(this);
            deviceRequestTimer->setSingleShot(true);
            QObject::connect(deviceRequestTimer, &QTimer::timeout, [=]() {
                qint32 pubDeviceRequest =
                    m_mqtt->publish(QMqttTopicName("mqtt_urc/config/request"), "{\"RequestConfig\":\"devices\"}");
                qCInfo(m_logCategory) << "pubDeviceRequest id:" << pubDeviceRequest;
                deviceRequestTimer->deleteLater();
            });

            QTimer *activityRequestTimer = new QTimer(this);
            activityRequestTimer->setSingleShot(true);
            QObject::connect(activityRequestTimer, &QTimer::timeout, [=]() {
                qint32 pubActivitiesRequest =
                    m_mqtt->publish(QMqttTopicName("mqtt_urc/config/request"), "{\"RequestConfig\":\"activities\"}");
                qCInfo(m_logCategory) << "pubActivitiesRequest id:" << pubActivitiesRequest;
                activityRequestTimer->deleteLater();
            });

            QTimer *currentActivityRequestTimer = new QTimer(this);
            currentActivityRequestTimer->setSingleShot(true);
            QObject::connect(currentActivityRequestTimer, &QTimer::timeout, [=]() {
                qint32 pubCurrentActivitiesRequest = m_mqtt->publish(QMqttTopicName("mqtt_urc/config/request"),
                                                                     "{\"RequestConfig\":\"currentActivity\"}");
                qCInfo(m_logCategory) << "pubCurrentActivitiesRequest id:" << pubCurrentActivitiesRequest;
                currentActivityRequestTimer->deleteLater();
            });

            deviceRequestTimer->start(1000);
            activityRequestTimer->start(3000);
            currentActivityRequestTimer->start(5000);
        });
        m_mqttReconnectTimer->setSingleShot(true);
        QObject::connect(m_mqttReconnectTimer, &QTimer::timeout, this, [=]() {
            qCInfo(m_logCategory) << "retry connect to MQTT";
            m_mqtt->connectToHost();
        });
        QObject::connect(m_mqtt, &QMqttClient::disconnected, this, [this]() {
            qCInfo(m_logCategory) << "MQTT disconnected!";
            if (state() != DISCONNECTED) {
                qCInfo(m_logCategory) << "starting reconnect timer";
                m_mqttReconnectTimer->start(10000);
            } else {
                qCInfo(m_logCategory) << "not starting reconnect timer (integration state is DISCONNECTED)";
            }
        });
        QObject::connect(m_mqtt, &QMqttClient::stateChanged, this, [this](QMqttClient::ClientState state) {
            qCInfo(m_logCategory) << "MQTT state changed:" << state;
        });
        QObject::connect(m_mqtt, &QMqttClient::messageReceived, this, &Mqtt::messageReceived);
    } else {
        qCInfo(m_logCategory) << "already initialized";
    }
}

void Mqtt::disconnect() {
    setState(DISCONNECTED);
    qCInfo(m_logCategory) << "Disconnecting from MQTT";
    m_mqtt->disconnectFromHost();
}

void Mqtt::enterStandby() {
    qCDebug(m_logCategory) << "Entering standby";
    disconnect();
}

void Mqtt::leaveStandby() {
    qCDebug(m_logCategory) << "Leaving standby";
    connect();
}

void Mqtt::sendCommand(const QString &type, const QString &entity_id, int command, const QVariant &param) {
    qCInfo(m_logCategory) << "sending command" << entity_id << command << param;
    EntityInterface *entity = m_entities->getEntityInterface(entity_id);
    QString          commandName = entity->getCommandName(command);
    QString          buttonName = supportedFeatureToButtonName(commandName);
    qCInfo(m_logCategory) << "command name" << commandName;
    qCInfo(m_logCategory) << "button name" << buttonName;

    QList<Button> *buttons = m_entityButtons->value(entity_id);
    qCDebug(m_logCategory) << "entity buttons:" << buttons->size();

    for (auto const &button : *buttons) {
        if (button.name.toUpper() == buttonName) {
            qCInfo(m_logCategory) << "sending command button" << button.name << button.topic << button.payload;
            m_mqtt->publish(QMqttTopicName(button.topic), button.payload.toUtf8());
            break;
        } else {
            // qCDebug(m_logCategory) << button.name.toUpper() << "!=" << buttonName;
        }
    }
}

void Mqtt::sendCustomCommand(const QString &type, const QString &entityId, int command, const QVariant &param) {
    Button button = m_entityButtons->value(entityId)->at(command);
    qCInfo(m_logCategory) << "sending custom command button" << button.name << button.topic << button.payload;
    m_mqtt->publish(QMqttTopicName(button.topic), button.payload.toUtf8());
}
