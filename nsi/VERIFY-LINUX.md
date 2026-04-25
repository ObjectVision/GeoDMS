# Verifying a GeoDMS Linux release

Each GeoDMS Linux tarball is accompanied by three extra files:

| File | Purpose |
|------|---------|
| `GeoDms<ver>-linux-x64.tar.gz.sha256` | SHA-256 checksum |
| `GeoDms<ver>-linux-x64.tar.gz.sha256.p7s` | CMS/PKCS#7 detached signature over the checksum file |
| `GlobalSign-CodeSigning-Root-R45.pem` | GlobalSign Code Signing Root CA (needed once) |

The signature is created with the **Object Vision B.V.** GlobalSign EV Code Signing
certificate (issued to `O=Object Vision B.V., C=NL`).  The private key never
leaves a hardware security token; signing is performed via the Windows CNG/CSP
layer on the build machine.

## Step 1 — verify the signature

```bash
openssl cms -verify -binary -purpose any \
  -in      GeoDms<ver>-linux-x64.tar.gz.sha256.p7s -inform DER \
  -content GeoDms<ver>-linux-x64.tar.gz.sha256 \
  -CAfile  GlobalSign-CodeSigning-Root-R45.pem
```

Expected output: `CMS Verification successful`

This confirms that the checksum file was produced by Object Vision B.V. and has
not been altered since signing.

> **Why a separate CA file?**  GlobalSign's *Code Signing* Root R45 is a
> dedicated root for code-signing certificates.  It is not included in all
> Linux distributions' default TLS CA bundles, so we ship it alongside the
> release.  You can independently fetch it from GlobalSign:
> `http://secure.globalsign.com/cacert/codesigningrootr45.crt`

## Step 2 — verify the tarball integrity

```bash
sha256sum -c GeoDms<ver>-linux-x64.tar.gz.sha256
```

Expected output: `GeoDms<ver>-linux-x64.tar.gz: OK`

## What the chain looks like

```
GlobalSign Code Signing Root R45          (self-signed, ships as GlobalSign-CodeSigning-Root-R45.pem)
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
