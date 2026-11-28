# Procedência do runtime de terceiros

Auditoria registrada em **27/11/2026**. Este documento acompanha apenas os binários de terceiros usados pelo runtime Windows; não autoriza a redistribuição do código recompilado do jogo, da ISO nem dos dados extraídos.

## Conjunto FFmpeg entregue

- Projeto de empacotamento: [System233/ffmpeg-msvc-prebuilt](https://github.com/System233/ffmpeg-msvc-prebuilt), commit [`26dba63b3c0563bb9c01288a1682b1e03b6ef92d`](https://github.com/System233/ffmpeg-msvc-prebuilt/tree/26dba63b3c0563bb9c01288a1682b1e03b6ef92d).
- Release: [`n7.1-241205`](https://github.com/System233/ffmpeg-msvc-prebuilt/releases/tag/n7.1-241205).
- Arquivo de origem: [ffmpeg-n7.1-241205-lgpl-amd64-shared.zip](https://github.com/System233/ffmpeg-msvc-prebuilt/releases/download/n7.1-241205/ffmpeg-n7.1-241205-lgpl-amd64-shared.zip).
- Tamanho: **35.240.955 bytes**.
- SHA-256: `b985887dbca8c0a9a4c47f9d2bac7ec3ab994335b1b7fe7000bdb1dcd772ac68`.
- Variante **LGPL**, amd64, DLLs compartilhadas. O pacote não usa a variante GPL nem x264/x265.
- Licença do conjunto de build: `FFmpeg-prebuilt-LICENSE` (MIT).
- Licença do FFmpeg: `FFmpeg-COPYING.LGPLv2.1` (LGPL-2.1-or-later).

O empacotador exige esse SHA-256 antes de extrair ou reutilizar o arquivo. O runtime é distribuído como executável próprio e essas bibliotecas permanecem em DLLs substituíveis, sem vinculação estática do FFmpeg.

## Commits exatos das fontes

Os commits abaixo são os submodules registrados no commit de empacotamento indicado acima.

| Componente | Licença no pacote | Commit/tag | Aviso incluído |
| --- | --- | --- | --- |
| FFmpeg | LGPL-2.1-or-later | [`b08d7969c550a804a59511c7b83f2dd8cc0499b8`](https://github.com/FFmpeg/FFmpeg/tree/b08d7969c550a804a59511c7b83f2dd8cc0499b8) | `FFmpeg-COPYING.LGPLv2.1` |
| SDL2 | zlib | [`c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc`](https://github.com/libsdl-org/SDL/tree/c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc) | `SDL2-LICENSE.txt` |
| FreeType | FTL ou GPL-2.0 | [`42608f77f20749dd6ddc9e0536788eaad70ea4b5`](https://gitlab.freedesktop.org/freetype/freetype/-/tree/42608f77f20749dd6ddc9e0536788eaad70ea4b5) | `FreeType-FTL.txt`, `FreeType-GPLv2.txt` |
| FriBidi | LGPL-2.1-or-later | [`68162babff4f39c4e2dc164a5e825af93bda9983`](https://github.com/fribidi/fribidi/tree/68162babff4f39c4e2dc164a5e825af93bda9983) | `FriBidi-COPYING` |
| HarfBuzz | MIT | [`9ef44a2d67ac870c1f7f671f6dc98d08a2579865`](https://github.com/harfbuzz/harfbuzz/tree/9ef44a2d67ac870c1f7f671f6dc98d08a2579865) | `HarfBuzz-COPYING` |
| libass | ISC | [`e46aedea0a0d17da4c4ef49d84b94a7994664ab5`](https://github.com/libass/libass/tree/e46aedea0a0d17da4c4ef49d84b94a7994664ab5) | `libass-COPYING` |
| libjxl | BSD-3-Clause | [`538c140df1e5390ce51b90a81058c53ca5af5aad`](https://github.com/libjxl/libjxl/tree/538c140df1e5390ce51b90a81058c53ca5af5aad) | `libjxl-LICENSE` |
| libvpx | BSD-3-Clause | [`6f0c446c7b88d384a1c09caf33ec132e7ee24aea`](https://github.com/webmproject/libvpx/tree/6f0c446c7b88d384a1c09caf33ec132e7ee24aea) | `libvpx-LICENSE` |
| libwebp e sharpyuv | BSD-3-Clause | [`4c85d860ea4ebddd47e4718880036268a80c1492`](https://github.com/webmproject/libwebp/tree/4c85d860ea4ebddd47e4718880036268a80c1492) | `libwebp-COPYING` |
| nv-codec-headers | MIT | [`c69278340ab1d5559c7d7bf0edf615dc33ddbba7`](https://github.com/FFmpeg/nv-codec-headers/tree/c69278340ab1d5559c7d7bf0edf615dc33ddbba7) | `nv-codec-headers-NOTICE.txt` |
| OpenEXR | BSD-3-Clause | [`55d1a1404cec5b4b187009d9f7fe55a5622ac4e5`](https://github.com/AcademySoftwareFoundation/openexr/tree/55d1a1404cec5b4b187009d9f7fe55a5622ac4e5) | `OpenEXR-LICENSE.md` |
| zlib | zlib | [`51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf`](https://github.com/madler/zlib/tree/51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf) | `zlib-LICENSE` |

Dependências transitivas identificadas na árvore de build:

| Componente | Licença no pacote | Commit/tag | Aviso incluído |
| --- | --- | --- | --- |
| Brotli | MIT | [`36533a866ed1ca4b75cf049f4521e4ec5fe24727`](https://github.com/google/brotli/tree/36533a866ed1ca4b75cf049f4521e4ec5fe24727) | `Brotli-LICENSE` |
| Imath | BSD-3-Clause | [`v3.1.12`](https://github.com/AcademySoftwareFoundation/Imath/tree/v3.1.12) | `Imath-LICENSE.md` |

O sharpyuv não possui arquivo de licença separado neste commit; `sharpyuv/` integra a árvore do libwebp e coberto pelo `COPYING` principal.

## Microsoft Visual C++ CRT

São incluídos apenas `msvcp140.dll`, `vcruntime140.dll` e `vcruntime140_1.dll`, sem modificação, dentro de `runtime/`.

- Texto oficial de redistribuição: [Visual Studio 2022 Redistribution](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution), obtido por [aka.ms/vs/17/redist.txt](https://aka.ms/vs/17/redist.txt).
- Cópia local: `MSVC-REDIST.txt`.
- A seleção local usa `vswhere` e a pasta `VC\Redist\MSVC\<versão>\x64\Microsoft.VC143.CRT`; nunca uma pasta de debug.
- O launcher usa a CRT estática do MSVC. O runtime AOT e suas DLLs mantêm as DLLs da CRT dentro da própria pasta `runtime/`.

## Oferta de código-fonte e substituição das DLLs

O código-fonte correspondente está público nos links de commit/tag acima. O conjunto de scripts usado para gerar os binários está em [`.github/workflows`](https://github.com/System233/ffmpeg-msvc-prebuilt/tree/26dba63b3c0563bb9c01288a1682b1e03b6ef92d/.github/workflows), [build.sh](https://github.com/System233/ffmpeg-msvc-prebuilt/blob/26dba63b3c0563bb9c01288a1682b1e03b6ef92d/build.sh), [build-ffmpeg.sh](https://github.com/System233/ffmpeg-msvc-prebuilt/blob/26dba63b3c0563bb9c01288a1682b1e03b6ef92d/build-ffmpeg.sh), [patches](https://github.com/System233/ffmpeg-msvc-prebuilt/tree/26dba63b3c0563bb9c01288a1682b1e03b6ef92d/patches) e [.gitmodules](https://github.com/System233/ffmpeg-msvc-prebuilt/blob/26dba63b3c0563bb9c01288a1682b1e03b6ef92d/.gitmodules).

Para cumprir os termos LGPL, a distribuição deve conservar estes avisos e os links de fonte, não impedir a substituição ou engenharia reversa das DLLs para depuração de modificações e disponibilizar o código-fonte correspondente pelo mesmo canal do binário. A lista de DLLs e o SHA-256 do ZIP de origem permitem auditar uma build. O `BUILD-INFO.txt` do runtime registra o commit do port e o SHA-256 do executável AOT, sem incluir dados do jogo.

## Limites desta auditoria

- Os hashes e commits acima identificam o conjunto de terceiros; não provam equivalência funcional em todas as fases.
- Os direitos de redistribuição do runtime AOT, que contém código derivado do jogo original, precisam ser confirmados pelo responsável antes de publicar um Release público.
- Nenhum arquivo de `original/`, ISO, WAD, PSS, VAG, ELF, save ou dado extraído faz parte deste diretório.
