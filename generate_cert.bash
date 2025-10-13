#!/bin/bash
set -e
mkdir -p certs
cd certs

echo "Generating Root CA..."
openssl req -x509 -newkey rsa:4096 -sha256 -days 3650 -nodes \
  -keyout ca.key -out ca.crt -subj "/CN=MyTestRootCA"

echo "Generating Server key and CSR..."
openssl req -newkey rsa:2048 -nodes -keyout server.key -out server.csr \
  -subj "/CN=localhost"

echo "Creating SAN config..."
cat > san.cnf <<EOF
subjectAltName = DNS:localhost,IP:127.0.0.1
EOF

echo "Signing Server certificate with Root CA..."
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 365 -sha256 -extfile san.cnf

echo "Certificates generated:"
ls -1