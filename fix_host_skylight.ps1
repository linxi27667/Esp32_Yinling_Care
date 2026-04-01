$file = 'e:\MCU\esp32\xaiozhi1111111111111111111111111\xiaozhi-esp32-main\main\boards\custom-esp32s3-cam\custom_esp32s3_cam.cc'
$content = Get-Content $file -Raw -Encoding UTF8
$content = $content -replace 'angle\(角度0-180\)', 'angle(天窗0/35/70度,晾衣架0/45/90/135/180度)'
$content = $content -replace '三楼:index0=左天窗,index1=右天窗,index2=晾衣架', '三楼:index0=左天窗(限0/35/70),index1=右天窗(限0/35/70),index2=晾衣架'
[System.IO.File]::WriteAllText($file, $content, [System.Text.UTF8Encoding]::new($false))
Write-Host 'Done'
