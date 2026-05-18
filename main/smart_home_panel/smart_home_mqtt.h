#ifndef SMART_HOME_MQTT_H
#define SMART_HOME_MQTT_H

#include "smart_home_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void smart_home_mqtt_start(void);
void smart_home_mqtt_stop(void);
void smart_home_mqtt_publish_command(sh_device_id_t id, const char *payload);
void smart_home_mqtt_set_network_connected(bool connected, const char *ssid);
const sh_state_t *smart_home_mqtt_get_state(void);

#ifdef __cplusplus
}
#endif

#endif
