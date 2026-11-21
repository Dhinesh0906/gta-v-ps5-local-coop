#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${PS5_SDK:?Set PS5_SDK to your built ps5-payload-sdk install directory}"
"$PS5_SDK/bin/prospero-clang" -O2 -Wall -Wextra -Werror \
 -DGTA_CONSOLE_PS5 -DGTA_COOP_AUTOLOAD -DGTA_COOP_OWNED_HOVER \
 -DGTA_COOP_PLANE_CONTROLS -DGTA_COOP_AIRCRAFT_EXTRAS -DGTA_COOP_PLANE_PAYLOADS \
 -DGTA_COOP_AUTO_EQUIP_PAYLOADS -DGTA_COOP_PRIMARY_PAYLOAD_EQUIPMENT \
 -DGTA_COOP_LATE_PLANE -DGTA_COOP_OPPRESSOR2 -DGTA_COOP_SPLIT \
 -DGTA_COOP_PLAYER_ASSISTS -DGTA_COOP_ROAD \
 -I generated -I . trainers/console-autoload.c -o gta5-coop-autoload-rebuilt.elf
