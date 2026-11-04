> Documento histórico. Os executáveis e atalhos desta versão foram removidos na limpeza. Para abrir o candidato atual, use `Disney's Extreme Skate Adventure.exe` na pasta principal.

# Disney's Extreme Skate Adventure — port experimental para Windows x64

**Ainda não é jogável.** O executável abre, inicializa vídeo e áudio, executa o
jogo e carrega a cena skateshop, mas a imagem continua preta. A inicialização
já ultrapassa a preparação dos modelos e chega ao gameflow nos logs, mas ainda aguarda um sinal gráfico posterior.
Não há validação de menu, fase, desempenho ou comandos durante jogabilidade.

## Abrir

- Executável: `dist/Release/DisneySkatePort.exe`.
- Atalho: `Jogar.cmd`.
- Os arquivos em `original/extracted` precisam permanecer no projeto.
- Escape fecha a janela. Esta versão serve para investigação da inicialização.

## Controles

Teclado e controle podem ser usados simultaneamente. O backend busca o primeiro
controle entre quatro slots e aplica zona morta de 18% aos analógicos.
Espaço/X corresponde a Cruz; Z a Quadrado; C a Círculo; V a Triângulo;
WASD/setas ao direcional; Enter a Start. Layout Xbox: A/B/X/Y nas mesmas posições.
O mapeamento completo e o suporte a controles universais estão em `docs/controles.md`.
O backend passou em testes com entrada simulada. Nenhum controle físico foi
identificado no teste de inicialização; ainda é necessário validar o modelo do usuário.

## Resultado técnico

- ELF USA `SLUS_206.07` extraído e analisado no Ghidra 12.1.4/R5900.
- Runtime, código traduzido, FFmpeg, áudio e gráficos ligados ao executável x64.
- Argumentos de boot fornecidos no syscall SetupThread, após a limpeza de BSS.
  Isso impede a seleção incorreta do modo TOOL e do pool acima dos 32 MB.
- APIs identificadas de arquivos, CD, Pad e IOP ligadas ao runtime.
- Módulos SIO2MAN, PADMAN, LIBSD, SDRDRV, MCMAN, MCSERV e EZPCM carregados.
  O resultado de inicialização de cada um está nos logs; carregar não confirma
  funcionamento completo de todos os serviços.
- WADs, vídeos, scripts, texturas, esqueletos e dados da cena skateshop encontrados.
- 483 corpos de funções complementares e 967 entradas restaurados da tradução
  inicial e de referências do ELF, sem substituir entradas já presentes.
- Dez destinos únicos de uma tabela de saltos foram recuperados do ELF.
- Corrigida a distinção entre checkpoint e retorno de função, com regressão.
- Áreas RPC/TLS/boot e pilhas de callbacks realocadas para a região entre o espelho do kernel e o início do ELF, evitando os heaps próprios do jogo.

- Corrupção do arquivo comprimido do Timon corrigida ao remover a sobreposição com o pool RPC.
- Sinais GS SIGNAL/FINISH encaminhados ao handler INTC_GS original.
- Profundidade das chamadas nativas limitada com retomada pelo dispatcher; pilha Windows de 8 MiB.
- Testes de fluxo, máscara/entrega/ack de interrupção GS e renderização OpenGL isolada passaram.

## Bloqueio atual

O laço de geometria anterior foi superado. O jogo carrega os personagens,
inicia o gameflow, vincula o skatista ao controle e carrega animações nos logs.
Ainda permanece em uma espera gráfica em `0x0015CA10/0x0015CA18`, com
`DAT_00346B34=0x00C275D0` no estado capturado. A tela segue preta nos testes.
Menu, fase, controles em gameplay, áudio do jogo e saves continuam sem validação.

O driver NVIDIA 616.56 passou no teste isolado de OpenGL. O relatório e a opção
de atualização compatível estão em `analysis/driver-assessment.md`. Não foi
alterado o driver do computador.

## Reproduzir

1. `scripts/Compilar-Port.ps1` prepara as adaptações e compila o port e a regressão.
2. `scripts/Testar-Controles.cmd` valida o backend com entradas simuladas.
3. `scripts/Testar-Inicializacao.ps1 -Seconds 45` executa o diagnóstico com limite.
4. `analysis/boot-stdout.log`, `boot-stderr.log` e `boot-result.json` registram o teste.

Ferramentas portáteis ficam em tools. Dependências de compilação usam o caminho
mais curto `C:/Users/vitor/OneDrive/Documentos/ChatGPT/REA/deps-ps2` para contornar
limites de caminhos do Windows. Compilador: MSVC 2022 x64; C++20; AVX2.
Hardware observado: i5-11400, RTX 3060, aproximadamente 16 GB RAM.
Base: PS2Recomp commit e5636981a1c9de368f6b71e819c3dc188dfedbf4.
Os fontes complementares gerados podem ser recriados pelos scripts.

