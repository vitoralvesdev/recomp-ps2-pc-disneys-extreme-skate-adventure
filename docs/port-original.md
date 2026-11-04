# Conversão nativa do jogo original: evidência e trabalho restante

## Objetivo aceito

Executar o mesmo Disney da ISO de PS2 no Windows, preservando a lógica original,
com código x64 e APIs PC. Sem PCSX2, intérpretes MIPS/VU/IOP ou execução de IRX.
O disco é entrada durante a preparação dos dados; não basta renomear a ISO ou
compilar a imagem como se fosse código fonte Windows.

## Correção de direção

A aplicação de HUB/Timon com movimento aproximado não executa a lógica original.
Foi um desvio do objetivo e foi retirada do atalho e do build padrão. Seus
conversores de recursos podem servir ao port; sua simulação não é a solução.
Não se considera o jogo jogável nem o port concluído.

## O que foi confirmado

- Identidade: executável USA SLUS_206.07, SHA256
  2FE6CE3A08AADE170BE27EA221137C96AA15083A408D61856F605F1A40F321FC.
- Ghidra existente: DisneySkate / r5900:LE:32:default, 9.390 funções definidas.
  Essa contagem não comprova cobertura ou fidelidade integral.
- O legado chama funções EE já compiladas em C++, preservando boa parte da
  lógica original. Ainda depende de contexto de registradores, RAM e MMIO.
- VU0 é realmente interpretado: chamadas `executeVU0Microprogram` e `m_vu0.execute`.
  VU1: callbacks MSCAL/MSCNT chamam `execute/resume`.
- IOP é realmente interpretado: IopSubsystem prioriza emulador para IRX/RPC e
  avança ciclos; o log anterior registra cargas físicas de SIO2MAN, PADMAN,
  LIBSD, SDRDRV, MCMAN, MCSERV e EZPCM. Não é só biblioteca ligada sem uso.
- O original espera conclusão gráfica em FUN_0015ca00 -> FUN_0015ca40,
  global 0x346b34. O snapshot antigo mostra valor não zero, então esse quadro
  não foi concluído. Forçar retorno/sucesso não seria uma implementação gráfica.
- O aviso `WARNING: ScriptAllowPause is STUBBED` já existe no próprio ELF,
  string 0x320b68, função 0x238280. Ele não comprova um stub introduzido pelo port.
  Evidência: analysis/script-allow-pause-warning-xrefs.txt.

## Imagem corrompida: limite do diagnóstico

Um teste isolado do backend, com a imagem RGB555 original 640x448, verificou
286.720 pixels: upload CT16 em blocos IMAGE de 16 bytes, desenho de sprite
texturizado CT16->CT32 e apresentação passaram sem erros. Isso não reproduz o
estado gráfico real do jogo e não identifica a causa da captura do usuário.

O arquivo render-wait.ram contém RAM EE e não contém VRAM/registros GS completos.
Ainda faltam o estado BITBLTBUF/TRXPOS/TRXREG/TEX0/FRAME/DISPLAY/DISPFB e os pacotes
reais do quadro corrompido para fechar sua causa. Nenhuma correção gráfica foi
alegada com base nesse teste. O jogo legado não foi executado nesta investigação.

## Código original começando a ser recuperado para PC

`native/original/occlusion.cpp` porta o caminho escalar existente em
FUN_001624c0 (0x16254c..0x1625f8). O original oferece esse caminho como alternativa
a um microprograma VU0; recuperar o caminho escalar evita inventar nova regra.

Contrato recuperado: registros de 0x70 bytes, cinco planos a partir de +0x10,
contador em +0x60. Um objeto é considerado oculto quando passa pelos cinco testes
estritos contra -raio; o primeiro registro correspondente atualiza seu contador
com soma módulo 2^32. A implementação usa estruturas C++ e std::span, sem CPU,
RAM ou instruções de console em tempo de execução.

Validado: lista vazia, cinco planos, separação, igualdade/tangência, primeiro
registro correspondente, contador com overflow e ordem das operações float.
Compilação MSVC /fp:strict. Resultado: analysis/original-core-test.log.
A equivalência integral do FPU do console em casos excepcionais e a integração
com o renderizador ainda não foram validadas. Não é um executável de jogo.

Três chamadas VCALLMS estáticas identificadas no código mapeado:

| Endereço EE | Rotina | Entrada VU0 em bytes |
|---|---|---|
| 0x109f5c | FUN_00109f48 | 0x7e8 |
| 0x162484 | FUN_001616f8 | 0x30 |
| 0x1624fc | FUN_001624c0 | 0x458 |

O número abrange chamadas estáticas decodificadas em funções mapeadas; não prova
que todos os caminhos dinâmicos foram encontrados.
Evidência: analysis/original-vu0-boundaries.txt e fontes gerados correspondentes.

## Fronteiras restantes, preservando o jogo

| Parte | Evidência existente | Falta implementar/validar |
|---|---|---|
| Gráficos | Carregador 0x1690b8, materiais 0x15d0a0, geometria 0x15d698 | Contratos de objetos, skinning, iluminação, clipping e submissão em API PC, antes de DMA/VIF/VU/GIF |
| VU0 | Três entradas estáticas e alternativa escalar para o teste de cinco planos | Recuperar cálculos dos outros programas e integrar substituições fiéis |
| Áudio | Cargas IRX e protocolos SDK/RPC observados | Som/streaming nativos equivalentes, incluindo EZPCM/SDRDRV |
| Serviços | Interceptações SDK existentes de pad/arquivos/CD | Retirar execução IRX e hardware virtual, validar saves/tempo/tarefas |
| Lógica | EE recompilado e parser de scripts recuperado | Completar cobertura e confirmar menus, estados, fases, objetivos e física original integrados |

REA/Ghidra ajudam a localizar e recuperar essas rotinas. As ferramentas
instaladas não fazem a conversão completa e equivalente automaticamente.
O impedimento atual é implementação e validação dessas partes; não uma DLL
faltando ou a idade do jogo. Ainda não existe um port original completo.
## Microprograma original de colisão identificado

O upload em FUN_00160d28 foi provado pelo Ghidra: DMA0/VIF0 transfere 0x980 bytes
de 0x2f63a0, contendo dois MPG que fornecem 0x970 bytes de código VU0.
A entrada 0x7e8 tem 49 pares de instruções e implementa interseção unilateral de
segmento/triângulo. Foram recuperados seus cinco vetores de entrada, retorno,
parâmetro t e constantes/limites originais por análise offline. Não foi executado
interpretador nem foi integrada uma versão PC ainda sem equivalência validada.
Evidência: analysis/vu0-upload-xrefs.txt e analysis/vu0-micro7e8-analysis.md.
Este é material do código original, não outra simulação de skate.