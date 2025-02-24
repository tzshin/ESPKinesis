from typing import TYPE_CHECKING

if TYPE_CHECKING:
    # Dummy definitions for linting/type-checkers only.
    def Import(name: str) -> None:
        ...
    class DummyEnv:
        ...
    env: DummyEnv

# At runtime, PlatformIO will inject the actual "Import" and "env".
Import("env")

import os
import sys
import argparse

def parse_arguments():
    usage_msg = (
        "\n"
        "  Windows (PowerShell):\n"
        "    $env:PROGRAM_ARGS=\"--mode <build_mode>\"; pio run -e <platformio_env> -t <target>\n"
        "  Windows (CMD):\n"
        "    set \"PROGRAM_ARGS=--mode <build_mode>\" && pio run -e <platformio_env> -t <target>\n"
        "  Unix (Linux/macOS):\n"
        "    PROGRAM_ARGS=\"--mode <build_mode>\" pio run -e <platformio_env> -t <target>\n"
        "  Replace <platformio_env> with the environment name from platformio.ini (e.g., c3, esp32dev).\n"
        "  Replace <target> with the desired action (e.g., upload, build, monitor, clean).\n"
    )
    epilog_msg = (
        "Build mode options:\n"
        "  transmitter : Build for transmitter mode\n"
        "  receiver    : Build for receiver mode\n"
        "  show_mac    : Build to display MAC address\n"
        "  dev         : Build in development mode"
    )
    
    parser = argparse.ArgumentParser(
        description="Select the build mode",
        usage=usage_msg,
        epilog=epilog_msg,
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument(
        "--mode", "-m",
        type=str,
        required=True,
        help="Select build mode: transmitter, receiver, show_mac, dev, etc."
    )
    
    # Use the PROGRAM_ARGS environment variable exclusively.
    custom_args = os.environ.get("PROGRAM_ARGS", "").split()
    
    if not custom_args:
        parser.print_help()
        sys.exit(1)
    
    return parser.parse_args(custom_args)

args = parse_arguments()
mode = args.mode.lower()
print(f"Selected build mode: {mode}")

mode_config = {
    "transmitter": {"folder": "transmitter", "macro": "TRANSMITTER"},
    "receiver":    {"folder": "receiver",    "macro": "RECEIVER"},
    "show_mac":    {"folder": "show_mac",    "macro": "SHOW_MAC"},
    "dev":         {"folder": "dev",         "macro": "DEV"}
}

if mode not in mode_config:
    sys.exit(f"Error: Unknown build mode '{mode}'. Allowed modes: {', '.join(mode_config.keys())}")

config = mode_config[mode]
env.Replace(SRC_FILTER=["-<*>", f"+<{config['folder']}>"])
env.Append(CPPDEFINES=[config["macro"]])
