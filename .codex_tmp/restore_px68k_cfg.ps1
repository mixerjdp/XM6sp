$path = 'D:\Emulation\Emulators\RetroArch\config\PX68K\PX68K.cfg'
$content = @"
audio_driver = "dsound"
input_load_state = "f4"
midi_output = "OFF"
video_shader_enable = "true"
"@

Set-Content -LiteralPath $path -Value $content
