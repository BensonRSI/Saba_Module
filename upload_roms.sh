#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROMS_DIR="${ROMS_DIR:-$ROOT_DIR/roms}"
NAMES_FILE="${NAMES_FILE:-$ROMS_DIR/rom_names.txt}"
DEVICE="${DEVICE:-/dev/ttyACM0}"
XMODEM_BIN="${XMODEM_BIN:-$ROOT_DIR/tools/xmodem_upload/build/xmodem_upload}"
DRY_RUN=0
ACTION="--setup"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --setup|-s)
      ACTION="--setup"
      ;;
    --upload|-u)
      ACTION="--upload"
      ;;
    --dry-run|-n)
      DRY_RUN=1
      ;;
    --help|-h)
      ACTION="--help"
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --setup to create the names file or --upload to start the transfer."
      exit 1
      ;;
  esac
  shift
done

if [[ ! -x "$XMODEM_BIN" ]]; then
  echo "xmodem_upload not found at: $XMODEM_BIN"
  echo "Build it with:"
  echo "  cmake -S $ROOT_DIR/tools/xmodem_upload -B $ROOT_DIR/tools/xmodem_upload/build"
  echo "  cmake --build $ROOT_DIR/tools/xmodem_upload/build"
  exit 1
fi

trim() {
  local s="$1"
  s="${s#"${s%%[![:space:]]*}"}"
  s="${s%"${s##*[![:space:]]}"}"
  printf '%s' "$s"
}

sanitize_cartname() {
  local value
  value="$(trim "$1")"
  value="${value//[$'\r\n\t\f\v']/}"
  value="${value//[^[:print:]]/}"
  value="$(printf '%s' "$value" | cut -c1-21)"
  if [[ -z "$value" ]]; then
    printf 'ROM'
  else
    printf '%s' "$value"
  fi
}

max_kb_for_slot() {
  local slot_num="$1"
  if ((slot_num >= 0 && slot_num <= 3)); then
    printf '64'
  elif ((slot_num >= 4 && slot_num <= 12)); then
    printf '8'
  else
    printf '4'
  fi
}

file_size_kb() {
  local path="$1"
  local bytes
  bytes=$(stat -c%s "$path")
  printf '%d' $(((bytes + 1023) / 1024))
}

write_template() {
  mkdir -p "$ROMS_DIR"

  if [[ -f "$NAMES_FILE" ]]; then
    echo "The file $NAMES_FILE already exists."
    read -r -p "Overwrite it and create a backup? [y/N]: " answer
    case "$answer" in
      [Yy]|[Yy][Ee][Ss])
        backup="$NAMES_FILE.bak.$(date +%Y%m%d-%H%M%S)"
        cp "$NAMES_FILE" "$backup"
        echo "Backup created: $backup"
        ;;
      *)
        echo "Aborted. Existing file was left unchanged."
        exit 0
        ;;
    esac
  fi

  shopt -s nullglob
  files=("$ROMS_DIR"/*.BIN "$ROMS_DIR"/*.bin)
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No .BIN files found in $ROMS_DIR"
    exit 1
  fi

  {
    echo "# Edit each line as: <SLOT> <SIZE_KB> <FILENAME>=<CARTNAME>"
    echo "# Example: 01 64 01.BIN=ASTEROIDS"
    echo "# Max length: 21 chars"
    slot_num=0
    for path in "${files[@]}"; do
      [[ -f "$path" ]] || continue
      slot_num=$((slot_num + 1))
      file="$(basename "$path")"
      size_kb="$(file_size_kb "$path")"
      printf '%02d %d %s=\n' "$slot_num" "$size_kb" "$file"
    done
  } > "$NAMES_FILE"

  chmod 644 "$NAMES_FILE"
  echo "Created names file: $NAMES_FILE"
  echo "Edit the names, then run:"
  echo "  $0 --upload"
}

run_upload() {
  if [[ ! -f "$NAMES_FILE" ]]; then
    echo "Names file not found: $NAMES_FILE"
    echo "Create it with:"
    echo "  $0 --setup"
    exit 1
  fi

  while IFS= read -r line; do
    line="$(trim "$line")"
    [[ -z "$line" || "$line" == \#* ]] && continue

    if [[ ! "$line" =~ ^([0-9]{2})[[:space:]]+([0-9]+)[[:space:]]+([^=]+)=(.*)$ ]]; then
      echo "Invalid line in $NAMES_FILE: $line"
      echo "Expected format: <SLOT> <SIZE_KB> <FILENAME>=<CARTNAME>"
      exit 1
    fi

    slot="${BASH_REMATCH[1]}"
    declared_size_kb="${BASH_REMATCH[2]}"
    file="$(trim "${BASH_REMATCH[3]}")"
    cart="$(sanitize_cartname "${BASH_REMATCH[4]}")"
    full_path="$ROMS_DIR/$file"

    if [[ ! -f "$full_path" ]]; then
      echo "WARNING: ROM file not found for line: $line"
      echo "WARNING: Expected file: $full_path"
      continue
    fi

    slot_num=$((10#$slot))
    actual_size_kb="$(file_size_kb "$full_path")"
    max_kb="$(max_kb_for_slot "$slot_num")"

    if ((actual_size_kb > max_kb)); then
      echo "WARNING: slot $slot allows max ${max_kb}kB, but $file has ${actual_size_kb}kB"
    fi

    if ((declared_size_kb != actual_size_kb)); then
      echo "WARNING: declared size ${declared_size_kb}kB differs from actual ${actual_size_kb}kB for $file"
    fi

    echo "Slot $slot -> $full_path -> SIZE=${actual_size_kb}kB -> CARTNAME=$cart"
    if [[ "$DRY_RUN" -eq 1 ]]; then
      printf '%s\n' "$XMODEM_BIN -f \"$full_path\" -d \"$DEVICE\" -p \"W${slot}\\n${cart}\\n\" -b 115200"
    else
      "$XMODEM_BIN" -f "$full_path" -d "$DEVICE" -p "W${slot}\n${cart}\n" -b 115200
      sleep 8
    fi
  done < "$NAMES_FILE"
}

case "$ACTION" in
  --setup)
    write_template
    ;;
  --upload)
    run_upload
    ;;
  --help|"")
    echo "Usage:"
    echo "  $0 --setup      Create the names file for editing"
    echo "  $0 --upload     Upload all ROMs using the names file"
    echo "  $0 --dry-run    Preview the upload commands without sending data"
    echo ""
    echo "File format: <SLOT> <SIZE_KB> <FILENAME>=<CARTNAME>"
    echo "Names file: $NAMES_FILE"
    ;;
  *)
    echo "Unknown option: $ACTION"
    echo "Use --setup to create the names file or --upload to start the transfer."
    exit 1
    ;;
esac
