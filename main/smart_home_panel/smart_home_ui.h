#ifndef SMART_HOME_UI_H
#define SMART_HOME_UI_H

#include "smart_home_state.h"

#ifdef __cplusplus
extern "C" {
#endif

void smart_home_ui_create(void);
void smart_home_ui_set_mqtt_publish(sh_mqtt_publish_fn_t publish_cb);
void smart_home_ui_update_full(const sh_state_t *state);
void smart_home_ui_update_device(sh_device_id_t id, const sh_device_state_t *device);
void smart_home_ui_update_controller(sh_controller_id_t id, const sh_device_state_t *controller);
void smart_home_ui_update_env(const sh_env_state_t *env);
void smart_home_ui_update_network(const sh_network_state_t *network);
void smart_home_ui_set_language(sh_language_t language);

#ifdef __cplusplus
}
#endif

#endif
