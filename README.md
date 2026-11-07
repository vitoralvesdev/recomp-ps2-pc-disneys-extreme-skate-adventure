# Disney's Extreme Skate Adventure — Port nativo para Windows x64

Port nativo do jogo **Disney's Extreme Skate Adventure** (PlayStation 2, SLUS_206.07) para Windows x64.
O jogo roda a partir do código original recompilado para o PC: **não usa PCSX2 nem nenhum emulador de
PlayStation 2** e **não interpreta o binário do jogo em tempo de execução**.

O projeto combina quatro camadas próprias:

| Camada | O que faz |
| --- | --- |
| Runtime do host | Loop principal, scheduler, memória RDRAM/VRAM, IRQs, DMA, bridges de sistema |
| Código do jogo (AOT) | Funções do EE (R5900) e microprogramas de VU0/VU1 recompilados para C++ |
| GPU | GS (Graphics Synthesizer) traduzido para OpenGL 3.3, com merge de batches e apresentação em janela |
| Mídia | Áudio nativo, vídeo e IOP reimplementados no host, sem depender de BIOS ou emulador externo |

O iniciador valida o pacote, ajusta o ambiente, abre a janela com o título do jogo e inicia o runtime.

---

## Status de desenvolvimento

Atualizado em **06/11/2026**.

| Área | Estado |
| --- | --- |
| Boot e menu | Funcionando a 60 fps |
| Controle (teclado e USB) | Funcionando |
| Entrada em fase (Toy Story) | Funcionando; carregamento inicial único de ~15 s |
| Gameplay | Jogável, porém abaixo do alvo de 60 fps: **~19–22 fps** em gameplay carregado |
| Áudio | Parcial, em validação fase a fase |
| Colisão | Boa no geral; algumas rampas/objetos ainda falham |
| Vídeo/fidelidade gráfica | Avançado; ainda há diferenças pontuais de blending e apresentação |

### Otimização mais recente (06/11/2026)

A entrega das primitivas GS para a thread de GPU era feita uma a uma: cada uma das ~31 mil
primitivas por frame travava um mutex, alocava um nó de fila e acordava o worker. Isso consumia
**25% do frame**. As primitivas agora são acumuladas e entregues em blocos de 256, preservando a
ordem dos comandos.

| Métrica (frame 10500, mesma cena) | Antes | Depois |
| --- | --- | --- |
| FPS de gameplay | 17,06 | **21,69** |
| Tempo de GIF/GS no frame | 30% | **15%** |
| Hand-off GS (batch) | 25% | **9%** |

O gargalo restante é a execução do código do jogo no EE: VU1 recompilado (~36% do frame),
unpack do VIF1 (~14%) e decode GIF/GS (~15%). A thread de GPU fica ~62% ocupada, com
~31 mil primitivas e ~600 chamadas de desenho OpenGL por frame.

### Problemas conhecidos

- Gameplay em ~19–22 fps; menu a 60 fps.
- Hitch único de ~15 s ao entrar na fase (loader do jogo, antes do primeiro frame de gameplay).
- Áudio e vídeo ainda precisam de validação fase a fase.
- Algumas rampas e objetos sem colisão correta, principalmente nas fases de Toy Story.
- Algumas fases podem parar durante o carregamento.

Relate defeitos informando a fase, o personagem, a ação que reproduz o problema e o log
`analysis/game-session.log` gerado pelo iniciador. **Nunca anexe ISO, WAD, saves ou outros dados
originais do jogo.**

---

## Jogar (sem compilar nada)

O pacote pronto contém o iniciador, o runtime e as DLLs. Você **não precisa** instalar Visual Studio,
CMake, PCSX2, Ghidra ou Java para jogar.

### 1. Monte a pasta do jogo

Mantenha esta estrutura lado a lado:

```text
Disney's Extreme Skate Adventure.exe   <- iniciador (duplo clique)
Jogar.cmd                              <- atalho opcional
runtime/
  DisneySkateRuntime.exe               <- runtime do port
  *.dll                                <- DLLs de mídia/áudio
original/extracted/
  SLUS_206.07                          <- binário da sua cópia legal
  SKATE4.HED
  SKATE4.WAD
  IOP/*.IRX
  ...                                  <- demais dados extraídos
assets/
  native/audio/catalog.tsv
  ...                                  <- demais assets nativos
```

