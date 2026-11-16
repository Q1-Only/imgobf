# imgobf

A Windows command-line image obfuscator. It flattens an image's pixels along a
Gilbert space-filling curve, cyclically shifts the resulting 1D sequence by an
offset derived from a password, and writes the result back. De-obfuscation applies
the inverse shift.

Because the Gilbert curve preserves local pixel adjacency, obfuscated images stay
compressible and retain their color distribution — unlike naive random shuffling.

> **Not encryption.** The algorithm and hash function are public, and the password
> only controls the shift amount. Do not use this to protect sensitive data.

## Features

- Reversible pixel scrambling on top of a Gilbert space-filling curve
- Password-controlled shift (FNV-1a 64 hash → offset in `[1, N-1]`)
- Same input + same password ⇒ same output; different passwords ⇒ different output
- Reads and writes PNG / JPEG / BMP / TIFF (and any other WIC-supported format)
- Single small C++ file, no external dependencies beyond the Windows SDK

## Build

### MSVC

```bat
cl /std:c++17 /EHsc /O2 imgobf.cpp /link windowscodecs.lib ole32.lib oleaut32.lib uuid.lib
```

### MinGW-w64

```bat
g++ -std:c++17 -municode -O2 imgobf.cpp -o imgobf.exe -lwindowscodecs -lole32 -loleaut32 -luuid
```

`-municode` is required on MinGW because the entry point is `wmain`.

## Usage

```
imgobf -c -i <input> -o <output> [-p <password>]
imgobf -d -i <input> -o <output> [-p <password>]
```

| Option | Meaning                                                       |
|--------|---------------------------------------------------------------|
| `-c`   | Obfuscate (exactly one of `-c` / `-d` is required)            |
| `-d`   | De-obfuscate                                                  |
| `-i`   | Input image path                                              |
| `-o`   | Output image path                                             |
| `-p`   | Password (digits and ASCII letters). Default: `"0"`           |

### Examples

```bat
:: Obfuscate with the default password
imgobf -c -i input.png -o output.png

:: De-obfuscate with the default password
imgobf -d -i output.png -o back.png

:: Obfuscate / de-obfuscate with an explicit password
imgobf -c -i input.png  -o output.png -p dEepsEek123456
imgobf -d -i output.png -o back.png   -p dEepsEek123456
```

## How it works

1. The input image is decoded to 32-bit BGRA.
2. A Gilbert space-filling curve covering the `W × H` grid produces an index
   array `curve[i]` that visits every pixel exactly once.
3. The password is hashed with FNV-1a 64. The hash is reduced to an offset
   `off ∈ [1, W·H - 1]`.
4. Obfuscation: `dst[curve[(i + off) mod N]] = src[curve[i]]`.
5. De-obfuscation: `dst[curve[i]] = src[curve[(i + off) mod N]]`.
6. The result is encoded according to the output extension.

The curve keeps neighboring pixels close together in the 1D ordering, so the
cyclically shifted image still has strong local correlation and survives JPEG
compression with most colors intact.

## Format notes

| Extension         | Encoder      | Lossless       |
|-------------------|--------------|----------------|
| `.png`            | WIC PNG      | Yes            |
| `.bmp`            | WIC BMP      | Yes            |
| `.tif` / `.tiff`  | WIC TIFF     | Yes            |
| `.jpg` / `.jpeg`  | WIC JPEG     | No (quality 1.0) |

JPEG dimensions are limited to 65535 pixels per side.

## Caveats

- The offset space is `N - 1`, where `N = W · H`. For very small images two
  passwords can collide.
- Any scaling, rotation, cropping, or lossy re-encoding between `-c` and `-d`
  will prevent exact recovery.
- JPEG output loses the alpha channel (it is converted to 24-bit BGR).
- The output path must differ from the input path.
