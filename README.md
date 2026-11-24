# Disney's Extreme Skate Adventure — versão para Windows

![Capa de Disney's Extreme Skate Adventure para PlayStation 2](docs/capa-jogo.png)

> **Estado: em desenvolvimento — atualizado em 24/11/2026.**
> Nos testes locais, o projeto já abre o menu, permite escolher personagem e chega ao HUB e a uma fase de Toy Story. Ainda não é um port completo: algumas fases podem travar, o carregamento pode demorar e vários recursos ainda precisam de validação.

## O que é este projeto?

É um projeto não oficial da comunidade para transformar o código de **Disney's Extreme Skate Adventure**, de PlayStation 2, em um programa para **Windows x64**.

Não é um emulador como o PCSX2: o código do jogo é convertido antecipadamente para C++ e os serviços do console são adaptados para o PC. O projeto ainda depende de uma cópia legal da versão americana do jogo.

**O repositório não inclui um executável pronto, o programa principal que executa o jogo nem os dados do jogo.**

## O que já foi corrigido ou funciona nos testes locais

- **Menus e início do jogo:** menu, seleção de personagem, HUB e entrada em uma fase de Toy Story foram registrados em sessões locais.
- **Desempenho:** correções no VU0 melhoraram o fluxo de imagens. Em `ToyStory_Bedroom`, a média foi de **59,98 quadros por segundo (FPS)** em 8 amostras, perto do alvo de 60 FPS. Isso ainda precisa ser confirmado nas outras fases.
- **Launcher experimental:** o programa de instalação exige o `BOOT2` exato e o SHA-256 de referência do ELF americano `SLUS_206.07`, extrai os arquivos com progresso, compara extrações por hash, configura o jogo e mostra logs. O pacote recebe um manifesto SHA-256 verificado antes de liberar o jogo. A verificação manual de atualizações mostra carregamento e um modal quando não há versão nova.
- **Atualizações mais seguras:** o launcher só aceita HTTPS, baixa apenas o instalador oficial esperado, confere o tamanho e a impressão digital SHA-256 e exige uma assinatura digital confiável do Windows antes de executar.
- **Cartão de memória (memory card):** caminhos com drive, caminhos absolutos do Windows, caminhos que tentam sair da pasta (`..`) ou links externos agora são bloqueados. Leituras e escritas também verificam o intervalo inteiro de memória.
- **Controles:** teclado e controle têm mapeamento inicial, com posições baseadas em um controle tipo Xbox.
- **Gráficos e áudio:** menus e cenas são exibidos; testes básicos de textura passaram; sons e músicas começam a tocar nos testes. A fidelidade visual e o áudio completo ainda não estão validados.
- **Build do launcher:** compila separadamente, sem ISO ou código gerado do jogo. A última execução local passou em **100 verificações, sem falhas**, incluindo a validação da ISO americana e da extração existente.
- **Áudio pela ISO:** o instalador não exige pacotes externos; converte VAGs/streams e recupera as músicas de `MUSIC.WAD` da própria imagem selecionada. Em 24/11/2026, um teste isolado produziu **1.285 entradas** — 393 efeitos, 855 streams e 37 músicas — sem erros de conversão, arquivos ausentes ou músicas pendentes.
- **Handlers originais copiados:** o runtime registra os blocos do kernel copiados para `0x80075000` e `0x80076000`, com aliases físicos/KSEG0 e retomada nos pontos internos. Os testes confirmam a tabela original de syscalls, o despacho dos handlers e a chamada aninhada `0x75158 → 0x3042ac`.
- **Falhas de runtime explícitas:** syscalls desconhecidos retornam erro e funções EE ausentes param por padrão; DMA GIF/VIF direto ou em chain com endereço inválido não sinaliza conclusão nem IRQ.
- **Dados de colisão:** o preparador já converte os arquivos de colisão COL v8 das fases extraídas. Isso ainda não comprova que rampas e objetos estejam corretos durante o jogo.

## O que ainda precisa melhorar

1. **Validação completa do jogo:** testar todas as fases, personagens, ações, salvamentos e telas do início ao fim.
2. **Carregamento:** algumas fases podem parar. Foi registrada uma espera de cerca de 15 segundos ao entrar em uma fase, e a causa ainda não foi isolada.
3. **Desempenho nas demais fases:** repetir a mesma medição controlada no HUB e nas outras etapas.
4. **Colisão:** confirmar rampas, objetos e intersecções no jogo recompilado, especialmente nas fases de Toy Story.
5. **Áudio:** validar a mixagem em partidas completas. A varredura atual não encontrou WAVs vazios ou ausentes no catálogo, mas os logs ainda registram `couldn't load` para sons que existem no catálogo; a causa dessa divergência não foi isolada.
6. **Vídeos:** o decodificador está integrado, mas as cutscenes `.PSS` ainda não foram testadas no fluxo recente.
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
