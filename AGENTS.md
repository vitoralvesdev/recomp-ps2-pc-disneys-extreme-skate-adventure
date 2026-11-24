# AGENTS.md — Disney's Extreme Skate Adventure (PS2 → Windows Static Recomp)

## Contexto do Projeto

- **Objetivo:** Recompilação estática / port de **Disney's Extreme Skate Adventure** (PlayStation 2, Emotion Engine / MIPS R5900 + VU0/VU1 + GS) para **Windows (x86/x64, C/C++, CMake)**.
- **Repositório:** `vitoralvesdev/recomp-ps2-pc-disneys-extreme-skate-adventure`
- **Ambiente de trabalho:** Windows 11, VS Code, CMake, compiladores C/C++ nativos e **Ghidra** instalado localmente para análise estática do binário PS2 (ELF / módulos / símbolos / funções / estruturas).

## Compatibilidade com Agentes e Ferramentas de IA

- Este arquivo foi redigido de forma agnóstica a fornecedor ou modelo para funcionar em diferentes IAs e agentes de código (ex.: Cursor, Cline, Qoder, GitHub Copilot, Claude Code, Codex, Gemini CLI, Windsurf ou execuções manuais via chat/terminal).
- Siga estas regras independentemente de o agente ler `AGENTS.md` automaticamente ou recebê-lo colado no contexto:
  - Respeite rigorosamente tudo o que já foi observado no repositório, nos arquivos abertos, no terminal ou no Ghidra.
  - Quando uma ferramenta (como terminal, busca de arquivos ou Ghidra) não estiver disponível no ambiente do agente, não simule saídas nem invente resultados de execução; informe exatamente qual comando ou verificação ficou pendente.
  - Em ambientes multi-agente ou sessões longas, releia o estado atual do arquivo antes de editar e não reverta alterações de outros trechos fora do escopo pedido.

## Ferramentas e Fluxo de Engenharia Reversa Disponíveis

- **Ghidra:** Disponível na máquina local para inspecionar funções MIPS R5900, referências cruzadas (XREFs), constantes, tabelas de ponteiros, estruturas de dados, rotinas de VU/GS e chamadas de sistema/runtime do PS2.
- Quando faltar contexto de endereço, assinatura de função, registradores, convenção de chamada MIPS ou layout de _struct_ do binário original:
  - **Não invente** endereços virtuais (`0x...`), offsets de estrutura ou comportamentos de hardware/jogo.
  - Indique exatamente o que deve ser verificado ou exportado no **Ghidra** (nome/endereço da função, XREFs, decompilação C do Ghidra, offsets de struct ou constantes).

## Estrutura e Áreas Críticas do Projeto

- `README.md`: documentação principal do projeto; deve refletir com fidelidade o estado real e as alterações aplicadas.
- `recomp/` e `runtime/`: infraestrutura de recompilação estática e runtime em Windows.
- `native/` (incluindo `native/original/vu/...`): implementações nativas e rotinas ligadas a VU/runtime do PS2.
- `tests/` (ex.: `runtime_flow_test.cpp`, `test_irq_atomic.inl`, `test_native_vblank.inl`): testes de fluxo do runtime, atomicidade/IRQ e temporização/vblank.
- `CMakeLists.txt`, `CMakeLists.legacy.txt`, `CMakeLists.original-aot.txt`: configuração de build e alvos.

## Regras de Trabalho e Edição de Código

1. **Edições cirúrgicas e verificáveis:** Altere apenas o arquivo e o trecho necessários para resolver o problema pedido. Preserve convenções de estilo, tipos inteiros de largura fixa (`uint8_t`, `int32_t`, `uint64_t`, etc.) e portabilidade Windows.
2. **Sem alucinação de símbolos ou hardware:** Se um símbolo, função recompilada, registrador GS/VU/VIF/GIF/DMAC ou endereço do ELF do PS2 não estiver visível no contexto fornecido, deixe claro a suposição ou peça o trecho correspondente do código/Ghidra.
3. **Cuidado com matemática e precisão de PS2:** Atenção redobrada a overflow/underflow de 32/64 bits, saturação, alinhamento de memória, endianness (little-endian no PS2/x86, mas atenção a operações SIMD/VU de 128 bits), estados globais e sincronização entre threads/vblank/IRQ.
4. **Atualização contínua e fiel do `README.md`:** Sempre que houver alterações relevantes no código, build, testes, estrutura de pastas, fluxo de recompilação ou status/funcionalidades do port, atualize também o `README.md`:
   - Siga estritamente o estilo, o tom, a estrutura de seções e o padrão de formatação já existentes no `README.md` atual.
   - Registre apenas o que foi de fato alterado, verificado ou observado nos arquivos/diffs/testes/Ghidra — **nunca** alucine recursos implementados, benchmarks, percentuais de progresso, compatibilidade não testada, caminhos inexistentes ou símbolos não confirmados.
   - Se o `README.md` não estiver presente no contexto ou exigir confirmação de um comportamento não verificado, mantenha o registro restrito aos fatos comprovados e indique o que precisa ser checado.
5. **Formato de resposta ao editar código:**
   - Explique brevemente a causa ou o ajuste necessário.
   - Forneça o código completo e pronto para colar/substituir do trecho ou arquivo alterado (incluindo atualizações no `README.md` quando aplicável), indicando o caminho exato do arquivo e os símbolos/funções/seções afetados.
   - Liste o que precisa ser conferido no build (CMake/compilador/testes), na documentação (`README.md`) ou no Ghidra após aplicar a mudança.
