#!/bin/bash

CERT_DIR="certs"
CERT_FILE="$CERT_DIR/cert.pem"
KEY_FILE="$CERT_DIR/key.pem"
DHPARAM_FILE="$CERT_DIR/dhparam.pem"
DAYS_VALID=365
KEY_BITS=2048

mkdir -p "$CERT_DIR"

# Detect Windows (Git Bash, MSYS, MINGW)
UNAME=$(uname)
IS_WINDOWS=false
if [[ "$UNAME" == MINGW* || "$UNAME" == MSYS* || "$UNAME" == CYGWIN* ]]; then
    IS_WINDOWS=true
fi

# Only chmod on non-Windows
if [[ "$IS_WINDOWS" == false ]]; then
    chmod 700 "$CERT_DIR"
fi

# ===== Prepare temporary OpenSSL config for SAN =====
TEMP_CNF=$(mktemp)
cat > "$TEMP_CNF" <<EOF
[req]
distinguished_name = req_distinguished_name
req_extensions = v3_req
prompt = no

[req_distinguished_name]
CN = localhost

[v3_req]
subjectAltName = @alt_names
basicConstraints = CA:FALSE

[alt_names]
DNS.1 = localhost
IP.1 = 127.0.0.1
EOF

# ===== Check if certificate and key exist =====
if [[ -f "$CERT_FILE" && -f "$KEY_FILE" ]]; then
    echo "Certificate and key already exist. Skipping generation."

elif [[ -f "$CERT_FILE" && ! -f "$KEY_FILE" ]]; then
    echo "Error: Certificate exists but private key is missing!"
    rm -f "$TEMP_CNF"
    exit 1

elif [[ ! -f "$CERT_FILE" && -f "$KEY_FILE" ]]; then
    echo "Generating self-signed certificate using existing private key..."
    openssl req -new -x509 -key "$KEY_FILE" -out "$CERT_FILE" -days $DAYS_VALID \
        -extensions v3_req \
        -config "$TEMP_CNF" || {
        echo "Error: Failed to generate certificate!" >&2
        rm -f "$TEMP_CNF"
        exit 1
    }

else
    echo "Generating new private key and self-signed certificate with SAN..."
    openssl req -x509 -newkey rsa:$KEY_BITS -nodes -keyout "$KEY_FILE" \
        -out "$CERT_FILE" -days $DAYS_VALID \
        -extensions v3_req \
        -config "$TEMP_CNF" || {
        echo "Error: Failed to generate certificate and key!" >&2
        rm -f "$TEMP_CNF"
        exit 1
    }
fi

# ===== Set file permissions on non-Windows =====
if [[ "$IS_WINDOWS" == false ]]; then
    chmod 600 "$KEY_FILE"
fi

# ===== Generate DH parameters =====
if [[ ! -f "$DHPARAM_FILE" ]]; then
    echo "Generating Diffie-Hellman parameters (may take a while)..."
    if [[ "$IS_WINDOWS" == true ]]; then
        echo "Skipping DH param generation on Windows"
    else
        openssl dhparam -out "$DHPARAM_FILE" $KEY_BITS && \
        echo "DH parameters generated successfully."
    fi
else
    echo "DH parameters already exist. Skipping generation."
fi

# Cleanup temporary config
rm -f "$TEMP_CNF"

echo "Certificate generation complete."
