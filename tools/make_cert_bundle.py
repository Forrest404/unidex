# Builds certs/x509_crt_bundle.bin from a PEM list of root CAs (Mozilla's, https://curl.se/ca/cacert.pem), in the
# layout Arduino-ESP32 2.0.x reads (libraries/WiFiClientSecure/src/esp_crt_bundle.c): a 2-byte big-endian count,
# then per certificate a 2-byte big-endian name length and key length, the DER subject and the DER public key,
# sorted by subject for its binary search. (ESP-IDF 5's gen_crt_bundle.py writes a newer layout that this
# Arduino version misreads and crashes on.) Needs the `cryptography` package.
#   python3 tools/make_cert_bundle.py cacert.pem
import re, struct, sys
from pathlib import Path
from cryptography import x509
from cryptography.hazmat.primitives import serialization

pem = Path(sys.argv[1]).read_text()
certs = [x509.load_pem_x509_certificate(m.encode())
         for m in re.findall(r'-----BEGIN CERTIFICATE-----.+?-----END CERTIFICATE-----', pem, re.S)]
entries = sorted((c.subject.public_bytes(),
                  c.public_key().public_bytes(serialization.Encoding.DER,
                                              serialization.PublicFormat.SubjectPublicKeyInfo)) for c in certs)
out = struct.pack('>H', len(entries))
for name, key in entries:
    out += struct.pack('>HH', len(name), len(key)) + name + key
dest = Path(__file__).resolve().parent.parent / 'certs/x509_crt_bundle.bin'
dest.write_bytes(out)
print(f'{len(entries)} certificates, {len(out)} bytes -> {dest}')
