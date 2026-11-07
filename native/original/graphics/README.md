# Ponte gráfica do candidato original

`native_memory.cpp` deriva de PS2Recomp e preserva a licença do projeto upstream.
A cópia do código original em `tools/PS2Recomp` fica intacta. O candidato substitui
somente essa unidade pelo CMakeLists.original-aot.txt.

Mudanças verificadas: montagem incremental de GIFtags/payloads por PATH e
conclusão/IRQ de cadeias DMA END com QWC=0. Teste em
`tests/native_gif_stream_test.cpp`; veja a pesquisa em `docs` para evidências.

Isto ainda implementa compatibilidade com memória, registradores e protocolos
PS2. Não constitui uma conversão completa do motor gráfico para APIs PC.
