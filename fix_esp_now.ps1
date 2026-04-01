$file = 'E:\MCU\esp32\xaiozhi1111111111111111111111111\xiaozhi-esp32-main\xiaozhi_slave_Thirdfloor\main\APP\Src\esp_now_receive.c'
$content = Get-Content $file -Raw
$content = $content -replace 'cmd->value % 45 == 0 && cmd->value <= 180', '(cmd->value == 0 || cmd->value == 35 || cmd->value == 70)'
$content = $content -replace '\(servo_angle_enum_t\)\(cmd->value / 45\)', '(servo_angle_enum_t)(cmd->value == 0 ? 0 : cmd->value == 35 ? 1 : 2)'
[System.IO.File]::WriteAllText($file, $content)
Write-Host 'Done'
