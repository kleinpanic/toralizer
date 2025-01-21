#!/usr/bin/env bash
#
# toralize.sh - A script to preload a Tor SOCKS5 hooking .so library
#               and run commands through Tor transparently.
#
# Usage:
#   toralize.sh [--help] [--version] [--install] [--uninstall] command [args...]
#
# Examples:
#   ./toralize.sh curl http://check.torproject.org
#   ./toralize.sh wget https://example.com
#   sudo ./toralize.sh --install
#   sudo ./toralize.sh --uninstall

# ----------------
# CONFIG
# ----------------
VERSION="1.0.0"
# Name of the .so file we’ve built. Adjust if yours differs.
SO_FILENAME="toralizer.so"
# The directory for a "global" install
GLOBAL_LIB_DIR="/usr/local/share/toralizer"
# The target script name for a global install
GLOBAL_SCRIPT="/usr/local/bin/toralize"

# ----------------
# FUNCTIONS
# ----------------

print_help() {
  cat <<EOF
toralize.sh -- run commands through LD_PRELOAD with a Tor hooking library

Usage:
  toralize.sh [FLAGS] [COMMAND [ARGS...]]

Flags:
  --help       Show this help message
  --version    Show version information
  --install    Install toralize system-wide (requires root or sudo)
  --uninstall  Uninstall toralize from system (requires root or sudo)

When no special flags are given, toralize.sh sets LD_PRELOAD to the local
toralizer.so library, then executes the specified COMMAND. This forces all
connect() calls in that command to go through Tor (assuming your .so code
speaks SOCKS5 to Tor).

Examples:
  ./toralize.sh curl http://check.torproject.org
  ./toralize.sh wget https://example.com

EOF
}

print_version() {
  echo "toralize version $VERSION"
}

do_install() {
  # Check for root
  if [[ "$EUID" -ne 0 ]]; then
    echo "[!] Please run '--install' with sudo or as root."
    exit 1
  fi

  echo "[*] --install selected."

  # Prompt user: Global (G) or keep local (L)
  read -r -p "Install globally [G] or keep local [L]? (default: L) " choice
  choice="${choice^^}"  # uppercase

  if [[ "$choice" == "G" ]]; then
    # 1) Make sure the directory exists
    echo "[*] Installing toralizer globally..."

    mkdir -p "$GLOBAL_LIB_DIR"
    # Copy the .so file
    if [[ ! -f "$SO_FILENAME" ]]; then
      echo "[!] Cannot find $SO_FILENAME in current directory!"
      exit 1
    fi

    cp "$SO_FILENAME" "$GLOBAL_LIB_DIR"/

    # 2) Install a script as /usr/local/bin/toralize
    # We'll create a small wrapper that sets LD_PRELOAD to the global .so path
    cat <<EOF > "$GLOBAL_SCRIPT"
#!/usr/bin/env bash
# Global toralize script
export LD_PRELOAD="$GLOBAL_LIB_DIR/$SO_FILENAME"
exec "\$@"
EOF

    chmod +x "$GLOBAL_SCRIPT"
    echo "[+] Installed /usr/local/bin/toralize"
    echo "[*] You can now run:  toralize curl http://check.torproject.org"
    echo "[*] Done."
    exit 0

  else
    echo "[*] Keeping it local. No changes made."
    exit 0
  fi
}

do_uninstall() {
  # Check for root
  if [[ "$EUID" -ne 0 ]]; then
    echo "[!] Please run '--uninstall' with sudo or as root."
    exit 1
  fi

  echo "[*] --uninstall selected."
  # Remove the global script if it exists
  if [[ -f "$GLOBAL_SCRIPT" ]]; then
    echo "[*] Removing $GLOBAL_SCRIPT"
    rm -f "$GLOBAL_SCRIPT"
  else
    echo "[*] $GLOBAL_SCRIPT not found, skipping."
  fi

  # Remove the library directory if it exists
  if [[ -d "$GLOBAL_LIB_DIR" ]]; then
    echo "[*] Removing $GLOBAL_LIB_DIR"
    rm -rf "$GLOBAL_LIB_DIR"
  else
    echo "[*] $GLOBAL_LIB_DIR not found, skipping."
  fi

  echo "[*] Uninstall complete."
  exit 0
}

# ----------------
# MAIN
# ----------------

# If no arguments given, print help
if [[ $# -eq 0 ]]; then
  print_help
  exit 0
fi

# Check first argument for known flags
case "$1" in
  --help)
    print_help
    exit 0
    ;;
  --version)
    print_version
    exit 0
    ;;
  --install)
    do_install
    ;;
  --uninstall)
    do_uninstall
    ;;
esac

# If we reach here, no recognized "management" flag was used
# => We do the normal "run with LD_PRELOAD" flow.

# Build the path to our local .so
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SO_PATH="$SCRIPT_DIR/$SO_FILENAME"

if [[ ! -f "$SO_PATH" ]]; then
  echo "[!] $SO_PATH not found!"
  exit 1
fi

export LD_PRELOAD="$SO_PATH"

# Shift once, then run the user’s command
shift 0
"$@"

unset LD_PRELOAD

