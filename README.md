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

O projeto inclui um assistente gráfico de instalação para selecionar uma ISO local, extrair e
validar os arquivos esperados e chamar as etapas existentes de preparação e compilação. Depois
de preparado, o launcher valida o pacote, ajusta o ambiente, abre a janela do jogo e inicia o
runtime. A instalação completa ainda depende de ferramentas e arquivos gerados que não estão no
repositório; o assistente informa quando uma etapa indisponível impede a conclusão.

---

## Status de desenvolvimento

Baseado na última melhoria de desempenho e nas medições locais de **23/11/2026**; os resultados podem variar entre computadores.
O projeto ainda não foi validado do início ao fim.

| Área | Situação | O que funciona ou falta |
| --- | --- | --- |
| Inicialização e menus | Funciona nos testes | Menu e seleção de personagem aparecem nas sessões registradas. |
| Entrada em fase | Funciona nos testes | Há registros do HUB e de entrada em uma fase de Toy Story; outras fases precisam de testes. |
| Teclado e controle | Funciona, precisa melhorar | Há mapeamento de teclado e controle; compatibilidade com todos os modelos não foi validada. |
| Gameplay e desempenho | Melhora significativa; em validação | **59,98 FPS lógicos em média** em Toy Story Bedroom (8 amostras entre 8400–10500 frames; faixa 59,82–60,14). Menu permanece perto de 60 FPS. Outras fases e comparação controlada com o HUB ainda precisam de validação. |
| Áudio | Funciona, precisa melhorar | Sons e músicas foram iniciados nos registros. O log recente mostra WAVs de efeitos vazios no catálogo (por exemplo, alguns sons Conc/Metal); a recuperação desses recursos ainda falta. |
| Gráficos | Funciona, precisa melhorar | Menu e cenas aparecem; testes sintéticos de textura passaram, mas fidelidade geral e blending precisam de validação. |
| Colisão | Dados do protótipo ampliados; gameplay ainda não validado | O preparador agora converte os arquivos COL v8 de todas as fases extraídas. A equivalência da colisão em gameplay e os relatos de rampas/objetos ainda precisam de validação. |
| Carregamento | Problemas conhecidos | Algumas fases podem parar ao carregar; houve uma espera inicial longa nos testes. |
| Vídeos e cutscenes | Pendente de validação | O decodificador está ligado, mas as cutscenes não foram validadas no fluxo recente. |
| Saves | Pendente de validação completa | Há teste isolado do serviço, mas falta confirmar salvar e carregar uma partida completa. |
| Assistente de instalação | Experimental, não validado de ponta a ponta | Extrai e verifica arquivos de uma ISO USA e chama os scripts locais. A geração do catálogo de áudio e alguns pré-requisitos de compilação ainda dependem do ambiente de desenvolvimento. |

### Melhoria de FPS em gameplay (23/11/2026)

O runtime AOT recebeu a correção de inicialização do registrador constante VF0 e dos acessos
EE à memória de dados do VU0, além do ajuste SIMD da normalização VU. Com esses ajustes, a
sessão instrumentada em `ToyStory_Bedroom` chegou perto do limite de 60 FPS lógicos durante
todo o trecho de gameplay amostrado. A medição confirma melhora expressiva nessa fase; ainda
falta repetir o mesmo benchmark controlado no HUB e nas demais fases para estabelecer o ganho
geral do projeto.

| Métrica instrumentada em ToyStory_Bedroom | Média (8 amostras, frames 8400–10500) |
| --- | ---: |
| FPS lógico | 59,98 (59,82–60,14) |
| VIF1 | 36,9% |
| VU1 | 26,2% |
| GIF/GS | 9,4% |
| Worker GPU ocupado | 72,4% |
| Espera de GPU | 25,5% |

