#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

set -eu
set -o pipefail

# Support for both algorithms
ALGORITHM=${1:-ed25519}

SIGNSHA=256

# Ensure build directory exists
mkdir -p build

case $ALGORITHM in
    ed25519)
        echo "Generating Ed25519 key..."
        openssl genpkey -algorithm "$ALGORITHM" -out build/client.key
        SIGNSHA=256
        ;;
    secp256r1|p256|prime256v1)
        echo "Generating secp256r1 (P-256) key..."
        openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:prime256v1 -out build/client.key
        SIGNSHA=256
        ;;
    secp384r1|p384|prime384v1)
        echo "Generating secp384r1 (P-384) key..."
        openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:secp384r1 -out build/client.key
        SIGNSHA=384
        ;;
    *)
        echo "Error: Unsupported algorithm '$ALGORITHM'. Use 'ed25519' or 'secp256r1' or secp384r1'."
        exit 1
        ;;
esac

openssl pkey -in build/client.key -pubout -outform DER > build/pub.raw

# Generate CSR
openssl req -new -key build/client.key -out build/client.csr -subj "/CN=Turnstone-Client-User"

# Calculate Subject Key Identifier (SKID) from the raw public data
MY_SKID=$(openssl dgst -sha256 -hex build/pub.raw | awk '{print $2}')

# Prepare extension config
cat > build/client_ext.conf << EOF
basicConstraints = critical, CA:FALSE
nsCertType = client
keyUsage = critical, digitalSignature
extendedKeyUsage = clientAuth
subjectKeyIdentifier = ${MY_SKID}
authorityKeyIdentifier = keyid, issuer
EOF

# Sign the certificate
openssl x509 -req -in build/client.csr -CA build/ca.pem -CAkey build/ca.key -CAcreateserial \
    -out build/client.pem -days 30 -sha${SIGNSHA} -extfile build/client_ext.conf

echo "Successfully generated $ALGORITHM certificate in build/client.pem"
