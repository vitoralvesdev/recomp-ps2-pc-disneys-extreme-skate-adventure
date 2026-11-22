# Disney's Extreme Skate Adventure — Recompilação nativa para Windows

![Capa de Disney's Extreme Skate Adventure para PlayStation 2](docs/capa-jogo.png)

Projeto da comunidade, **em desenvolvimento**, para recompilar nativamente o jogo de
**PlayStation 2 Disney's Extreme Skate Adventure** para Windows x64. O candidato principal
usa código do jogo recompilado antecipadamente e serviços adaptados para o PC: **não é um
emulador**, não depende do PCSX2 e não interpreta as instruções do jogo em tempo de execução.
Há um protótipo legado separado para pesquisa, que não é o candidato principal.

O jogo, seus personagens, marcas e a capa pertencem aos respectivos detentores de direitos.
Este é um projeto não oficial, sem vínculo com eles.

O projeto combina quatro camadas próprias:

| Camada | O que faz |
| --- | --- |
| Runtime do host | Loop principal, scheduler, memória RDRAM/VRAM, IRQs, DMA, bridges de sistema |
| Código do jogo (AOT) | Funções do EE (R5900) e microprogramas de VU0/VU1 recompilados para C++ |
| GPU | GS (Graphics Synthesizer) traduzido para OpenGL 3.3, com merge de batches e apresentação em janela |
| Mídia | Áudio nativo e IOP reimplementados no host; decodificador de vídeo ligado, cutscenes ainda não validadas |

O iniciador valida o pacote, ajusta o ambiente, abre a janela com o título do jogo e inicia o runtime.

---

## Status de desenvolvimento

Baseado nos registros locais de **06/11/2026**; os resultados podem variar entre computadores.
O projeto ainda não foi validado do início ao fim.

| Área | Situação | O que funciona ou falta |
| --- | --- | --- |
| Inicialização e menus | Funciona nos testes | Menu e seleção de personagem aparecem nas sessões registradas. |
| Entrada em fase | Funciona nos testes | Há registros do HUB e de entrada em uma fase de Toy Story; outras fases precisam de testes. |
| Teclado e controle | Funciona, precisa melhorar | Há mapeamento de teclado e controle; compatibilidade com todos os modelos não foi validada. |
| Gameplay e desempenho | Funciona, precisa melhorar | Em uma sessão no HUB foram registrados aproximadamente 18–27 quadros lógicos/s; otimização continua necessária. |
| Áudio | Funciona, precisa melhorar | Sons e músicas foram iniciados nos registros; alguns bancos falham ao carregar e falta validar todas as fases. |
| Gráficos | Funciona, precisa melhorar | Menu e cenas aparecem; testes sintéticos de textura passaram, mas fidelidade geral e blending precisam de validação. |
| Colisão | Problemas conhecidos | Há relatos de rampas e objetos com colisão incorreta; a causa ainda precisa de investigação. |
| Carregamento | Problemas conhecidos | Algumas fases podem parar ao carregar; houve uma espera inicial longa nos testes. |
| Vídeos e cutscenes | Pendente de validação | O decodificador está ligado, mas as cutscenes não foram validadas no fluxo recente. |
| Saves | Pendente de validação completa | Há teste isolado do serviço, mas falta confirmar salvar e carregar uma partida completa. |
| Instalação e compilação limpa | Pendente | Ainda não há procedimento completo validado para gerar todos os arquivos necessários a partir de um clone novo. |

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

### Amostragem de textura GS (06/11/2026)

O backend OpenGL aplicava meio pixel de deslocamento nas coordenadas dos atributos e não
quantizava as coordenadas FST em 1/16 de texel, como o GS faz. O resultado era uma divergência
sistemática no filtro bilinear: **2.304 pixels** com erro máximo de canal 24.

A posição dos vértices agora vai exatamente para as coordenadas GS, as coordenadas FST são
arredondadas para 1/16 de texel no shader e o clamp de UV passou a usar centros de pixel. As
chamadas de limpeza de profundidade/espelho e as linhas foram ajustadas para manter a mesma
geometria.

| Teste de contrato GPU | Antes | Depois |
| --- | --- | --- |
| Textura RGBA com filtro linear | 2.304 px divergentes (erro 24) | **0 px (erro 0)** |
| Sprite plano, textura RGBA, PSMT8, alpha de framebuffer | 0 px | **0 px** |
| Profundidade compartilhada entre alvos | 0 diferenças | **0 diferenças** |
| Limpeza de cor/profundidade e alias Z32/cor | 0 diferenças | **0 diferenças** |

A sessão de validação de 10.500 frames confirmou que a correção não regrediu menu, seleção de
personagem, carregamento nem gameplay.

### Perfil de desempenho medido (06/11/2026)

Sessão de 10.500 frames no hub, com o build final:

| Métrica | Menu | Gameplay no hub |
| --- | --- | --- |
| FPS lógico | 59–60 | **18,3–27,2** |
| CPU do jogo (thread EE) | ~100% | 90–94% |
| VU1 recompilado | ~14% | **32–42%** |
| Unpack do VIF1 | ~19% | **45–54%** |
| Decode GIF/GS | ~6% | **17–22%** |
| Thread de GPU ocupada | ~74% | **67–75%** |
| Jobs VU1 | — | **~29 mil/s** |
| Submissões GS | — | **~666 mil/s** |

O gargalo restante é a execução do código do jogo no EE: VU1 recompilado, unpack do VIF1 e
decode GIF/GS. A thread de GPU fica mais de 60% ocupada, com dezenas de milhares de primitivas
GS por frame.

### Problemas conhecidos

- Gameplay em ~18–27 fps (média ~21 fps); menu a 60 fps.
- Hitch único de ~15 s ao entrar na fase (loader do jogo, antes do primeiro frame de gameplay).
- Áudio validado no menu e no hub; alguns bancos de som de fase ainda registram
  `couldn't load` no log durante o carregamento.
- Cutscenes em vídeo (`.PSS`) ainda não foram exercitadas pelo fluxo testado.
- Algumas rampas e objetos sem colisão correta, principalmente nas fases de Toy Story.
- Algumas fases podem parar durante o carregamento.

Relate defeitos informando a fase, o personagem, a ação que reproduz o problema e o log
`analysis/game-session.log` gerado pelo iniciador. **Nunca anexe ISO, WAD, saves ou outros dados
originais do jogo.**

---

## Como jogar pelo executável

**O repositório não inclui o executável pronto, o runtime nem os dados do jogo.** As instruções
abaixo se aplicam a um pacote de desenvolvimento já compilado e preparado localmente; ainda
não há uma instalação completa reproduzível a partir de um clone novo. É necessário ter uma
cópia legal da edição americana de PS2 (`SLUS_206.07`).

### 1. Prepare a pasta do jogo

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

Os dados de `original/` e `assets/` não acompanham o repositório. Extraia os dados da sua
própria cópia e mantenha a estrutura do disco. O catálogo de áudio em `assets/` exige preparação
adicional: apenas copiar os arquivos da ISO não o cria. O processo completo de preparação do
áudio e dos arquivos gerados ainda precisa ser documentado e validado.

O iniciador também exige os módulos IRX em `IOP/` e as DLLs FFmpeg no diretório `runtime/`.
Não compartilhe a ISO, os arquivos extraídos, os assets ou um runtime que incorpore código
gerado do jogo sem antes verificar os direitos de distribuição.

### 2. Execute

Dê **duplo clique em `Disney's Extreme Skate Adventure.exe`** (ou em `Jogar.cmd`).
O iniciador confere os arquivos, configura o renderer e abre o jogo na janela com o título
**Disney's Extreme Skate Adventure**.

Opções do iniciador:

| Comando | Efeito |
| --- | --- |
| `"Disney's Extreme Skate Adventure.exe" --verificar` | Verifica apenas a presença dos arquivos exigidos e retorna um código de saída; não testa a integridade nem o gameplay |
| `"Disney's Extreme Skate Adventure.exe" --diagnostico` | Sessão normal gravando evidências em `analysis/hang-reports` |

Também é possível abrir o runtime diretamente, a partir da raiz do projeto:

```bat
runtime\DisneySkateRuntime.exe original\extracted\SLUS_206.07
```

### 3. Se o jogo não abrir

O pacote local precisa das DLLs de mídia usadas pelo runtime. Em um Windows limpo, se aparecer uma mensagem
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

O runtime tenta usar o primeiro controle reconhecido pelo raylib. O mapeamento segue as posições
dos botões de um controle tipo Xbox; o teclado continua ativo. A compatibilidade varia conforme
o controle e ainda precisa de mais testes.

### Requisitos

O alvo atual é **Windows x64** com GPU e drivers compatíveis com **OpenGL 3.3**. Os requisitos
mínimos de processador, memória e espaço ainda não foram validados em diferentes computadores.

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
- `analysis/`, capturas de gameplay, logs e dumps — diagnósticos locais; a capa ilustrativa é uma exceção.
- `src/generated-*/`, `src/recovered/`, `src/host/recovered_functions.cpp`,
  `recomp/output/`, `recomp/recovered-overlap-entries.json`,
  `native/original/iop/generated/`, `native/original/vu/generated/` — saídas geradas a partir
  dos dados originais.

O `.gitignore` ajuda a impedir novos envios acidentais, mas **não remove arquivos já rastreados
nem apaga o histórico**. Revise a lista e os commits antes de publicar. Consulte
[docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

---

## Desenvolvimento

Pré-requisitos conhecidos: Windows x64, Visual Studio Build Tools com C++ x64, CMake e sua cópia
legal do jogo extraída localmente. O ambiente atual também depende de ferramentas, dependências
e código gerado que não acompanham o repositório. **Um clone limpo ainda não tem um processo de
compilação completo e validado.**

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

O comando acima foi usado no ambiente de desenvolvimento existente e pode não funcionar em um
clone limpo. Veja [docs/BUILDING.md](docs/BUILDING.md); a documentação de
[arquitetura](docs/arquitetura-nativa.md) descreve também uma etapa histórica do projeto.

## Como colaborar

Ajuda com desempenho, gráficos, áudio, colisão, preparação de dados e testes é bem-vinda. Ao
relatar um problema, informe a fase, a ação, o resultado e as configurações do computador.
Revise logs antes de compartilhá-los para remover caminhos e dados pessoais; nunca envie dados
originais, saves ou arquivos privados.

O projeto contém componentes de terceiros com suas próprias licenças, incluindo GPL-3.0; a
proveniência e a licença global ainda precisam de revisão. A capa e os demais direitos do jogo
pertencem aos respectivos titulares.
