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
  value="${value//[$'\r\n\t\f\v'] /}"
  value="${value//[^[:print:]]/}"
  value="$(printf '%s' "$value" | cut -c1-21)"
  if [[ -z "$value" ]]; then
    printf 'ROM'
  else
    printf '%s' "$value"
  fi
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

  mapfile -t sorted < <(printf '%s\n' "${files[@]}" | xargs -n 1 basename | sort)

  {
    echo "# Edit each line as: <FILENAME>=<CARTNAME>"
    echo "# Example: 01.BIN=ASTEROIDS"
    echo "# Max length: 21 chars"
    for file in "${sorted[@]}"; do
      printf '%s=\n' "$file"
    done
  } > "$NAMES_FILE"

  chmod 644 "$NAMES_FILE"
  echo "Created names file: $NAMES_FILE"
  echo "Edit the names, then run:"
  echo "  $0 --upload"
}

load_names() {
  if [[ ! -f "$NAMES_FILE" ]]; then
    echo "Names file not found: $NAMES_FILE"
    echo "Create it with:"
    echo "  $0 --setup"
    exit 1
  fi

  declare -Ag cart_names=()
  while IFS='=' read -r file value; do
    file="$(trim "$file")"
    value="$(trim "$value")"

    [[ -z "$file" || "$file" == \#* ]] && continue

    cart_names["$file"]="$(sanitize_cartname "$value")"
  done < "$NAMES_FILE"

  printf '%s\n' "${!cart_names[@]}" | sort >/dev/null 2>&1 || true
}

run_upload() {
  shopt -s nullglob
  files=("$ROMS_DIR"/*.BIN "$ROMS_DIR"/*.bin)
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "No .BIN files found in $ROMS_DIR"
    exit 1
  fi

  declare -A cart_names=()
  while IFS='=' read -r file value; do
    file="$(trim "$file")"
    value="$(trim "$value")"
    [[ -z "$file" || "$file" == \#* ]] && continue
    cart_names["$file"]="$(sanitize_cartname "$value")"
  done < "$NAMES_FILE"

  mapfile -t sorted < <(printf '%s\n' "${files[@]}" | xargs -n 1 basename | sort)

  count=0
  for file in "${sorted[@]}"; do
    full_path="$ROMS_DIR/$file"
    if [[ ! -f "$full_path" ]]; then
      continue
    fi

    count=$((count + 1))
    slot=$(printf '%02d' "$count")
    cart="${cart_names[$file]:-}"

    if [[ -z "$cart" ]]; then
      echo "Missing entry for $file in $NAMES_FILE"
      exit 1
    fi

    echo "Slot $slot -> $full_path -> CARTNAME=$cart"
    if [[ "$DRY_RUN" -eq 1 ]]; then
      printf '%s\n' "$XMODEM_BIN -f \"$full_path\" -d \"$DEVICE\" -p \"w${slot}\\n${cart}\\n\" -b 115200"
    else
      "$XMODEM_BIN" -f "$full_path" -d "$DEVICE" -p "w${slot}\n${cart}\n" -b 115200
    fi
    sleep 5
  done
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
    echo "Names file: $NAMES_FILE"
    ;;
  *)
    echo "Unknown option: $ACTION"
    echo "Use --setup to create the names file or --upload to start the transfer."
    exit 1
    ;;
esac
