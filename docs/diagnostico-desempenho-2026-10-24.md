# Diagnóstico de desempenho — 24/10/2026

O candidato abre o menu original. Ainda não está aprovado como port completo
jogável. A investigação encontrou dois gargalos concretos de implementação;
não encontrou evidência de que instalar outra biblioteca ou trocar o driver
resolva esses gargalos.

## 1. Pixels desenhados na CPU

O caminho anterior usava `GSCpuBackend::DrawTriangle/DrawSprite`: cada pixel,
amostra de textura, teste de profundidade e mistura era calculado na CPU.
A RTX recebia principalmente a imagem pronta para apresentação.

O teste offline usa 1.024 entradas VU originais retidas, repetidas duas vezes.
Não inicializa o jogo. Os mesmos 2.048 trabalhos produzem 37.024 primitivas,
3.732 pacotes e 3.962.464 bytes GIF. No diagnóstico inicial, o desenho consumiu
1.281,670 ms de 1.450,333 ms, cerca de 88% dessa carga gráfica isolada.
Sem desenhar pixels, mantendo os comandos, o tempo foi 163,533 ms.

Foi integrado um renderizador OpenGL de render targets nativos, com base no
trabalho de God of War mantido no Tobiichi-Port, revisão
`471c83180a8df575cff4abd3fae6c043b5096925`. A fila Windows possui limites,
copia os uploads e mantém criação, uso e destruição do contexto WGL na mesma
thread. O teste espera a conclusão da GPU antes de terminar a medição.
Nenhum GS do PCSX2 foi importado. Créditos e GPL-3.0 estão preservados em
`native/original/graphics/gpu/PROVENANCE.md` e `LICENSE.GPL-3.0.txt`.

Resultados finais estão em `analysis/native-performance-final-benchmark.log`.

| Caminho, nas mesmas entradas | Tempo total |
|---|---:|
| VU AOT + desenho CPU | 1245.921 ms |
| VU AOT + comandos, sem desenho | 120.070 ms |
| VU AOT + GPU direta | 244.089 ms |
| VU AOT + fila GPU | 165.777 ms |

A razão CPU/fila GPU foi **7.52 vezes nessa carga isolada**.

A tabela em `analysis/native-performance-results.json` é extraída dessa mesma
execução. São tempos da carga capturada, **não FPS do jogo**. A execução EE,
scripts, arquivos, áudio e sincronização do jogo inteiro não estão nessa carga.

Todas as modalidades preservaram bytes GIF, número de pacotes, ciclos e
estados finais VU. Hash GIF: `b33c6b06bf13cf75`. O caminho CPU também preservou
o hash da imagem completa de VRAM de referência: `97da30723242415d`.
Esses hashes são FNV-1a usados em regressão, não assinaturas de segurança.
O teste exercita alterações de microcódigo e reutilização da mesma imagem.

O cache de entrada AOT deixou de recalcular um hash de 16 KiB em toda chamada
VU. Escritas CPU/VIF invalidam a seleção por geração; programas desconhecidos
continuam sendo recusados, sem fallback para interpretação.

## 2. Relógio da tela dependente de ciclos estimados

Ghidra identificou no `SLUS_206.07` original:

* `0x274ea0`: espera o contador mudar e cede a execução.
* `0x274d10`: lê o contador de 64 bits em `0x300c20`.
* Referências em `0x274a94/0x274a9c`: leitura/escrita desse contador no callback
  original de tela, entrada recompilada `0x274a80`.
* `0x15ca00/0x15ca40`: outra espera consulta `0x346b34`, a fila gráfica ocupada.
  Não é uma espera por alocação de RAM.

No escalonador, VBlank exigia simultaneamente prazo de PC e quantidade
estimada de ciclos. Quando o código ou o desenho no PC demorava mais do que
essa estimativa, o contador de tela avançava lentamente e o jogo esperava.

`DISNEY_NATIVE_VBLANK=1` permite que somente os eventos de tela obedeçam ao
relógio monotônico do PC, em pontos seguros de execução. Os callbacks originais
continuam sendo executados. Ciclos de DMA, timers EE e IOP não são acelerados.
O experimento anterior que acelerava o relógio inteiro foi removido, pois
provocava bloqueio de fila gráfica.