O log confirmou `Loading Scene ToyStory_Bedroom ...... done`, `CAREER_STARTED` e `starting a run`.
As oito amostras de gameplay foram coletadas entre os frames 8400 e 10500. Como a sessão usou
instrumentação e uma fase diferente dos registros anteriores, os números abaixo documentam o
resultado observado; uma comparação A/B geral ainda requer repetir o cenário antigo com o mesmo
build e configuração.

### Problemas conhecidos

- O FPS das fases além de Toy Story Bedroom ainda precisa de uma medição controlada após a
  melhoria de 23/11/2026.
- Hitch único de ~15 s ao entrar na fase (loader do jogo, antes do primeiro frame de gameplay).
- Hitch único de ~15 s ao entrar na fase (loader do jogo, antes do primeiro frame de gameplay).
  A sessão de diagnóstico `analysis/audio-trace-run/session.log` confirma ausência de entradas
  em lógica/desenho nesse intervalo; o PC quente `0x002750a8` foi verificado no Ghidra e é uma
  leitura de timer, portanto não identifica a causa. As leituras dos arquivos de fase são
  registradas como 0 ms; o trabalho síncrono de parsing/construção no loader ainda precisa de
  uma medição dedicada por etapa.
- Áudio validado no menu e no hub; alguns bancos de som de fase ainda registram
  `couldn't load` no log durante o carregamento. A inspeção confirmou que vários WAVs
  correspondentes no catálogo têm tamanho zero; os sons de origem precisam ser recuperados.
- Cutscenes em vídeo (`.PSS`) ainda não foram exercitadas pelo fluxo testado.
- Colisão de rampas e objetos no jogo recompilado ainda não validada, principalmente nas fases de Toy Story. O kernel VU0 de interseção conferiu em 120 mil chamadas registradas, mas o caso visual problemático não aparece nesse corpus. O protótipo nativo agora pode preparar os COL v8 de todas as fases; isso não comprova correção no runtime do jogo.
- Algumas fases podem parar durante o carregamento.

Relate defeitos informando a fase, o personagem, a ação que reproduz o problema e o log
`analysis/game-session.log` gerado pelo iniciador. **Nunca anexe ISO, WAD, saves ou outros dados
originais do jogo.**

---

## Instalação e execução

**O repositório não inclui o executável pronto, o runtime nem os dados do jogo.** É necessária
uma cópia legal da edição americana de PS2. O assistente ainda é experimental: automatiza a
extração da ISO e chama as rotinas de preparação/compilação existentes, mas não torna o processo
reproduzível num clone limpo. Ferramentas locais, fontes geradas e o catálogo de áudio podem
estar ausentes.

### 1. Abra o assistente

No Windows, com PowerShell 5.1 ou superior, execute na raiz do projeto:

```powershell
powershell -STA -ExecutionPolicy Bypass -File .\scripts\Instalar-DisneySkate.ps1
```

Selecione a ISO USA. O assistente usa 7-Zip (incluído em `tools/7-Zip` no ambiente local ou
disponível no `PATH`), confere o executável `SLUS_206.07`, os arquivos HED/WAD e os módulos IOP,
e copia os arquivos extraídos para `original/extracted/`. Em seguida, tenta preparar os dados
nativos e compilar o runtime. O log da extração fica em `analysis/installer-7zip.log`.

O assistente não verifica o hash da ISO; valida a presença dos arquivos esperados da edição USA.
Ele não cria o catálogo `assets/native/audio/catalog.tsv`. Se esse catálogo, ferramentas locais,
ou código gerado estiverem ausentes, a etapa correspondente falha e mostra o motivo. A extração
é local: não há download da ISO nem dos dados do jogo.

### 2. Jogue após preparar um pacote válido

Quando o runtime e os assets estiverem presentes, dê **duplo clique em
`Disney's Extreme Skate Adventure.exe`** (ou em `Jogar.cmd`). O launcher confere o pacote,
configura o renderer e abre o jogo na janela com o título **Disney's Extreme Skate Adventure**.

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
