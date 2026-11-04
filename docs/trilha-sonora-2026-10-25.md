# Trilha sonora recuperada — 25/10/2026

Foram instaladas **27 faixas que faltavam**, incluindo 14 temas de personagens e 13 canções. Com os dez ambientes existentes, o catálogo tem **37 entradas de música** e **1.285 recursos de áudio** no total.

As músicas foram recuperadas da própria ISO do usuário, sem baixar ou substituir as gravações por áudio do YouTube. A [playlist enviada](https://www.youtube.com/watch?v=ygPl-t01_U0&list=PLxLkpANYnG3_19LskNpZQEmsh7cn83Bwz) fica registrada como referência; sua identificação como “oficial” não foi verificada.

## Conversão e correspondência

O índice MUSIC.HED fornece os nomes originais, como music/woody, music/downfall, music/sell_out e music/spinning_around. Esses mesmos nomes agora resolvem para PCM estéreo, 16 bits, 48 kHz, no catálogo usado pelo mixer Windows. Nenhuma música foi associada por aproximação de nome.

O módulo original EZPCM.IRX foi consultado no Ghidra: FUN_000005F4 usa o deslocamento 0x18000 para o segundo canal. Os dados físicos são organizados em pares de 0x30000 bytes. O tamanho lógico do índice não inclui necessariamente todo o bloco físico final; ler somente esse tamanho corta o canal direito. A conversão lê os blocos físicos completos e limita a duração pelo tamanho lógico original.

Dez ambientes existentes do acervo GameCube serviram como referência independente: foram comparadas janelas de início, meio e final em ambos os canais. Todos os segmentos não silenciosos passaram o limite de correlação 0,995. O ambiente HUB teve correlação superior a 0,9998 e deslocamento constante de 28 amostras, inclusive no final do canal direito. Não se afirma identidade binária entre plataformas.

## Testes

- Os 37 arquivos PCM foram verificados quanto a estrutura RIFF, canais, taxa, quantidade de amostras e SHA-256.
- Os 37 nomes passaram pela ponte original de música 0x11A220, com estado ativo, produção de PCM não nulo, parada e retorno ao estado disponível.
- Teste Windows/WASAPI: a trilha recuperada produziu 98.371 quadros de áudio não nulos em aproximadamente dois segundos. Isso verifica consumo pelo dispositivo, não o volume físico percebido pelo usuário.
- A regressão existente de efeitos, vozes, volume, pitch, pausa e ambientes continuou passando.

Manifestos: analysis/music-recovery-2026-10-25.json e analysis/audio-asset-installation.json. Evidências e logs em C:/Users/vitor/OneDrive/Documentos/ChatGPT/REA/music-recovery-2026-10-25/.

## Uso e reprodução da instalação

Abra Disney's Extreme Skate Adventure.exe na pasta principal. O runtime existente carrega o catálogo ampliado automaticamente; não foi adicionada biblioteca ao jogo.

Para reconstruir os WAVs, use scripts/Recuperar-Trilha-ISO.ps1. O conversor offline utiliza o compilador e o CMake já instalados. Fontes em scripts/music-recovery/. Se reinstalar o catálogo antigo com Instalar-Audios-Originais.py, execute depois a recuperação da trilha.

Os WAVs novos estão em assets/native/audio/files, nomeados por SHA-256; MUSIC.HED, MUSIC.WAD, o ELF original e os saves não foram modificados. Não há músicas faltantes no manifesto. Áudio de vídeos, reverberação completa e percepção de cada faixa durante partidas inteiras continuam sem certificação.
