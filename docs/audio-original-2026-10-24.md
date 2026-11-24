# Áudio original — integração de 24/10/2026

Atualização de 25/10: as 27 músicas antes ausentes foram recuperadas da ISO; o catálogo passou de 1.258 para 1.285 recursos, incluindo 37 entradas de música. Consulte [recuperação e testes](trilha-sonora-2026-10-25.md). Os números da integração inicial abaixo são históricos.

Atualização de 24/11/2026: o instalador não exige mais os 19 ZIPs externos e converte o áudio diretamente dos dados da ISO selecionada.

O executável principal continua sendo `Disney's Extreme Skate Adventure.exe`.
Ele abre `runtime/DisneySkateRuntime.exe`, agora com mixer PCM nativo para os
eventos originais de efeitos, vozes e os ambientes disponíveis. Mantenha também
a pasta `assets`, onde estão os WAVs e o catálogo `native/audio/catalog.tsv`.

## Arquivos e correspondência

Fonte indicada pelo usuário: [Disney's Extreme Skate Adventure — Sounds Resource](https://sounds.spriters-resource.com/gamecube/disneysextremeskateadventure/).
Foram baixados os 19 pacotes. Contêm 808 WAVs; 480 são variantes francesa/alemã.
Os 328 WAVs de inglês e efeitos foram validados e todos encontraram nomes
correspondentes nos índices/recursos da ISO USA de PS2. A correspondência por
nome não significa que as formas de onda das duas plataformas sejam idênticas.

Há 1.258 recursos lógicos instalados. Os sons ausentes do acervo foram convertidos
dos arquivos VAG e do banco STREAMS da própria ISO. WAVs idênticos compartilham
o mesmo arquivo por SHA-256; nenhuma variante de idioma é escolhida ao acaso.
O manifesto `analysis/audio-asset-installation.json` registra origem, nome,
checksum, duração, taxa de amostragem e loop de cada recurso. A instalação
ocupa aproximadamente 506 MiB de WAVs; os pacotes ZIP permanecem em Downloads.

Exemplos de associação:

| Pedido original | Recurso instalado |
|---|---|
| `Shared\Menu\MenuSelect` | `Menu Sounds/MenuSelect.wav` do acervo |
| CRC `3820e30d` / `woody_bail01` | `Woody/English/woody_bail01.wav` |
| `music\amb_hub` | `amb_hub.wav` |
| `Terrains\RollWood` | VAG correspondente da ISO convertido para PCM |

Não se usa correspondência aproximada. Caminhos de efeitos são exatos; vozes
usam o CRC de nome original. Um recurso desconhecido retorna falha em vez de
reproduzir o último som carregado. Não há colisões nos 855 hashes de vozes.

## Engenharia reversa

O Ghidra analisou o executável original `SLUS_206.07`, SHA-256
`2fe6ce3a08aade170be27ea221137c96aa15083a408d61856f605f1a40f321fc`.
O instalador não altera o ELF original.

As assinaturas foram conferidas na descompilação e nas instruções AOT:

| Endereço | Papel |
|---|---|
| `13e880` | carregar som por nome e preencher descritor |
| `13ecd8` | iniciar efeito: volumes/pitch em f12–f14, descritor/voz em a0/a1 |
| `13e8c8` | atualizar volume e pitch do canal |
| `13e7c8`, `13ec18` | parar canal / todos os canais |
| `13f000`, `13f0d8` | disponibilidade e reprodução dos canais |
| `11a398` | iniciar voz por CRC do nome, não ponteiro de string |
| `11a220` | iniciar música/ambiente por caminho |
| `11a898`, `11a8c8` | estado de música e vozes |
| `107780` | CRC original: inicial FFFFFFFF, minúsculas, barras normalizadas |

Os relatórios `analysis/audio-*-functions.txt`, `audio-name-crc.txt`,
`audio-stream-name-lookup.txt` e `audio-stream-metadata.txt` preservam a evidência.
REA inspecionou a integridade do ZIP de menu; o conjunto de evidências está em
`analysis/audio-menu-rea-evidence.json`, incluindo a inspeção
`ev_1c87c4522a1d3a29123a4fa285238f76e9c860b082182e1a707ce0cb403c1c2c`.
Essa inspeção estática não certifica comportamento em execução nem ausência
universal de malware.

As escolhas de sons, terrenos e eventos permanecem no código original AOT.
O novo código substitui as operações de reprodução de áudio por PCM no Windows;
não interpreta instruções do processador de áudio do PS2. A arquitetura geral
do port ainda conserva suas pontes de memória/registradores documentadas.

## Validação e limites

`DisneyNativeAudioTest` exercita os mesmos handlers usados pelo jogo:
nomes exatos e desconhecidos, retorno para o chamador original, sobreposição
do mesmo som, canais esquerdo/direito, volume, pitch, finalização, pausa,
retomada, parada individual/global, voz do Woody por CRC e loop de terreno.
Também renderiza um WAV com amostras reais de menu, voz e ambiente.
O teste com `--device` inicializa WASAPI e verifica que o callback consome PCM
não nulo: 92.117 quadros estéreo da voz foram processados no teste de dois segundos.
Isso confirma envio à saída do Windows, não a percepção auditiva
do usuário nem o volume físico do equipamento.

Foi feita uma única inicialização do jogo, com limite de 32 segundos. O jogo
registrou o catálogo, abriu a saída Windows a 48 kHz estéreo e acionou um efeito
original no canal 22. O código 75 é a saída intencional desse teste limitado.
Não foram certificadas partidas completas ou todas as falas em contexto.
O executável normal não contém esse limite de tempo.

Ainda faltam:

* Resolvido em 25/10: as 27 músicas de MUSIC.WAD foram convertidas e integradas; o manifesto não lista mais músicas ausentes.
* Reprodução e áudio dos vídeos originais, que ainda registram `ABORT (4)`.
* Conferência auditiva de todas as fases, reverberação e fidelidade completa da
  mixagem. O mixer respeita volumes, pitch, pausa e loops; não reproduz o DSP
  completo de reverberação do hardware original.

Os gráficos, o formato dos saves e os controles não foram alterados por esta
integração. Saves existentes permanecem nos mesmos diretórios.
