# Verifying a GeoDMS Linux release

Each GeoDMS Linux tarball is accompanied by two extra files:

| File | Purpose |
|------|---------|
| `GeoDms<ver>-linux-x64.tar.gz.sha256` | SHA-256 checksum |
| `GeoDms<ver>-linux-x64.tar.gz.sha256.p7s` | CMS/PKCS#7 detached signature over the checksum |

The signature is created with the **Object Vision B.V.** GlobalSign EV Code Signing
certificate (issued to `O=Object Vision B.V., C=NL`).  The private key never
leaves a hardware security token; signing is performed via the Windows CNG/CSP
layer on the build machine.

## Step 0 — obtain the root CA independently

Fetch the GlobalSign Code Signing Root R45 **directly from GlobalSign** — do not
use a copy distributed alongside the release.  If you downloaded the CA file from
the same source as the tarball, an attacker who compromised that source could
substitute both the signature and the CA file, defeating verification entirely.

```bash
curl -fsSL http://secure.globalsign.com/cacert/codesigningrootr45.crt \
  | openssl x509 -inform DER -out GlobalSign-CodeSigning-Root-R45.pem

# Confirm the fingerprint matches the value published in the GeoDMS README
openssl x509 -in GlobalSign-CodeSigning-Root-R45.pem -fingerprint -sha256 -noout
```

Expected fingerprint (published independently in the GeoDMS source repository):
```
7B:9D:55:3E:1C:92:CB:6E:88:03:E1:37:F4:F2:87:D4:36:37:57:F5:D4:4B:37:D5:2F:9F:CA:22:FB:97:DF:86
```

## Step 1 — verify the signature

```bash
openssl cms -verify -binary -purpose any \
  -in      GeoDms<ver>-linux-x64.tar.gz.sha256.p7s -inform DER \
  -content GeoDms<ver>-linux-x64.tar.gz.sha256 \
  -CAfile  GlobalSign-CodeSigning-Root-R45.pem
```

Expected output: `CMS Verification successful`

This confirms that the checksum was produced by Object Vision B.V. and has
not been altered since signing.

## Step 2 — verify the tarball integrity

```bash
sha256sum -c GeoDms<ver>-linux-x64.tar.gz.sha256
```

Expected output: `GeoDms<ver>-linux-x64.tar.gz: OK`

## What the chain looks like

```
GlobalSign Code Signing Root R45             (fetched from GlobalSign, fingerprint above)
  └─ GlobalSign GCC R45 EV CodeSigning CA 2020   (intermediate, embedded in .p7s)
       └─ Object Vision B.V.                      (signer, embedded in .p7s)
```

## Inspecting the signer identity

```bash
openssl pkcs7 -inform DER \
  -in GeoDms<ver>-linux-x64.tar.gz.sha256.p7s \
  -print_certs -noout
```

Look for:
```
subject=... O=Object Vision B.V. ... C=NL ...
issuer=... CN=GlobalSign GCC R45 EV CodeSigning CA 2020 ...
```
