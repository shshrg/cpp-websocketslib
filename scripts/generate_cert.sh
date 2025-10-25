#!/bin/bash

CERT_DIR="certs"
CERT_FILE="$CERT_DIR/cert.pem"
KEY_FILE="$CERT_DIR/key.pem"
DHPARAM_FILE="$CERT_DIR/dhparam.pem"
DAYS_VALID=365
KEY_BITS=2048

mkdir -p "$CERT_DIR"
chmod 700 "$CERT_DIR"

# ===== Check if both exist =====
if [[ -f "$CERT_FILE" && -f "$KEY_FILE" ]]; then
    echo "Certificate and key already exist. Skipping generation."

elif [[ -f "$CERT_FILE" && ! -f "$KEY_FILE" ]]; then
    echo "Error: Certificate exists but private key is missing!"
    exit 1

elif [[ ! -f "$CERT_FILE" && -f "$KEY_FILE" ]]; then
    echo "Generating self-signed certificate using existing private key..."
    openssl req -new -x509 -key "$KEY_FILE" -out "$CERT_FILE" -days $DAYS_VALID \
        -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=localhost" || {
        echo "Error: Failed to generate certificate!" >&2
        exit 1
    }

else
    echo "Generating new private key and self-signed certificate..."
    openssl req -newkey rsa:$KEY_BITS -nodes -keyout "$KEY_FILE" \
        -x509 -days $DAYS_VALID -out "$CERT_FILE" \
        -subj "/C=US/ST=State/L=City/O=Org/OU=Unit/CN=localhost" || {
        echo "Error: Failed to generate certificate and key!" >&2
        exit 1
    }
fi

chmod 600 "$KEY_FILE"

# ===== Generate DH parameters =====
if [[ ! -f "$DHPARAM_FILE" ]]; then
    echo "Generating Diffie-Hellman parameters (may take a while)..."
    openssl dhparam -out "$DHPARAM_FILE" $KEY_BITS && \
    echo "DH parameters generated successfully."
else
    echo "DH parameters already exist. Skipping generation."
fi