A regressão reproduz trabalho real de 180 ms com apenas pequenos avanços
virtuais. A referência gera zero VBlanks; o caminho corrigido gera cerca de
11, entregando os callbacks originais. Também verifica que os ciclos de
dispositivos continuam baixos. É uma prova isolada de agendamento, não uma
aprovação de 60 quadros únicos de gameplay por segundo.

Relatórios: `analysis/native-performance-*.txt`, `native-vblank-reference.log`
e `native-vblank-regression.log`. REA registrou a identidade PE x64 do teste;
a análise r5900 original usou o projeto Ghidra existente, em modo somente leitura.

## O que a comparação com outros ports mostrou

* [KillzoneRecomp — findings](https://github.com/drakolordx7/KillzoneRecomp/blob/master/docs/findings.md)
  documenta gargalos de sincronização, IOP, apresentação e diferença entre
  frequência da janela e progresso real. Seu GS utiliza código PCSX2; esse
  backend não foi adotado aqui.
* [God of War — native port](https://github.com/Waffle565/God-Of-War-Native-Port/blob/main/docs/native-port.md)
  usa desenho nativo na GPU e adaptação dos serviços de plataforma. Essa é a
  referência arquitetural do backend OpenGL qualificado aqui.
* [God of War — desempenho](https://github.com/KIexster/god-of-war-recomp/blob/main/docs/RENDIMIENTO.md)
  descreve medições por etapas e reexecução de comandos retidos. Foi adotado o
  método de comparação offline, evitando reinicializações repetidas do jogo.

Não foi encontrado um patch universal de driver, memória ou biblioteca que
converta esses problemas particulares do Disney em uma solução pronta.

## Validação e limites da entrega

Foi feita **uma única execução de integração GPU**, limitada a 90 segundos,
depois das alterações concretas. O menu original apareceu e animou. No menu,
foram observadas aproximadamente 6,4–6,9 atualizações de sincronização por
segundo, contra cerca de 2,5–3 nas capturas CPU anteriores. São cargas de
execuções diferentes e contadores de sincronização, não um benchmark controlado
de gameplay. Portanto, o ganho offline não foi apresentado como ganho de FPS.

Essa execução ocorreu antes da alteração final do relógio e do cache VU.
O jogo **não foi reiniciado** depois: essas mudanças passaram em regressões
isoladas, mas sua combinação no jogo completo permanece não validada.

Regressões preservadas: framing GIF, quatro uploads com comparação dos
4 MiB de VRAM, conclusão de DMA vazio, trabalhos VU 222/398, ordem dos botões,
apresentação estável e serviço de save com escrever/fechar/resetar/ler em
diretório temporário. O save real do usuário não foi alterado nesses testes.

Pendências observadas:

* O teste sintético de filtro bilinear diverge da referência CPU: 2.304 pixels,
  erro máximo de 24 por canal nesse caso. Sprites, textura sem filtro, paleta
  PSMT8 e FBA/DATE passaram nos respectivos testes isolados.
* A execução GPU mostrou resíduos de sobreposições. As entradas históricas
  offline não contêm um estado completo de quadro nem todos os uploads PATH3;
  diferença de imagem nesse replay não é prova de equivalência visual.
* Áudio, persistência completa de carreira, controle físico e jogabilidade
  nas fases não foram certificados. O contador da janela a 60 não comprova isso.
* Permanecem pontes de compatibilidade de registradores/memória e leitura de
  ELF/IRX como dados. Não é ainda uma reescrita completa do motor em APIs PC.

Para abrir a combinação atual: `Disney's Extreme Skate Adventure.exe`, na pasta principal (o antigo lançador CMD foi substituído).
É um lançador experimental, não a declaração de que o port está concluído.
O caminho CPU existente e o serviço de save foram preservados.
Para medir sem abrir o jogo:
`scripts/Medir-Desempenho-Nativo-Offline.ps1 -CompararGPU`.
