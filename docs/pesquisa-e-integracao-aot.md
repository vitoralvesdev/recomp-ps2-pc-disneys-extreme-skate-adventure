# Pesquisa aplicada ao código original

Não existe ainda jogo original jogável. O laboratório HUB/Timon continua excluído
do port. Os componentes abaixo preservam código original em vez de inventar física.

## Fontes primárias inspecionadas

- [Urban Reign](https://github.com/thiagordsfx/urban-reign-recomp), commit
  20b30fd731b153110bd1b60db61bc421d42586c4: gerador de VU0/VU1 para C++ e
  registro de programas por hash. O upstream mantém caminhos de interpretação;
  eles não podem servir de fallback no candidato Disney.
- [Tobiichi / God of War](https://github.com/YYOzcan/Tobiichi-Port), commit
  471c83180a8df575cff4abd3fae6c043b5096925: gerador IOP AOT de módulos relocados.
  Foram aproveitados o gerador e helpers, sem overrides/endereço de outro jogo.
  O runtime upstream também contém fallback interpretado; foi retirado da variante
  Disney. As licenças GPL-3.0 acompanham os fontes incorporados.
- [OpenGOAL](https://opengoal.dev/docs/faq/): recuperação específica de Jak/GOAL,
  não é um conversor aplicável diretamente ao motor Neversoft do Disney.

Não foi estabelecido que esses autores usaram REA. Aqui REA/Ghidra ajudam na
análise; a compilação exige ferramentas e implementação adicionais.

## Implementação já verificada

`native/original/iop` liga C++ dos sete IRX Disney à implementação de serviços PC.
O fonte do interpretador R3000A não é incluído. Hash e base são verificados
antes da chamada do código compilado; código ausente e importação desconhecida
produzem erro, sem interpretar a instrução original.

`scripts/Testar-Inicializacao-Original.ps1` recompila e executa os inicializadores:
SIO2MAN, PADMAN, LIBSD, SDRDRV, MCMAN, MCSERV e EZPCM. Todos retornaram como
residentes, criaram sete threads e seis RPC, totalizando 2018 instruções compiladas
no teste de inicialização/serviços. Não prova som, cartões ou controles em jogo.
Os IRX são entrada de dados/relocação neste teste, não código MIPS executado.
Ainda falta substituí-los por dados preparados no empacotamento final.

A colisão VU0 original (entrada 0x7e8) passou em nove casos geométricos via C++
gerado diretamente. A imagem VU0 é A71F9755E12AF1B2; a imagem VU1 salva do
travamento é 87BD03B7FADB25F4. Uma única imagem VU1 não prova cobertura de todas
as fases, telas ou variantes de microprogramas do jogo.

Foram implementados emissão nativa de ERCPR, paradas D/T e retomada nativa de
delay slots após esgotar o orçamento. Testes de controle e latência passaram.
O primeiro teste detectou que uma otimização ignorava paradas D/T; foi corrigida
antes de se considerar esses testes aprovados.

## Integração em andamento

`CMakeLists.original-aot.txt` é opt-in e mantém os atalhos de jogo desativados.
Liga o EE recompilado original ao IOP AOT e aos programas VU compilados. Qualquer
imagem VU ainda não compilada causa captura e erro explícito. Não aciona PCSX2,
BIOS ou fallback para interpretadores de instruções.

Essa ponte ainda preserva registradores, memória e formatos DMA/VIF/GIF/GS.
Portanto, não representa ainda a conversão completa do motor para APIs PC.
Também não corrige automaticamente a imagem corrompida nem conclui o quadro
que fica esperando em 0x15ca40. Isso precisa ser validado no jogo original.

O teste RGB555 anterior passou isoladamente, mas o estado GS/VRAM do quadro
corrompido não foi capturado. A pesquisa de correções upstream não confirmou
que um patch de outro jogo se aplica a essa captura; nenhum foi aplicado às cegas.

Critério de conclusão: abrir o executável original convertido, entrar em uma fase,
renderizar corretamente e controlar o personagem com regras/animações originais,
incluindo teclado e controle. Áudio, menus, missões e saves também precisam ser
validados. Compilar um componente, abrir uma janela ou mostrar um modelo não
satisfaz esse critério.


## Continuação: fluxo gráfico e conclusão DMA (23/10/2026)

O candidato original foi compilado e executado com EE, IOP e as duas imagens VU
em C++ nativo, sem fallback para interpretador. Ainda não é um jogo jogável.
A arquitetura continua com pontes de memória/registradores e formatos do PS2;
a conversão integral para APIs PC e o empacotamento sem ELF/IRX continuam pendentes.

Foram encontrados e corrigidos dois defeitos independentes:

- O cabeçalho GIF IMAGE e seus pixels chegam em submissões DMA separadas.
  O parser anterior tratava os pixels como novos comandos. O montador por canal
  preserva cabeçalhos/payloads parciais. Quatro transferências reais agora produzem
  VRAM idêntica à referência nos 4.194.304 bytes, sem alterar TEX0 indevidamente.
  Um SIGNAL de PATH1 pode passar enquanto PATH3 espera seus pixels.
- Uma cadeia DMA com END e QWC=0 não produzia conclusão. No bloqueio real, o
  endereço 0x00b84d80 contém exatamente esse END vazio; o código original em
  0x0015a600 o envia e aguarda o IRQ GIF para iniciar VIF1. A regressão falhou
  antes da correção. Agora VIF0, VIF1 e GIF encerram essa cadeia e notificam seus
  handlers originais, sem falsificar busy=0 ou pular lógica do jogo.

O teste completo passou do primeiro nó preso (0x00c275d0), mas encontrou outra
falha no nó 0x0101df60. Foram executados 224 trabalhos VU nativos até esse ponto.
A rotina XGKICK reportou formato 3 num tag; é necessário conferir o pacote e
sua origem antes de atribuir a causa. A imagem segue incorreta e não houve
validação de movimento, animações, áudio, saves ou controle físico.

Evidências: `analysis/native-chain-before-empty-dma-fix.log`,
`analysis/native-chain-after-empty-dma-fix.log`,
`analysis/native-after-empty-dma-fix.ram`, `analysis/native-gif-stream-test.log`.
Teste: `scripts/Testar-Transferencias-Graficas.ps1`.


## Menu original aberto (24/10/2026)

A correção de arredondamento VU1 e o suporte a IMAGE2/EOP vazio fizeram o
candidato ultrapassar o loading e renderizar o menu original com Woody animado.
Foram observados mais de 11 mil trabalhos VU nativos nesta execução. Isso valida
o avanço da inicialização/menu, ainda não a entrada em fase ou a jogabilidade.
Imagem preservada: `analysis/native-original-menu.png`.

VU1 estava em truncamento. A rotina original usa cancelamento em ponto flutuante
para montar GIFtags; isso mudava NREG de 3 para 2 e deslocava o próximo tag.
A captura 222 agora produz sete tags completos e retorna em 0x338, preservando
o arredondamento do chamador. VU0 mantém o comportamento testado anteriormente.
A referência específica do título pode ser consultada em
[GameIndex.yaml, SLUS-20607](https://github.com/PCSX2/pcsx2/blob/master/bin/resources/GameIndex.yaml#L67470):
VU1 round mode 0. Foi consultado apenas o texto; PCSX2 não foi instalado nem
executado nesta continuação. O arquivo de referência local teve sua origem
registrada na análise do projeto.

A captura 398 contém um fim vazio com FLG=3, NLOOP=0 e EOP=1. Esse formato
válido era reportado como instrução reservada pela ponte XGKICK. O tratamento
corrigido produz nove tags e termina normalmente; não remove EOP nem simula
SIGNAL. As três regressões gráficas passaram em `native-graphics-regressions.log`.

O teste interativo detectou que X (confirmação) chegava como direcional.
`native/original/input/native_pad_services.cpp` preserva máscaras/eixos/pressão
lógicos e serializa os bytes na ordem lida pelo jogo Disney. O teste dos 16 bits
falhou antes da correção e passou depois, incluindo eixos, pressão de cruz e
soltura. Ainda falta validar controle físico, movimento e manobras numa fase.
