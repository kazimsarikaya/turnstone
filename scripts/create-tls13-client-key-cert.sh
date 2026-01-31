#!/usr/bin/env bash

# This work is licensed under TURNSTONE OS Public License.
# Please read and understand latest version of Licence.

openssl genpkey -algorithm ed25519 -out build/client.key
openssl req -new -key build/client.key -out build/client.csr -subj "/CN=Turnstone-Client-User"
openssl pkey -in build/client.key -pubout -outform DER | tail -c 32 > build/pub.raw
MY_SKID=`openssl dgst -sha256 -hex build/pub.raw | awk '{print $2}'`

cat > build/client_ext.conf << EOF
# client_ext.cnf
basicConstraints = CA:FALSE
nsCertType = client
keyUsage = critical, digitalSignature
extendedKeyUsage = clientAuth
subjectKeyIdentifier = ${MY_SKID}
authorityKeyIdentifier = keyid, issuer
EOF

openssl x509 -req -in build/client.csr -CA build/ca.pem -CAkey build/ca.key -CAcreateserial -out build/client.pem -days 365 -extfile build/client_ext.conf
