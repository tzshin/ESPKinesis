```
usage: 
  Windows (PowerShell):
    $env:PROGRAM_ARGS="--mode <build_mode>"; pio run -e <platformio_env> -t <target>
  Windows (CMD):
    set "PROGRAM_ARGS=--mode <build_mode>" && pio run -e <platformio_env> -t <target>
  Unix (Linux/macOS):
    PROGRAM_ARGS="--mode <build_mode>" pio run -e <platformio_env> -t <target>

  Replace <platformio_env> with the environment name from platformio.ini (e.g., c3, esp32dev).
  Replace <target> with the desired action (e.g., upload, build, monitor, clean).
```