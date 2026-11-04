# Diagnóstico de desempenho — 25/10/2026

## 1. Tipo de paralisação

Lentidão por custo de CPU na preparação/renderização do jogo. Não foi observado deadlock nem frame permanentemente congelado nos intervalos medidos: lógica, desenho e troca de quadro avançaram. O usuário confirmou controle Xbox funcionando e entrada na fase.

A janela reapresentou 601 imagens em 10,014 s (60,014/s), incluindo imagens repetidas. Isso não equivale a 60 FPS do jogo. A sessão pausada desenhou cerca de 8,791 quadros/s. No intervalo em que o usuário informou a partida despausada, lógica, desenho e flip avançaram **8.258/s**, sob instrumentação. A amostragem nativa identificou a GameThread (TID 15968) como principal consumidora; o worker gráfico frequentemente esperava trabalho.

## 2. Local exato

- `native/original/vu/generated/vu1/vu1_87bd03b7fadb25f4_1.cpp`, bloco original `VU1InterpreterAccess::fastBlock<0x87BD03B7FADB25F4, 2>`, linha 5 antes da regeneração; faixa VU iniciada em 0x06E8. O mesmo programa VU mantém seu hash após a correção.
- `native/original/vu/ps2_vu1_impl.inl`, `startXgkick`; `src/host/pc_runtime.cpp`, callbacks de GIF e MSCAL em `syncCoreSubsystems`; `src/host/pc_scheduler.cpp`, entrega de IRQ.
- Funções originais contadas: lógica **0x001F9EF8**, desenho **0x001FB1A0**, flip **0x00166B28**. A entrada nativa é filtrada pelo PC original para distinguir retomadas de uma nova execução.

Stack representativa, da chamada original ao trabalho caro: `entry_0015a548` → `PS2Runtime::Store32` → `PS2Memory::writeIORegister/processPendingTransfers/processVIF1Data` → callback MSCAL → `VU1Interpreter::execute/run` → `runImage/runFast` → `fastBlock`. O nome histórico da classe permanece, mas o caminho medido executa código VU AOT gerado.

O mapa foi validado contra o executável da sessão anterior: `.text` de 94.338.048 bytes, SHA-256 `95b64b6b379d6c02013fa56c989b4db42070ccfc01cabfeb81d4c55db6f18d26`, idêntico no binário usado para produzir o mapa. Endereços nativos da versão nova usam `optimized-runtime-symbols.map`.

## 3. Causa raiz comprovada

O compilador MSVC emitiu **C4883** no bloco VU: o tamanho da função suprime otimizações. Isso foi reproduzido em compilação isolada com `/O2 /Ob2 /w14883`. A amostragem encontrou muitas movimentações de valores SSE para temporários com offsets de centenas de KiB na pilha, coerentes com o código gerado sem as otimizações necessárias. [Definição oficial do C4883](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/compiler-warnings-c4800-through-c4999?view=msvc-170).

Havia também consultas `getenv` de diagnóstico por pacote/chamada VU, mesmo sem tracing. Das 240 amostras da GameThread, 196 passaram pela cadeia VU; 59 tiveram como folha consultas de ambiente ligadas a essas flags, mais uma folha de alocação que não foi atribuída a `getenv`.

Não há evidência neste diagnóstico que justifique trocar driver, aumentar RAM ou instalar bibliotecas. Não foram alterados serviços, condições de saída, retornos do jogo ou limites de ciclos para obter progresso.

## 4. Evidências e coleta

Arquivos completos em `C:\Users\vitor\OneDrive\Documentos\ChatGPT\REA\performance-2026-10-25`:

- `active-game-entry-counts-before.log`: contadores originais por 15,015 s, 124 entradas de lógica/desenho/flip; tempo dentro dos handlers do coletor 93 ms, além do custo dos eventos de depuração.
- `live-profile-2.txt`, `.symbolized.txt`, `profile-2-classification.json`: 240 amostras e todas as stacks relevantes.
- `active-profile-before.txt` e `.summary.json`: 60 amostras após o usuário informar que despausou.
- `compiler-probe-before-fast-fmac.log`: C4883 reproduzido; `compiler-probe-blocks32.log`: aviso ausente com blocos menores.
- `generator-reproduction.json`: os dez arquivos originais foram reproduzidos byte por byte usando o gerador e suas flags anteriores, antes de variar somente o tamanho dos blocos.
- `paired-benchmark.json`: cinco pares alternados; hashes de registros, dados VU, pacotes e campos de ciclos iguais em todas as execuções.

