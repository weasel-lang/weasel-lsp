#!/usr/bin/env sh
# Install weaselc from the latest GitHub Release.
# Usage: curl -fsSL https://raw.githubusercontent.com/weasel-lang/language-services-weasel/main/install.sh | sh
# Override destination: INSTALL_DIR=/usr/local/bin sh install.sh

set -e

REPO="weasel-lang/language-services-weasel"
BINARY="weaselc"

# Detect OS
case "$(uname -s)" in
    Darwin) OS="macos" ;;
    Linux)  OS="linux" ;;
    *) echo "Unsupported OS: $(uname -s)" >&2; exit 1 ;;
esac

# Detect architecture
case "$(uname -m)" in
    x86_64)          ARCH="x86_64" ;;
    arm64 | aarch64) ARCH="arm64"  ;;
    *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;;
esac

TARGET="${OS}-${ARCH}"

# Resolve install directory
if [ -z "$INSTALL_DIR" ]; then
    if [ -w "/usr/local/bin" ]; then
        INSTALL_DIR="/usr/local/bin"
    else
        INSTALL_DIR="$HOME/.local/bin"
        mkdir -p "$INSTALL_DIR"
    fi
fi

# Fetch latest release version
echo "Fetching latest release..."
VERSION=$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
    | grep '"tag_name"' | sed 's/.*"tag_name": *"\(.*\)".*/\1/')

if [ -z "$VERSION" ]; then
    echo "Could not determine latest release version." >&2
    exit 1
fi

echo "Installing ${BINARY} ${VERSION} for ${TARGET}..."

TARBALL="${BINARY}-${VERSION}-${TARGET}.tar.gz"
URL="https://github.com/${REPO}/releases/download/${VERSION}/${TARBALL}"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

curl -fsSL "$URL" -o "$TMP/$TARBALL"
tar xzf "$TMP/$TARBALL" -C "$TMP"
install -m 755 "$TMP/$BINARY" "$INSTALL_DIR/$BINARY"

echo "Installed: ${INSTALL_DIR}/${BINARY}"

# Warn if INSTALL_DIR is not on PATH
case ":$PATH:" in
    *":$INSTALL_DIR:"*) ;;
    *) echo "Note: add '${INSTALL_DIR}' to your PATH" ;;
esac
