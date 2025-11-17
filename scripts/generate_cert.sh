#!/bin/bash
set -e

CERT_DIR="certs"
ROOT_CA_KEY="$CERT_DIR/rootCA.key"
ROOT_CA_CERT="$CERT_DIR/rootCA.pem"
LOCAL_KEY="$CERT_DIR/localhost.key"
LOCAL_CERT="$CERT_DIR/localhost.crt"
DAYS_VALID=365
KEY_BITS=2048

mkdir -p "$CERT_DIR"

UNAME=$(uname)
IS_WINDOWS=false
if [[ "$UNAME" == MINGW* || "$UNAME" == MSYS* || "$UNAME" == CYGWIN* ]]; then
    IS_WINDOWS=true
fi

# Create portable temp config
TEMP_CNF="$CERT_DIR/tmp_openssl.cnf"
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

# Convert to Windows format for OpenSSL
if [[ "$IS_WINDOWS" == true ]]; then
    unix2dos "$TEMP_CNF" 2>/dev/null || true
fi

echo "=== Generating Root CA ==="
if [[ ! -f "$ROOT_CA_KEY" ]]; then
    openssl genrsa -out "$ROOT_CA_KEY" $KEY_BITS
    openssl req -x509 -new -nodes -key "$ROOT_CA_KEY" -sha256 -days 1825 \
        -out "$ROOT_CA_CERT" -subj "/CN=Local Dev Root CA"
fi

echo "=== Generating localhost key and CSR ==="
openssl genrsa -out "$LOCAL_KEY" $KEY_BITS
openssl req -new -key "$LOCAL_KEY" -out "$CERT_DIR/localhost.csr" -config "$TEMP_CNF"

echo "=== Signing localhost certificate ==="
openssl x509 -req -in "$CERT_DIR/localhost.csr" \
    -CA "$ROOT_CA_CERT" -CAkey "$ROOT_CA_KEY" -CAcreateserial \
    -out "$LOCAL_CERT" -days $DAYS_VALID -sha256 \
    -extensions v3_req -extfile "$TEMP_CNF"

rm -f "$CERT_DIR/localhost.csr" "$CERT_DIR/rootCA.srl" "$TEMP_CNF"

echo "=== Done ==="