Os coletores usam amostragem ou contadores agregados. Breakpoints temporários foram restaurados e o processo foi destacado ao final. Não houve logs por instrução.

## 5. Correção mínima entregue

Regeneração do mesmo programa VU1 com **32 pares por bloco**, em lugar de 128, usando a opção já existente do gerador: **42 blocos normais em lugar de 11**, mesmos 1.313 pares de instruções. As configurações de precisão, flags e normalização foram mantidas. O build foi reconfigurado para os sete arquivos de partes atuais. Os arquivos originais foram arquivados, não descartados.

As flags de diagnóstico passaram a ser lidas uma vez por processo/primeiro uso em `native/original/diagnostic_flags.h`. Foi habilitada a exibição do C4883 no helper VU. **A matemática dos helpers é a original**: as experiências intermediárias com inlining foram revertidas e essa equivalência textual foi verificada automaticamente.

Runtime instalado: `runtime/DisneySkateRuntime.exe`, SHA-256 `f3e7956472023ede5b3d5767a392224ac4287927f09fe4a81a545d4a9d847aa6`. O iniciador principal é o mesmo `Disney's Extreme Skate Adventure.exe`; `--verificar` passou. Backup anterior preservado no diretório de investigação.

## 6. Teste antes/depois

Benchmark com 128 entradas VU capturadas da sessão, repetidas 128 vezes (16.384 execuções), com cinco pares alternados:

| Medida | Referência | Blocos de 32 |
|---|---:|---:|
| Mediana VU/GIF com hashes de estado/dados | 1344.480 ms | 569.216 ms |
| Ganho nesse benchmark | 1,00× | 2.362× |
| Hash dos pacotes | `f917e09bec0c66eb` | idêntico |
| Hash dos estados finais | `15691b46f53ae1d4` | idêntico |
| Hash dos dados VU finais | `9284d054ef1e6bbf` | idêntico |

O benchmark inclui o custo dos hashes, usa trabalhos independentes e **não representa FPS de uma partida completa**. As 1.024 capturas históricas, repetidas oito vezes, também mantiveram todos os hashes e campos de ciclos idênticos.

Passaram os casos imutáveis `native-vu-regression-job222.bin` (sete pacotes, NREG e arredondamento) e `native-vu-regression-job398.bin` (terminador IMAGE2 vazio); os três testes analíticos de cor/profundidade tiveram zero divergências; botões/eixos/pressão e tabela original passaram; IRQ, VBlank e retomada após yield passaram. O caminho GPU com worker produziu imagem idêntica ao caminho GPU direto no replay. A comparação CPU/GPU da captura incompleta é relatada separadamente e não certifica fidelidade visual total.

O runtime otimizado está instalado. Falta medir a partida depois de fechar e reabrir a janela anterior; o processo antigo continua executando o código antigo. Ainda não há FPS de gameplay após a correção certificado.

## 7. Estado final

Controles e entrada em gameplay confirmados pelo usuário antes da atualização de desempenho. Correção compilada, comparada e instalada no iniciador normal. O runtime otimizado está instalado. Falta medir a partida depois de fechar e reabrir a janela anterior; o processo antigo continua executando o código antigo. Ainda não há FPS de gameplay após a correção certificado.

## 8. Próxima investigação

Medir a versão reaberta durante gameplay e amostrar o novo caminho dominante se ainda houver engasgos. Não escolher outra alteração de memória/renderização sem esse perfil. Filmes/IPU e fidelidade de filtro linear permanecem pendências já documentadas, fora da correção de desempenho atual. Nenhum PCSX2 ou biblioteca nova foi usado/instalado.
