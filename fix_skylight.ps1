$file = 'E:\MCU\esp32\xaiozhi1111111111111111111111111\xiaozhi-esp32-main\xiaozhi_slave_Thirdfloor\main\APP\Src\iot_control_task.c'
$content = Get-Content $file -Raw
$content = $content -replace 'SERVO_45\s*=\s*1', 'SERVO_35 = 1'
$content = $content -replace 'SERVO_90\s*=\s*2', 'SERVO_70 = 2'
$content = $content -replace 'SERVO_135\s*=\s*3,?\s*', ''
$content = $content -replace 'SERVO_180\s*=\s*4', ''
$content = $content -replace 'g_servo_angle_map\[5\]', 'g_servo_angle_map[3]'
$content = $content -replace '\[SERVO_135\] = SERVO_ANGLE_135,?\s*', ''
$content = $content -replace '\[SERVO_180\] = SERVO_ANGLE_180\s*', ''
[System.IO.File]::WriteAllText($file, $content)
Write-Host 'Done'
