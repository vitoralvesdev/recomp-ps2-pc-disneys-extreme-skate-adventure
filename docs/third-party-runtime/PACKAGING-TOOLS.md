# Procedência das ferramentas de preparação

Auditoria registrada em **27/11/2026**. Estes componentes processam a ISO fornecida pelo usuário; não são incluídos no runtime AOT.

## Python incorporado

- Versão: **3.12.10**, Windows x64 embeddable.
- Origem oficial: [python-3.12.10-embed-amd64.zip](https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip).
- Tamanho: **11.133.606 bytes**.
- SHA-256: `4acbed6dd1c744b0376e3b1cf57ce906f9dc9e95e68824584c8099a63025a3c3`.
- MD5 publicado pela Python.org e conferido localmente: `fe8ef205f2e9c3ba44d0cf9954e1abd3`.
- Licença: `tools\python\LICENSE.txt`, copiada do arquivo oficial.
- O pacote conserva `python312.zip`; não há instalação no sistema nem alteração do `PATH`.

## Neversoft Multitool CLI

- Versão: [v1.3.4](https://github.com/slfx77/neversoft-multitool/releases/tag/v1.3.4).
- Origem: [NeversoftMultitool-Windows-CLI-x64.zip](https://github.com/slfx77/neversoft-multitool/releases/download/v1.3.4/NeversoftMultitool-Windows-CLI-x64.zip).
- Tamanho: **14.664.275 bytes**.
- SHA-256: `062c89e00382e7b7c4b73015f228c88e7be34e50a4c48654d39e3142bd633d35`.
- Licença MIT da tag: `tools\neversoft-multitool-v1.3.4\LICENSE.md`.
- São copiados somente `NeversoftMultitool.exe` e a licença. O PDB, XML, scripts do Blender Exporter e o helper Blender/GPL não são necessários para a conversão de assets e não são distribuídos.

## Conversor COL8 próprio com .NET

O executável é publicado como aplicativo .NET **autocontido**, win-x64, em arquivo único. Isso inclui o runtime .NET e suas dependências dentro de `Col8Converter.exe`; não exige SDK ou runtime .NET instalado no computador do jogador.

- Código de integração: `tools-src\col8-converter`.
- Fontes COL v8: Neversoft Multitool, commit [`3c9028e0178deb9060190362d1c47039a32846e2`](https://github.com/slfx77/neversoft-multitool/tree/3c9028e0178deb9060190362d1c47039a32846e2), MIT; aviso em `tools\col8-converter\UPSTREAM-LICENSE.md`.
- SDK usado na build: registrado em `BUILD-INFO.txt`.
- Licenças do .NET: `third-party-notices\DOTNET-LICENSE.txt` e `third-party-notices\DOTNET-THIRD-PARTY.txt`, copiadas dos arquivos oficiais `LICENSE.txt` e `ThirdPartyNotices.txt` da instalação do SDK .NET usada no empacotamento. O `dotnet publish` autocontido não gera esses dois arquivos por padrão.

## Recuperação de música

`DisneyMusicRecover.exe` é compilado a partir de `scripts\music-recovery\recover_music.cpp` neste projeto, com C++17 e CRT estática do MSVC. O script Python `install_music.py` é executado pelo Python incorporado. Nenhuma ferramenta de desenvolvimento é necessária depois que o pacote é gerado.
