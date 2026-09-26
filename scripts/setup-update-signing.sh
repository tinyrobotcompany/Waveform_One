#!/bin/sh
# Publisher workstation only. Never copy the private key onto a customer device.
set -eu
cd "$(dirname "$0")/.."
key_dir="$HOME/.config/waveform-one-publisher"
private="$key_dir/update-signing.pem"
public="pi/updater/release-public.pem"
umask 077
mkdir -p "$key_dir"
if [ ! -f "$private" ]; then
  if [ -f "$public" ]; then
    echo 'Public key already exists but private key is missing. Restore its backup; refusing to rotate trust.' >&2
    exit 1
  fi
  openssl genpkey -algorithm RSA -pkeyopt rsa_keygen_bits:3072 -out "$private"
fi
temporary=$(mktemp)
trap 'rm -f "$temporary"' EXIT
openssl pkey -in "$private" -pubout -out "$temporary"
if [ -f "$public" ]; then
  cmp "$public" "$temporary"
else
  cp "$temporary" "$public"
  chmod 644 "$public"
fi
gh secret set UPDATE_SIGNING_KEY --repo tinyrobotcompany/Waveform_One < "$private"
printf 'Signing key configured. Keep an offline backup of %s. Only the public key is in the repository.\n' "$private"
