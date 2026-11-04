# Disney's Extreme Skate Adventure — Windows

Port em desenvolvimento para Windows x64 de **Disney's Extreme Skate Adventure** (PS2, SLUS_206.07). O jogo inicia nativamente no Windows por meio do executável incluído; ele **não usa PCSX2** para executar.

> Estado atual: o menu, o controle e a entrada em fase funcionam. Ainda existem defeitos conhecidos de renderização, colisão e estabilidade em algumas fases. Este repositório é de desenvolvimento, não uma distribuição final do jogo.

## Jogar

1. Baixe ou clone o projeto e mantenha as pastas `runtime`, `original` e `assets` lado a lado com o executável.
2. Forneça seus próprios arquivos da versão norte-americana de PS2 que você possui legalmente. O arquivo necessário é `original/extracted/SLUS_206.07`; os demais dados extraídos usados pelo jogo ficam em `original/extracted/`.
3. Abra **`Disney's Extreme Skate Adventure.exe`** na raiz da pasta. Alternativamente, `Jogar.cmd` chama o mesmo iniciador.

O título da janela deve ser **Disney's Extreme Skate Adventure**. Não procure nem execute `runtime/DisneySkateRuntime.exe` diretamente: o iniciador raiz verifica a estrutura de arquivos e o abre com os argumentos corretos.

### Se o jogo não abrir

O pacote traz as DLLs de mídia usadas pelo runtime. Em um Windows limpo, se houver uma mensagem citando `VCRUNTIME140.dll` ou `MSVCP140.dll`, instale o **Microsoft Visual C++ Redistributable x64** mais recente e reinicie o jogo:

<https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist>

Não é necessário instalar PCSX2, Ghidra, Java, Visual Studio ou CMake para **jogar**. Essas ferramentas só são usadas no desenvolvimento e na análise.

### Controles

Teclado:

| Ação | Tecla |
| --- | --- |
| Direção | WASD ou setas |
| Confirmar / botão X | Espaço ou X |
| Voltar / círculo | C |
| Start | Enter |
| Select | Tab |
| L1 / R1 | Q / E |
| L2 / R2 | Shift esquerdo / Shift direito |

Controles USB modernos são detectados pelo Windows. O mapeamento segue o padrão do controle Xbox (A confirma, B volta, Start pausa/avança) e usa a base SDL/raylib para outros controles compatíveis, incluindo DualShock/DualSense em modo XInput ou SDL. O teclado continua ativo mesmo com um controle conectado.

## Requisitos

Requisitos mínimos provisórios — ainda não são uma certificação de desempenho, porque o port está em desenvolvimento:

| Componente | Mínimo | Recomendado |
| --- | --- | --- |
| Sistema | Windows 10 64-bit | Windows 10/11 64-bit atualizado |
| Processador | 2 núcleos, 3 GHz | 4 núcleos modernos ou superior |
| Memória | 8 GB RAM | 16 GB RAM |
| Vídeo | GPU com OpenGL 3.3 e 2 GB VRAM | GPU dedicada moderna com driver atualizado |
| Espaço livre | 7 GB para dados extraídos e runtime | 12 GB para desenvolvimento e diagnósticos |
| Entrada | Teclado | Controle USB XInput/SDL também suportado |

Atualize o driver da GPU diretamente pelo fabricante (NVIDIA, AMD ou Intel). O runtime utiliza OpenGL; o PC não precisa de uma GPU de PlayStation 2 ou emulador.

## Dados do jogo e publicação

Os arquivos da ISO, dados extraídos, áudio, saves e binários gerados **não entram no GitHub**. Eles são protegidos por direitos autorais e cada jogador deve obtê-los da sua própria cópia legal. O `.gitignore` foi configurado para evitar inclusão acidental desses arquivos, bem como de builds, logs e capturas de diagnóstico.

O repositório contém o código do host Windows, scripts de preparação/análise e documentação. Para preparar dados locais de desenvolvimento, use `scripts/Preparar-Dados-Nativos.ps1` depois de colocar os dados legalmente extraídos nas pastas esperadas. Esse procedimento requer ferramentas de desenvolvimento e não é necessário para jogar uma compilação já preparada.

## Problemas conhecidos

- Pode haver piscadas/quadros misturados durante o gameplay.
- Algumas rampas e objetos ainda podem não ter colisão correta, sobretudo nas fases de Toy Story.
- Algumas fases podem parar durante o carregamento.
- O áudio e a fidelidade de vídeo ainda exigem validação fase a fase.

Relate defeitos com a fase, personagem, ação que os reproduz e o conteúdo de `analysis/game-session.log` quando disponível. Não anexe ISO, WAD, saves pessoais ou outros dados originais do jogo.

## Desenvolvimento

Para compilar, execute `scripts/Compilar-Candidato-Original.ps1` em uma máquina de desenvolvimento já preparada. Consulte [docs/BUILDING.md](docs/BUILDING.md) para os pré-requisitos e [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md) antes de criar uma release. Ferramentas de análise e referências técnicas ficam em `docs/`.
