# Correções do menu e da renderização — 24/10/2026

## 1. Tipo de parada confirmado

Havia uma espera ativa na thread do jogo: a lógica e a entrada paravam enquanto janela e VBlank continuavam. Depois dessa correção, a aparente falta de resposta do menu tinha outra causa: os botões estavam trocados. Não foi comprovada falta de RAM, DLL ou defeito no driver.

## 2. Local exato

| Falha | Código e endereço |
|---|---|
| IRQ intercalada antes de terminar | `src/host/pc_scheduler.cpp`; handlers PS2 0x15A4C0 e 0x15A690; espera 0x15CA00/0x15CA40 lendo 0x346B34 |
| Bytes de controle invertidos duas vezes | `src/host/pc_pad.cpp`, `native/original/input/native_pad_services.cpp`; leitura original 0x112770 e tabela 0x2FCAE8 usada por 0x112DA0 |
| Imagem antiga não apagada | `native/original/graphics/gpu/gs_hw_backend.cpp`, `mirrorDepthClearToColor`; FBP=0 / ZBP=70, largura 640, limpeza 224+224 linhas |
| Metade da geometria escondida | Mesmo backend, `attachDepth`; cena FBP=0 e apresentação FBP=140 compartilham ZBP=210, Z24, largura 640 |

As stacks nativas e os relatórios Ghidra completos foram preservados junto das evidências externas indicadas abaixo. A stack de espera original passava pelo despacho recompilado, 0x15CA00/0x15CA40 e a consulta de conclusão gráfica; apresentação e áudio continuavam em suas threads.

## 3. Causas comprovadas

A agenda trocava de thread com uma invocação de IRQ ainda ativa. O handler GIF leu 0x362614=0; o handler GS intercalado leu 0x362610=0 e colocou 0x362614=1; ao retomar, GIF colocou 0x362610=1. A fila nunca concluía.

O pacote libpad tem o campo de 16 bits em ordem little-endian. O código original faz sua conversão interna. A ponte já invertia os bytes antes disso: Cross (0x4000 no host) virava Down na tabela do jogo. O teste de confirmação descia o menu em vez de escolher Play Game. Também foram corrigidos os campos de pressão L1/R1/L2/R2.

No gráfico, o jogo usa a mesma memória para visões de cor e profundidade. O backend não refletia limpezas de uma visão na outra e criava profundidades distintas para buffers de cor que usam o mesmo ZBP. O passe de apresentação escreve Z=0 em ZBP=210 para preparar a cena seguinte; antes, essa escrita só limpava a profundidade da apresentação.

## 4. Evidências e testes

| Verificação | Antes | Depois |
|---|---|---|
| Preempção dentro da IRQ | teste falhava | passa; pedido de troca continua pendente até retorno |
| Limpeza Z32 sobre cor | 5952 canais diferentes; pixel antigo 200,30,20 | zero diferenças; pixel esperado 80,96,112 |
| Limpeza CT32/Z24 sobre profundidade | geometria era ocultada pela profundidade anterior | teste analítico/CPU e GPU concordam |
| Mesmo ZBP em dois buffers de cor | 11532 canais diferentes | zero diferenças |
| Tabela original de entrada e pressão | botões em slots errados | 12 botões nos slots originais corretos |
| Pacote de entrada completo | ordem invertida no adaptador | 16 bits, eixos e liberação passam |

A captura do menu contém 876551 operações até VBlank 1800. A captura que alcança a fase contém 4180997 operações até VBlank 4209; o limite de 2 GiB encerrou a gravação em uma fronteira de registro. Reproduzir os mesmos comandos antes/depois removeu aviso antigo, pose duplicada e metade cinza, sem alterar a lógica do jogo.

O contrato gráfico geral mantém a falha **preexistente** de filtro linear: 2304 pixels, erro máximo de canal 24. Sprite simples, textura pontual, textura indexada e alfa continuam passando. Não há certificação de fidelidade gráfica completa.

## 5. Correção aplicada

Adiar a troca de thread enquanto uma IRQ original está ativa; preservar a ordem real do pacote de controle; refletir as limpezas constantes de páginas completas entre cor e profundidade; identificar a superfície de profundidade por ZBP, largura e formato, compartilhando-a entre os buffers de cor.

Não foram alteradas as condições de saída originais, inventados retornos de sucesso ou puladas inicializações. Nenhuma biblioteca foi instalada. PCSX2 não é usado para executar o jogo.

## 6. Antes/depois na execução

Antes: menu mantinha pixels de telas anteriores; confirmar podia mover a seleção para baixo; lógica podia ficar na espera gráfica.

Depois: comandos internos controlados navegaram até Play Game, carregaram `ToyStory_Bedroom`, registraram `CAREER_STARTED` e `starting a run`. Na sessão de 120 segundos, direção e ação chegaram à função original; Woody virou e a câmera mudou. A lógica continuou atualizando. A captura da fase com o backend final mostra Woody e o cenário completos.

Os testes de entrada descritos são automatizados. Eles não substituem a validação humana do controle USB específico.

## 7. Estado final e como abrir

Use **`Disney's Extreme Skate Adventure.exe` na pasta principal**. Ele inicia `runtime/DisneySkateRuntime.exe` e limpa as flags de diagnóstico. Não procure um executável em `dist` nem use os binários de investigação.

Teclado: setas/WASD, X ou espaço para confirmar/ação, C para voltar/círculo, Enter para Start. Controle Xbox: A corresponde ao X do PS2, B ao círculo, X ao quadrado, Y ao triângulo.

Menu, entrada na fase e resposta a comandos foram confirmados em testes controlados. Persistem problemas de desempenho, áudio/vídeos e fidelidade gráfica; não é uma versão finalizada.

## 8. Próxima investigação localizada

Vídeo do menu ainda aborta no quadro zero. Ghidra: 0x118A20 → 0x2D0588 → leitura de bits em 0x2E1818/0x2E14D0/0x2E15D8/0x2E12A8. Esse caminho usa IPU_CMD/CTRL; o adaptador atual apenas armazena os registradores. O decoder FFmpeg existente ainda não está ligado a esses endereços. A integração exige validar ABI, callbacks, término e cancelamento; o watchdog original foi preservado.

Evidências completas: `C:/Users/vitor/OneDrive/Documentos/ChatGPT/REA/hang-investigation-2026-10-24/`, especialmente `PROGRESS.md`, testes `*-final.log`, `shared-depth-before.log`, `shared-depth-after.log`, `run-11.*.log`, `gameplay-before.png` e `gameplay-after-shared-depth.png`.

Referência primária para o pacote de controle: [libpad.h do PS2SDK](https://github.com/ps2dev/ps2sdk/blob/master/ee/rpc/pad/include/libpad.h). O oráculo específico do jogo foi a tabela original verificada no Ghidra e na RAM capturada.
