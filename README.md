# GeoDMS
Geographic Data & Model Software (GeoDMS) is a platform for developing Geographic Planning Support Systems.
Where fast generic, land-use modelling and routing algorithms are implemented in C++ and exposed through the GeoDMS scripting language.

# Downloading and getting started
Recent GeoDMS versions can be downloaded from this repository's [release page](https://github.com/ObjectVision/GeoDMS/releases). 

If you are new to GeoDMS, the [GeoDMS Academy](https://github.com/ObjectVision/GeoDMS_Academy) will guide you through some of the language's basic features. 
For the complete documentation of GeoDMS, including additional examples, see [our wiki](https://github.com/ObjectVision/GeoDMS/wiki) or [geodms website](https://geodms.nl)

## Verifying downloads

All GeoDMS installers and packages are signed with the **Object Vision B.V.**
GlobalSign EV Code Signing certificate.  Only run or install a package after
confirming it carries a valid signature from Object Vision B.V.

### Windows

The `.exe` installer is Authenticode-signed.  Windows will show a publisher
dialog during installation; confirm the publisher reads **Object Vision B.V.**
before proceeding.

To verify before running:

```powershell
# PowerShell — should print "Valid" and show O=Object Vision B.V.
Get-AuthenticodeSignature "GeoDms<ver>-Setup-x64-cmake.exe" | Format-List
```

Or right-click the file → Properties → Digital Signatures → select the
signature → Details → confirm the signer is **Object Vision B.V.**

### Linux

Each Linux release includes a SHA-256 checksum file and a CMS/PKCS#7 detached
signature (`.p7s`) signed with the Object Vision B.V. GlobalSign EV Code Signing
certificate.  Download both alongside the tarball and verify before extracting:

```bash
openssl cms -verify -binary -purpose any \
  -in      GeoDms<ver>-linux-x64.tar.gz.sha256.p7s -inform DER \
  -content GeoDms<ver>-linux-x64.tar.gz.sha256 \
  -CAfile  <(curl -fsSL http://secure.globalsign.com/cacert/codesigningrootr45.crt \
             | openssl x509 -inform DER)

sha256sum -c GeoDms<ver>-linux-x64.tar.gz.sha256
```

For a full explanation of the certificate chain, how to obtain the signing root
from alternative sources, and how to inspect the signer identity, see
[`nsi/VERIFY-LINUX.md`](nsi/VERIFY-LINUX.md).

# Compilation
Build instructions for GeoDMS can be found at our [wiki compilation page](https://github.com/ObjectVision/GeoDMS/wiki/Compiling-the-GeoDMS) or our [website](https://geodms.nl/docs/compiling-the-geodms.html).
For a technical overview of the GeoDMS software, see readme-dms.MD in the GeoDMS source code root folder, or [here](https://github.com/ObjectVision/GeoDMS/blob/main/readme-dms.MD)

# Terms of use
This open source software, you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 3 
(the License) as published by the Free Software Foundation.

For redistribution consult the GNU GPL3 license at [LICENSE.md](LICENSE.md)
or [https://www.gnu.org/licenses/gpl-3.0.en.html](https://www.gnu.org/licenses/gpl-3.0.en.html)

# Contact
Feel free to contact us for support on land use modelling, transport modelling or geographic scientific computing.

Object Vision b.v.
Email:    [info(at)objectvision.nl](mailto:info@objectvision.nl)
Phone:    +31-20-598.9083 
Location: 1081 HV  AMSTERDAM - the Netherlands - De Boelelaan 1085 room R223
Website:  [https://objectvision.nl](https://objectvision.nl)
