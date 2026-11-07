# Inicialização IOP original recompilada

Este componente executa C++ x64 gerado dos sete módulos originais do Disney.
Não contém o interpretador R3000A nem chama executeInstruction/iopExecute.
As importações do kernel usam serviços C++ existentes; não há BIOS nem PCSX2.
O contexto de registradores e a memória de dados ainda fazem parte da ponte de
integração. Isso não equivale ao port completo nem comprova áudio ou controles.

O teste carrega os IRX locais somente para relocação e dados. O código executado
foi compilado antecipadamente, e o hash/base devem coincidir antes de iniciar.
Código não convertido ou importação ausente causa erro explícito, sem intérprete.
Ainda falta substituir a entrada IRX por dados preparados no pacote final,
integrar ao EE e validar os protocolos de áudio, controles e saves em jogo.

Teste validado: sete inicializadores residentes (retorno 0/2), sete threads,
seis serviços RPC e um segundo de tempo de serviços. Não é teste de gameplay.

Fontes GPL-3.0 preservadas em COPYING.md:

- PS2Recomp, árvore local e563698: https://github.com/ran-j/PS2Recomp
- Gerador e helpers Tobiichi, commit 471c83180a8df575cff4abd3fae6c043b5096925:
  https://github.com/YYOzcan/Tobiichi-Port

Adaptações Disney: remoção do decoder de instruções do build; helpers de exceção
independentes; memória compatível; seleção de módulos por hash/base; dispatcher
nativo estrito; verificação de retorno; teste dos inicializadores reais.
Nenhum endereço/override específico de God of War foi aplicado ao Disney.