Os arquivos de `original/` e `assets/` **não são distribuídos** com o projeto: use a sua própria
cópia legal do jogo. Eles nunca entram no GitHub.

### 2. Execute

Dê **duplo clique em `Disney's Extreme Skate Adventure.exe`** (ou em `Jogar.cmd`).
O iniciador confere os arquivos, configura o renderer e abre o jogo na janela com o título
**Disney's Extreme Skate Adventure**.

Opções do iniciador:

| Comando | Efeito |
| --- | --- |
| `"Disney's Extreme Skate Adventure.exe" --verificar` | Só valida o pacote e mostra o que está faltando |
| `"Disney's Extreme Skate Adventure.exe" --diagnostico` | Sessão normal gravando evidências em `analysis/hang-reports` |

Também é possível abrir o runtime diretamente, a partir da raiz do projeto:

```bat
runtime\DisneySkateRuntime.exe original\extracted\SLUS_206.07
```

### 3. Se o jogo não abrir

O pacote traz as DLLs de mídia usadas pelo runtime. Em um Windows limpo, se aparecer uma mensagem
citando `VCRUNTIME140.dll` ou `MSVCP140.dll`, instale o **Microsoft Visual C++ Redistributable x64**
mais recente e reinicie o jogo:

<https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist>

Atualize também o driver da GPU pelo fabricante. O runtime usa **OpenGL 3.3**.

O iniciador grava `analysis/game-session.log` na raiz do projeto. Esse arquivo e a pasta `analysis/`
são locais, ignorados pelo Git.

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

Controles USB modernos são detectados pelo Windows. O mapeamento segue o padrão Xbox (A confirma,
B volta, Start pausa/avança) e usa a base SDL/raylib para outros controles compatíveis, incluindo
DualShock/DualSense em modo XInput ou SDL. O teclado continua ativo com um controle conectado.

### Requisitos

| Componente | Mínimo | Recomendado |
| --- | --- | --- |
| Sistema | Windows 10 64-bit | Windows 10/11 64-bit atualizado |
| Processador | 2 núcleos, 3 GHz | 4 núcleos modernos ou superior |
| Memória | 8 GB RAM | 16 GB RAM |
| Vídeo | GPU com OpenGL 3.3 e 2 GB VRAM | GPU dedicada moderna com driver atualizado |
| Espaço livre | 7 GB para dados extraídos e runtime | 12 GB para desenvolvimento e diagnósticos |
| Entrada | Teclado | Controle USB XInput/SDL também suportado |

---

## Estrutura do repositório

```text
src/host/          Runtime do host: loop principal, scheduler, memória, IRQs, bridges, áudio/vídeo
src/launcher/      Iniciador Windows (validação do pacote e janela do jogo)
native/original/   Port nativo: GS/OpenGL, VIF/GIF, VU, IOP, áudio, input, oclusão
native/            Conversor de assets e testes nativos de colisão
tests/             Testes do runtime e dos drivers gráficos
scripts/           Preparação de dados, compilação e análise
recomp/            Configuração da recompilação (pontos de entrada e bridges)
docs/              Documentação técnica, arquitetura e política de distribuição
```

O que **não** fica no Git:

- `original/`, `assets/`, `mc0/`, `mc1/` — dados do jogo e saves (direitos autorais).
- `runtime/`, `build/`, `dist/`, `*.exe`, `*.dll` — binários gerados.
- `analysis/`, capturas, logs e dumps — diagnósticos locais.
- `src/generated-*/`, `src/recovered/`, `recomp/output/`,
  `native/original/iop/generated/`, `native/original/vu/generated/` — código gerado a partir dos
  dados originais.

O `.gitignore` é a fonte de verdade. Antes de publicar, consulte
[docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

---

## Desenvolvimento

Pré-requisitos: Windows 10/11 x64, Visual Studio Build Tools com C++ x64, CMake 3.21+ e a sua cópia
legal do jogo extraída localmente.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\Compilar-Candidato-Original.ps1
```

Instruções detalhadas em [docs/BUILDING.md](docs/BUILDING.md); arquitetura em
[docs/arquitetura-nativa.md](docs/arquitetura-nativa.md).

Este repositório não possui uma licença global: autoria e dependências de terceiros ainda precisam
de revisão antes de publicação. Não presuma que o código ou os recursos possam ser reutilizados.
