$file = 'E:\MCU\esp32\xaiozhi1111111111111111111111111\xiaozhi-esp32-main\xiaozhi_slave_Thirdfloor\main\APP\Src\iot_control_task.c'
$content = Get-Content $file -Raw
$content = $content -replace '\[SERVO_45\]\s*=\s*SERVO_ANGLE_45', '[SERVO_35]  = SERVO_ANGLE_35'
$content = $content -replace '\[SERVO_90\]\s*=\s*SERVO_ANGLE_90', '[SERVO_70]  = SERVO_ANGLE_70'
[System.IO.File]::WriteAllText($file, $content)
Write-Host 'Done'
