# TLS root certificates

`x509_crt_bundle.bin` is the Mozilla root CA list in ESP-IDF's bundle format, embedded in the firmware
(`board_build.embed_files` in `platformio.ini`) so HTTPS calls (OpenAI, Anthropic, GitHub) check the server's
certificate. To refresh it from a newer list (https://curl.se/docs/caextract.html, `cacert.pem`):

```sh
python3 tools/make_cert_bundle.py cacert.pem   # needs the cryptography package
```

It must be in the layout Arduino-ESP32 2.0.x reads (a big-endian count, then big-endian lengths per
certificate). ESP-IDF 5's `gen_crt_bundle.py` writes a newer layout that this Arduino version misreads, crashing on
the first HTTPS connection, so don't use that. The current file was made from Mozilla's list of 2024-09-24
(151 roots).
