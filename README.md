# Disney's Extreme Skate Adventure — versão para Windows

![Capa de Disney's Extreme Skate Adventure para PlayStation 2](docs/capa-jogo.png)

> **Estado: em desenvolvimento — atualizado em 24/11/2026.**
> Nos testes locais, o projeto já abre o menu, permite escolher personagem e chega ao HUB e a uma fase de Toy Story. Ainda não é um port completo: algumas fases podem travar, o carregamento pode demorar e vários recursos ainda precisam de validação.

## O que é este projeto?

É um projeto não oficial da comunidade para transformar o código de **Disney's Extreme Skate Adventure**, de PlayStation 2, em um programa para **Windows x64**.

Não é um emulador como o PCSX2: o código do jogo é convertido antecipadamente para C++ e os serviços do console são adaptados para o PC. O projeto ainda depende de uma cópia legal da versão americana do jogo.

**O repositório não inclui um executável pronto, o programa principal que executa o jogo nem os dados do jogo.**

## Progresso da recompilação

Atualizado em **24/11/2026**. O percentual só é usado quando existe um denominador conhecido; **não mensurável** não significa 0%.

| Componente | Progresso | O que já está feito | O que ainda impede 100% |
| --- | --- | --- | --- |
| **EE (R5900)** | **100% da exportação rastreada** | 9.390 funções no escopo Ghidra, incluindo 55 stubs de runtime; 41.864 handlers únicos na tabela AOT; nenhuma entrada ignorada na exportação. Metadados em [recomp/ghidra.toml](recomp/ghidra.toml). | Validar e corrigir os caminhos executados, tratar os stubs e tornar o build AOT reproduzível sem os arquivos locais. |
| **VU0** | **Não mensurável globalmente** | 1 imagem registrada (`A71F9755…`) e 27 blocos rápidos gerados. O kernel de colisão da entrada `0x7e8` passou na regressão offline com 10 casos, incluindo chão, rampa, erros, limites e winding invertido. | Inventariar as demais imagens e validar colisão, rampas e objetos durante o jogo. |
| **VU1** | **Não mensurável globalmente** | 1 imagem registrada (`87BD03B7…`) e 254 blocos rápidos gerados, usados pelo runtime AOT. | Cobrir as variantes de microprograma e validar os jobs em todas as fases e telas. |
| **VIF0 / VIF1** | **Parcial, sem percentual** | Parser de comandos, DMA direto/em chain e regressões de endereço inválido e END vazio. | Medir cobertura de todos os opcodes/caminhos e validar o fluxo gráfico de ponta a ponta. |
| **GIF / GS** | **Parcial, sem percentual** | Backend OpenGL 3.3, worker GPU e testes sintéticos de contrato GPU e de pacotes GIF. | Comprovar equivalência visual completa, transparência, profundidade e frames em todas as fases. |
| **Jogo completo** | **Em validação** | Menu, seleção de personagem, HUB, entrada em uma fase de Toy Story e gameplay inicial registrados localmente. | Validar todas as fases, personagens, ações, saves, áudio, vídeo, colisão, carregamento e instalação reproduzível. |

Os totais de geração foram conferidos no ambiente local. `src/generated-*` e `native/original/vu/generated/` não são versionados; por isso, um clone limpo ainda não reproduz o runtime AOT completo, conforme [docs/BUILDING.md](docs/BUILDING.md). O marco de **100% do EE** acima é de exportação/geração, não de fidelidade nem de conclusão do port.

### Evolução de FPS registrada

| Data | Cenário | Resultado | Comparação ou limite |
| --- | --- | ---: | --- |
| 06/11/2026 | HUB, frame 10500 | **21,69 FPS** após otimização do GS | 17,06 FPS antes, na mesma cena |
| 06/11/2026 | HUB, perfil de 10.500 frames | **18,3–27,2 FPS** no gameplay | Menu entre 59 e 60 FPS |
| 23/11/2026 | `ToyStory_Bedroom`, frames 8400–10500 | **59,98 FPS** (59,82–60,14) | 8 amostras; VIF1 36,9%, VU1 26,2% e GIF/GS 9,4% de ocupação |

A medição de 23/11 está documentada no histórico do projeto, mas sua captura bruta de FPS não foi localizada no ambiente atual. Ela não foi repetida agora e não deve ser tratada como um benchmark de todas as fases; os cenários de 06/11 e 23/11 também não formam uma comparação A/B. O port só poderá ser declarado **100% recompilado e validado** quando todos os itens da tabela estiverem concluídos e o fluxo completo do jogo for testado.

## O que já foi corrigido ou funciona nos testes locais

- **Menus e início do jogo:** menu, seleção de personagem, HUB e entrada em uma fase de Toy Story foram registrados em sessões locais.
- **Desempenho:** correções no VU0 melhoraram o fluxo de imagens. Em `ToyStory_Bedroom`, a média foi de **59,98 quadros por segundo (FPS)** em 8 amostras, perto do alvo de 60 FPS. Isso ainda precisa ser confirmado nas outras fases.
- **Launcher experimental:** o programa de instalação exige o `BOOT2` exato e o SHA-256 de referência do ELF americano `SLUS_206.07`, extrai os arquivos com progresso, compara extrações por hash, configura o jogo e mostra logs. O pacote recebe um manifesto SHA-256 verificado antes de liberar o jogo. A verificação manual de atualizações mostra carregamento e um modal quando não há versão nova.
- **Atualizações mais seguras:** o launcher só aceita HTTPS, baixa apenas o instalador oficial esperado, confere o tamanho e a impressão digital SHA-256 e exige uma assinatura digital confiável do Windows antes de executar.
- **Cartão de memória (memory card):** caminhos com drive, caminhos absolutos do Windows, caminhos que tentam sair da pasta (`..`) ou links externos agora são bloqueados. Leituras e escritas também verificam o intervalo inteiro de memória.
- **Controles:** teclado e controle têm mapeamento inicial, com posições baseadas em um controle tipo Xbox.
- **Gráficos e áudio:** menus e cenas são exibidos e testes básicos de textura passaram. Em 24/11/2026, o som inicial e o áudio da gameplay foram confirmados funcionando no jogo. A fidelidade visual e a cobertura integral do áudio ainda não estão validadas.
- **Build do launcher:** compila separadamente, sem ISO ou código gerado do jogo. A última execução local passou em **100 verificações, sem falhas**, incluindo a validação da ISO americana e da extração existente.
- **Áudio pela ISO:** o instalador não exige pacotes externos; converte VAGs/streams e recupera as músicas de `MUSIC.WAD` da própria imagem selecionada. Em 24/11/2026, um teste isolado produziu **1.285 entradas** — 393 efeitos, 855 streams e 37 músicas — sem erros de conversão, arquivos ausentes ou músicas pendentes.
- **Vídeos da abertura:** os cinco `.PSS` iniciais contêm vídeo MPEG-2 e áudio SS2/PCM incorporado. O leitor foi validado offline em todos eles como PCM16 little-endian, 48 kHz e estéreo; a quantidade de áudio decodificada corresponde exatamente ao tamanho declarado em cada arquivo. O áudio agora é entregue continuamente ao backend Windows, e os frames são gravados no guest como macroblocos RGBA 16×16 column-major, com pixels row-major e stride de `0x400` bytes por bloco, conforme o consumidor original. O ELF contém caminhos genéricos `host`/`hdd` do motor, mas não há evidência de dependência de áudio externo nessa abertura. Em 24/11/2026, o áudio inicial foi confirmado funcionando no jogo; a apresentação visual e a mixagem completa ainda precisam de validação.
- **Auditoria de recursos:** os 26 `.PSS` nomeados no ELF têm correspondente extraído, e todos contêm o mesmo cabeçalho de áudio SS2. O diretório real da ISO não contém duas referências explícitas do ELF: `IOP/USBKB.IRX` e `IOP/DIAL_SPD.CNF`. As strings vizinhas de `USBKB.IRX` indicam teclado USB/debug; `DIAL_SPD.CNF` aparece em `dial=cdrom0:\IOP\DIAL_SPD.CNF;1`. Nenhuma das duas tem relação conhecida com a abertura, mas classificar seus fluxos como opcionais exige os XREFs no Ghidra.
- **Handlers originais copiados:** o runtime registra os blocos do kernel copiados para `0x80075000` e `0x80076000`, com aliases físicos/KSEG0 e retomada nos pontos internos. Os testes confirmam a tabela original de syscalls, o despacho dos handlers e a chamada aninhada `0x75158 → 0x3042ac`.
- **Falhas de runtime explícitas:** syscalls desconhecidos retornam erro e funções EE ausentes param por padrão; DMA GIF/VIF direto ou em chain com endereço inválido não sinaliza conclusão nem IRQ.
- **Dados de colisão:** o preparador já converte os arquivos de colisão COL v8 das fases extraídas. O kernel VU0 original (entrada `0x7e8`) tem regressão offline para chão, rampa, acertos e erros, limites do segmento, triângulo degenerado, bordas e winding invertido. Isso ainda não comprova que rampas e objetos estejam corretos durante o jogo.

## O que ainda precisa melhorar

1. **Validação completa do jogo:** testar todas as fases, personagens, ações, salvamentos e telas do início ao fim.
2. **Carregamento:** algumas fases podem parar. Foi registrada uma espera de cerca de 15 segundos ao entrar em uma fase, e a causa ainda não foi isolada.
3. **Desempenho nas demais fases:** repetir a mesma medição controlada no HUB e nas outras etapas.
4. **Colisão:** confirmar rampas, objetos e intersecções no jogo recompilado, especialmente nas fases de Toy Story.
5. **Áudio:** o som inicial e o áudio da gameplay estão funcionando, mas ainda é necessário validar a mixagem em partidas completas. A varredura atual não encontrou WAVs vazios ou ausentes no catálogo, mas os logs ainda registram `couldn't load` para sons que existem no catálogo; a causa dessa divergência não foi isolada.
6. **Vídeos:** o demux PSS, os timestamps PES/DTS, o áudio incorporado e o layout de macroblocos foram corrigidos e validados offline. Em 24/11/2026, `runtime_flow`, `graphics_driver` e `DisneyLauncherTests` passaram, e o launcher verificou 7.573 arquivos do pacote; ainda é necessário confirmar os frames, a sincronização e o áudio das cutscenes no fluxo do jogo.
7. **Saves:** salvar e carregar uma partida completa ainda não foi testado do começo ao fim.
8. **Gráficos e controles:** revisar fidelidade, mistura de cores e transparências; testar mais modelos de controle.
9. **Instalação pública:** o programa principal ainda não pode ser montado do zero em um clone limpo. O launcher ainda não baixa e compila todas as ferramentas sozinho; a preparação dos arquivos do jogo e a compilação continuam dependendo do ambiente local.
10. **Instalador assinado:** o `Setup.exe` local ainda não possui assinatura confiável. O atualizador reforçado só deve liberar downloads depois que um instalador assinado for publicado.
11. **Requisitos e licenças:** definir requisitos mínimos por testes em outros computadores e revisar a origem e as licenças dos componentes de terceiros.

## Como testar

Você precisa de uma **cópia legal da versão americana de PlayStation 2 (NTSC-U)**. A instalação usa cerca de **2,3 GB** de espaço livre.

1. Compile o launcher:

   ```powershell
   .\scripts\Compilar-Launcher.ps1
   ```

2. Abra `Disney's Extreme Skate Adventure.exe`.
3. Escolha a ISO americana e clique em **Instalar jogo**.
4. Quando o pacote estiver pronto, clique em **JOGAR**.

A extração é incremental e só pula arquivos cujo tamanho e SHA-256 coincidem com a ISO. O manifesto `package-manifest.sha256` cobre runtime, dados do jogo e assets, exclui saves e é regerado ao final da instalação. A ISO não é enviada para a internet.

**Importante:** um clone limpo compila o launcher, mas ainda não monta sozinho o programa principal e os arquivos preparados do jogo. Essas etapas continuam usando os scripts e arquivos gerados do ambiente de desenvolvimento.

## Controles

| Ação | Teclado |
| --- | --- |
| Direção | WASD ou setas |
| Confirmar / botão X | Espaço ou X |
| Voltar / círculo | C |
| Start | Enter |
| Select | Tab |
| L1 / R1 | Q / E |
| L2 / R2 | Shift esquerdo / Shift direito |

O jogo tenta usar o primeiro controle reconhecido. A compatibilidade varia conforme o modelo.

## Requisitos

- Windows x64.
- GPU e driver compatíveis com **OpenGL 3.3**.
- Cerca de **2,3 GB livres** para os dados extraídos.
- Microsoft Visual C++ Redistributable x64, se o Windows pedir `VCRUNTIME140.dll` ou `MSVCP140.dll`.

Os requisitos mínimos de processador e memória ainda não foram medidos em diferentes computadores.

## Se algo não abrir

Atualize o driver da GPU pelo fabricante. Se faltar uma DLL do Microsoft Visual C++, instale o [Redistributable x64 mais recente](https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist).

O launcher grava `analysis\launcher.log`, `analysis\game-session.log` e relatórios de travamento. Em uma instalação, os logs ficam em `%LOCALAPPDATA%\HighSky\DisneySkate\logs`.

Ao relatar um problema, informe a fase, o personagem, a ação e o resultado. **Nunca anexe ISO, WAD, saves ou outros dados originais do jogo.**

---

<details>
<summary><strong>Detalhes para quem compila ou colabora</strong></summary>

### Build isolado do launcher

```powershell
.\scripts\Compilar-Launcher.ps1
# ou:
cmake -S . -B build/launcher -G "Visual Studio 17 2022" -A x64 -DDISNEY_BUILD_LAUNCHER_ONLY=ON
cmake --build build/launcher --config Release --target DisneySkateLauncher DisneyLauncherTests --parallel 2
ctest --test-dir build/launcher -C Release --output-on-failure
```

O workflow Windows executa esse mesmo alvo e os testes. Veja [docs/launcher.md](docs/launcher.md).

### Comandos do launcher

| Comando | Efeito |
| --- | --- |
| `"Disney's Extreme Skate Adventure.exe" --verificar` | Verifica o pacote e retorna 0 (pronto) ou 2 (incompleto); não testa gameplay |
| `"Disney's Extreme Skate Adventure.exe" --iso <imagem> --verificar` | Valida somente a ISO escolhida |
| `"Disney's Extreme Skate Adventure.exe" --instalar <imagem>` | Instala sem interface, útil para automação |
| `"Disney's Extreme Skate Adventure.exe" --diagnostico` | Sessão normal com relatórios em `analysis/hang-reports` |
| `"Disney's Extreme Skate Adventure.exe" --versao` | Mostra a versão do launcher |

### Runtime e instalador local

O runtime completo continua dependente de PS2Recomp, bibliotecas, dados do jogo e código recompilado gerado localmente:

```powershell
.\scripts\Compilar-Candidato-Original.ps1
```

Com o pacote pronto, também é possível abrir o runtime diretamente:

```bat
runtime\DisneySkateRuntime.exe original\extracted\SLUS_206.07
```

Com o Inno Setup 6, o script abaixo gera um instalador local:

```powershell
.\scripts\Empacotar-Instalador.ps1
```

O resultado fica em `dist\DisneySkateSetup-0.3.0.exe`. Ele ainda não está assinado e inclui os scripts/fontes próprios de preparação, mas não a ISO, os dados do jogo, áudio derivado, assets gerados nem ferramentas de terceiros. A primeira instalação valida os pré-requisitos antes da extração; o áudio é convertido dos VAGs, streams e faixas da própria ISO. A conversão de assets continua dependendo de Python 3.9–3.12, SDK .NET 9, Neversoft Multitool CLI v1.3.4, CMake e Visual Studio 2022 com C++.

### Medição de desempenho de 23/11/2026

| Métrica em ToyStory_Bedroom | Média de 8 amostras |
| --- | ---: |
| FPS lógico | 59,98 (59,82–60,14) |
| VIF1 | 36,9% |
| VU1 | 26,2% |
| GIF/GS | 9,4% |
| Worker GPU ocupado | 72,4% |
| Espera de GPU | 25,5% |

As amostras cobriram os frames 8400–10500. O log confirmou `Loading Scene ToyStory_Bedroom ...... done`, `CAREER_STARTED` e `starting a run`. A comparação controlada com o HUB e as demais fases ainda está pendente.

### Estrutura resumida

```text
src/host/          Runtime do host, memória, interrupções, áudio e vídeo
src/launcher/      Instalação, validação, interface, configurações e logs
native/original/   Gráficos, VIF/GIF, VU, IOP, áudio e entrada
native/            Conversor de assets e testes de colisão
tests/             Testes do runtime e dos drivers gráficos
scripts/           Preparação de dados, compilação e análise
recomp/            Configuração da recompilação
docs/              Documentação técnica e arquitetura
```

Veja [docs/BUILDING.md](docs/BUILDING.md), [docs/arquitetura-nativa.md](docs/arquitetura-nativa.md) e [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

</details>

## Aviso legal

O jogo, seus personagens, marcas e a capa pertencem aos respectivos detentores de direitos. Este é um projeto não oficial, sem vínculo com eles. Não são distribuídos ISO, dados originais, saves ou binários prontos neste repositório.

O projeto contém componentes de terceiros com suas próprias licenças, incluindo GPL-3.0. A revisão final da proveniência e da licença global ainda está pendente.
