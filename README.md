# GeoDMS
Geographic Data & Model Software (GeoDMS) is a platform for developing Geographic Planning Support Systems.
Where fast generic, land-use modelling and routing algorithms are implemented in C++ and exposed through the GeoDMS scripting language.

# Downloading and getting started
Recent GeoDMS versions can be downloaded from this repository's [release page](https://github.com/ObjectVision/GeoDMS/releases). 

If you are new to GeoDMS, the [GeoDMS Academy](https://github.com/ObjectVision/GeoDMS_Academy) will guide you through some of the language's basic features. 
For the complete documentation of GeoDMS, including additional examples, see [our wiki](https://github.com/ObjectVision/GeoDMS/wiki) or [geodms website](https://geodms.nl)

## Verifying a Linux download

Each Linux release includes a SHA-256 checksum and a CMS/PKCS#7 signature
signed with the **Object Vision B.V.** GlobalSign EV Code Signing certificate.
Download all four files from the release page, then run:

```bash
# 1. Verify the signature (proves the checksum came from Object Vision B.V.)
openssl cms -verify -binary -purpose any \
  -in      GeoDms<ver>-linux-x64.tar.gz.sha256.p7s -inform DER \
  -content GeoDms<ver>-linux-x64.tar.gz.sha256 \
  -CAfile  GlobalSign-CodeSigning-Root-R45.pem

# 2. Verify the tarball matches the checksum
sha256sum -c GeoDms<ver>-linux-x64.tar.gz.sha256
```

`GlobalSign-CodeSigning-Root-R45.pem` is the signing root CA, distributed
alongside the release because it is a dedicated code-signing root not present
in all Linux CA bundles.  You can independently fetch it from GlobalSign:
`http://secure.globalsign.com/cacert/codesigningrootr45.crt`

A `VERIFY.md` file with the same instructions is also included inside the
tarball itself.

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
