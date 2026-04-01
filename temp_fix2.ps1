$f = 'E:\MCU\esp32\xaiozhi1111111111111111111111111\xiaozhi-esp32-main\xiaozhi_slave_Thirdfloor\main\APP\Src\esp_now_receive.c'
$c = [System.IO.File]::ReadAllText($f)
$old = 'case IOT_CMD_SET_SERVO:
            if (cmd->gpio_index >= 6 && cmd->gpio_index < 6 + SERVO_COUNT &&
                ((cmd->value == 0 || cmd->value == 35 || cmd->value == 70) && servo_local_index < 2) || ((cmd->value % 45 == 0 && cmd->value <= 180) && servo_local_index >= 2)) {
                uint8_t servo_local_index = cmd->gpio_index - 6;
                g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)((cmd->value == 0 || cmd->value == 35 || cmd->value == 70) ? (cmd->value == 0 ? 0 : cmd->value == 35 ? 1 : 2) : (cmd->value / 45));
                ESP_LOGI(TAG, "Servo[%d] (GPIO%d) = %d deg", servo_local_index, cmd->gpio_index, cmd->value);
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;'
$new = 'case IOT_CMD_SET_SERVO:
            if (cmd->gpio_index >= 6 && cmd->gpio_index < 6 + SERVO_COUNT) {
                uint8_t servo_local_index = cmd->gpio_index - 6;
                if (servo_local_index < 2) {
                    if (cmd->value == 0 || cmd->value == 35 || cmd->value == 70) {
                        g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)(cmd->value == 0 ? 0 : cmd->value == 35 ? 1 : 2);
                    }
                } else {
                    if (cmd->value % 45 == 0 && cmd->value <= 180) {
                        g_device_flags.servo[servo_local_index] = (servo_angle_enum_t)(cmd->value / 45);
                    }
                }
                ESP_LOGI(TAG, "Servo[%d] (GPIO%d) = %d deg", servo_local_index, cmd->gpio_index, cmd->value);
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;'
$c = $c.Replace($old, $new)
[System.IO.File]::WriteAllText($f, $c)
Write-Host 'Done'
