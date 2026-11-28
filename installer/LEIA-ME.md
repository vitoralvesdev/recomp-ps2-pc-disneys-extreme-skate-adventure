# Disney's Extreme Skate Adventure — Windows x64

Versão **0.5.0 do launcher**, em desenvolvimento. Este pacote não contém a ISO nem arquivos extraídos do jogo.

## Instalar

1. Baixe `DisneySkateSetup-0.5.0.exe` na página [Releases do projeto](https://github.com/vitoralvesdev/recomp-ps2-pc-disneys-extreme-skate-adventure/releases) e confira o SHA-256 em `SHA256SUMS.txt`.
2. Abra o instalador e siga as etapas. Não é necessário instalar Python, .NET, CMake ou Visual Studio.
3. Abra **Disney's Extreme Skate Adventure** e clique em **Selecionar ISO**.
4. Escolha sua cópia original da versão americana NTSC-U de PlayStation 2.
5. Clique em **Instalar jogo** e aguarde a extração, a conversão dos assets e a auditoria.
6. Quando **JOGAR** ficar disponível, clique para iniciar.

A ISO é lida localmente e não é enviada para a internet. A instalação usa cerca de **2,3 GB** de espaço livre, além do pacote. A imagem esperada é a edição americana com `SLUS_206.07`; versões PAL ou de outra região são recusadas.

Esta build não possui assinatura Authenticode. O Windows pode mostrar um aviso do SmartScreen; confira o SHA-256 no `SHA256SUMS.txt` antes de executar. A atualização automática não está disponível para a 0.5.0.

## Usar a versão portátil

Como alternativa ao instalador, extraia todo `DisneySkate-0.5.0-portable.zip` para uma pasta e abra `Disney's Extreme Skate Adventure.exe` dentro de `DisneySkate-0.5.0`. Não execute o launcher sem extrair os arquivos de `runtime\`, `scripts\` e `tools\`.

O ZIP portátil não é atualizado automaticamente. A versão 0.5.0 e outras versões sem assinatura Authenticode devem ser atualizadas manualmente, preservando a pasta de dados do jogo, os saves e a configuração.

## Requisitos

- Windows 10/11 x64.
- GPU e driver compatíveis com OpenGL 3.3.
- Uma ISO original americana NTSC-U.
- Espaço livre para o pacote e aproximadamente 2,3 GB para os dados preparados.

As DLLs necessárias do Microsoft Visual C++ acompanham o runtime. O launcher e o conversor COL8 usam CRT estática. O Python incorporado e os conversores também acompanham o pacote.

## Verificação e problemas

Os comandos abaixo podem ser executados no Prompt de Comando ou no PowerShell, na pasta do launcher:

```bat
"Disney's Extreme Skate Adventure.exe" --versao
"Disney's Extreme Skate Adventure.exe" --verificar
```

`--verificar` retorna **0** quando o pacote está íntegro e **2** quando há falha; não testa o gameplay. Os logs ficam em `%LOCALAPPDATA%\HighSky\DisneySkate\logs` na instalação ou em `analysis\` no modo portátil.

Para diagnosticar, atualize o driver da GPU pelo fabricante. Ao relatar um problema, informe a fase, o personagem, a ação e o resultado. **Nunca anexe ISO, WAD, saves ou outros dados originais do jogo.**

## Estado e limites

O port ainda está em validação: algumas fases podem travar, o carregamento pode demorar e recursos de gráficos, áudio, controles e saves ainda não foram validados integralmente. Este é um projeto não oficial da comunidade, não um emulador. Use apenas uma cópia legalmente adquirida.

Os avisos e a procedência das bibliotecas de terceiros estão em `third-party-notices\`. O arquivo `BUILD-INFO.txt` identifica a origem do runtime e as ferramentas usadas nesta build. `package-files.sha256` permite conferir todos os arquivos do payload; ele não inclui a si mesmo nem o marcador vazio `launcher.portable`, adicionado somente ao ZIP portátil.
