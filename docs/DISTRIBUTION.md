# Política de distribuição

O repositório é o código-fonte do projeto. Binários de terceiros, builds locais, dados do jogo e capturas não devem ser commitados. Um Release é um artefato separado, gerado a partir de um staging fechado e validado.

## Nunca enviar ao GitHub

- ISO, ELF, WAD/HED, IRX, texturas, modelos, músicas, vozes, vídeos ou qualquer outro dado extraído de Disney's Extreme Skate Adventure.
- Saves, memory cards, logs com dados pessoais, capturas de RAM/VRAM e dumps.
- `original/`, `assets/`, `game/`, `mc0/`, `mc1/`, `analysis/`, `logs/`, `saves/`, `build/` ou `dist/` como conteúdo do repositório.
- Certificados, senhas, chaves privadas, PDBs e intermediários de compilação.
- Ferramentas de terceiros sem versão, hash e licença auditados.
- Capas, logotipos ou artes do jogo fora do launcher. O ícone do launcher e do instalador usa a logo do jogo; a capa incorporada ao executável vem de `docs/capa-jogo.png`, e `src/launcher/cover.jpg` não é usado.

`.gitignore` não remove arquivos já rastreados nem apaga commits. Revise o histórico e os arquivos rastreados antes de publicar.

## O que um Release pode conter

Somente após a revisão dos direitos do runtime AOT e da assinatura Authenticode:

- `DisneySkateSetup-<versão>.exe`, assinado.
- `DisneySkate-<versão>-portable.zip`, com o launcher assinado e todos os arquivos sob `DisneySkate-<versão>/`.
- `SHA256SUMS.txt`.

O payload interno é limitado a:

- launcher, `README.md`, `BUILD-INFO.txt` e `package-files.sha256`;
- executável AOT e DLLs x64 explicitamente listadas em `scripts/Package.Common.psm1`;
- scripts próprios necessários à preparação e documentação selecionada;
- Python 3.12.10 incorporado, COL8 autocontido, recuperador de música nativo e Neversoft Multitool CLI v1.3.4;
- avisos de licença e procedência em `third-party-notices/`;
- `launcher.portable`, marcador vazio adicionado somente ao ZIP portátil para manter dados, saves e logs junto da extração; ele não entra no instalador nem no manifesto do payload.

A ISO e os dados extraídos são sempre fornecidos pelo jogador. O launcher os lê localmente; o processo de instalação não os envia para a internet.

## Geração e validação

O empacotador compila o launcher e seus testes, publica o COL8 como .NET autocontido win-x64 em arquivo único, compila o recuperador de música com CRT estática e baixa somente entradas com SHA-256 fixado. Em seguida:

1. limpa e monta `build/package/DisneySkate-<versão>`;
2. copia o runtime por allowlist e valida todos os binários como PE x64;
3. exige `BUILD-INFO.txt` do runtime e confere commit e versão;
4. anexa os avisos de terceiros;
5. assina o launcher quando `-PfxPath` é informado;
6. gera e valida `package-files.sha256`;
7. executa `scripts/Testar-Pacote-Distribuicao.ps1`, incluindo allowlist, caminhos seguros, hashes, versão e Python incorporado;
8. compila o Inno Setup e o ZIP portátil, adicionando `launcher.portable` somente ao ZIP;
9. extrai e audita novamente o ZIP portátil gerado;
10. assina o Setup.exe e grava `SHA256SUMS.txt`.

Comandos locais:

```powershell
.\scripts\Compilar-Launcher.ps1
.\scripts\Empacotar-Instalador.ps1
.\scripts\Testar-Pacote-Distribuicao.ps1 -PackageRoot .\build\package\DisneySkate-0.5.0
```

Para reutilizar builds já verificados, use `-SemCompilar`. Para embalar um artefato de runtime separado:

```powershell
.\scripts\Empacotar-Runtime.ps1 -NoticesDirectory .\docs\third-party-runtime
.\scripts\Empacotar-Instalador.ps1 -RuntimeZip .\dist\DisneySkateRuntime-0.5.0.zip -RuntimeSha256 <hash-do-zip>
```

Um ZIP de runtime com alterações locais é recusado por padrão; `-AllowDirtyRuntimeZip` existe apenas para validação local. O workflow `.github/workflows/windows-release.yml` consome um ZIP HTTPS com hash fixado e não publica nada por padrão.

### Validação local registrada

Em 27/11/2026, a variante `-TestInstall` do pacote 0.5.0 foi instalada em uma pasta nova com `PATH`, `TEMP`/`TMP` e `DISNEY_DATA_ROOT` isolados. O Setup extraiu 107 arquivos com código 0 e a auditoria independente do payload passou. A instalação real da ISO NTSC-U terminou com código 0, usando o COL8, o Python, o Neversoft Multitool e o recuperador de música incluídos, sem o fallback de desenvolvimento. Foram convertidas 272 colisões e produzidas 1.285 entradas de áudio, incluindo 37 músicas. O comando `--verificar` retornou 0 após conferir 7.569 arquivos com tamanho e SHA-256.

O launcher instalado abriu na tela pronta; um evento de Enter iniciou `DisneySkateRuntime.exe`, que criou uma janela `GLFW30`, inicializou OpenGL 3.3 e áudio, executou por 12 segundos e foi encerrado pelo teste. A raiz de saves apontou para a pasta isolada, e o launcher também foi encerrado com código 0. O ZIP portátil foi extraído em outro diretório e passou pela auditoria de 108 arquivos, incluindo o marcador `launcher.portable`; o launcher detectado nessa extração usou o modo portátil.

O instalador oficial não foi validado de ponta a ponta neste ambiente: a criação do registro de desinstalação em `HKCU` e a gravação de atalhos falharam com acesso negado. A variante `-TestInstall` omite somente o registro de desinstalação e os atalhos; portanto, ela valida o payload e o fluxo do jogo, mas não substitui o teste do instalador oficial em uma conta sem essas restrições. O pacote local permanece sem assinatura e sem Release publicado.

## Assinatura e atualização

O atualizador do launcher executa somente um Setup.exe com assinatura Authenticode confiável. Por isso:

- Setup.exe de Release deve ser assinado antes da publicação;
- o launcher dentro do ZIP portátil também deve ser assinado;
- o ZIP em si não é um instalador executável e pode circular sem assinatura de arquivo, mas sua build portátil deve ser distribuída manualmente enquanto não houver uma atualização automática confiável;
- hashes não substituem assinatura.

O workflow de publicação exige `publish=true`, uma tag existente no commit em build, a variável `RUNTIME_REDISTRIBUTION_APPROVED=yes` e os segredos `WINDOWS_SIGNING_PFX_BASE64` e `WINDOWS_SIGNING_PASSWORD`. A senha é lida de `DISNEY_SIGNING_PASSWORD` e não deve aparecer em logs ou arquivos.

## Direitos e limites

O repositório não possui licença global porque autoria e dependências ainda estão em revisão. A procedência e as licenças das DLLs de terceiros estão em [third-party-runtime/PROVENANCE.md](third-party-runtime/PROVENANCE.md); a das ferramentas de preparação está em [third-party-runtime/PACKAGING-TOOLS.md](third-party-runtime/PACKAGING-TOOLS.md).

O runtime AOT contém código derivado do jogo original. A existência do código-fonte não prova autorização para redistribuí-lo. A revisão dos direitos é obrigatória antes de qualquer Release público. O pacote local pode ser validado pelo responsável sem publicar dados proprietários.

O port continua em desenvolvimento. A release deve registrar limitações reais de fases, carregamento, gráficos, áudio, controles e saves, sem afirmar compatibilidade não testada.
