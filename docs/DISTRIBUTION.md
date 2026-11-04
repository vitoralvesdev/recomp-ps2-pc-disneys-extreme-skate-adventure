# Política de distribuição

Este projeto deve publicar apenas código próprio, scripts, arquivos de configuração e documentação necessários para reproduzir o trabalho de desenvolvimento.

## Nunca enviar ao GitHub

- ISO, ELF, WAD/HED, IRX, texturas, modelos, músicas, vozes, vídeos ou qualquer outro dado extraído de Disney's Extreme Skate Adventure.
- Saves, memory cards, logs com dados pessoais, capturas de RAM/VRAM e dumps.
- Executáveis gerados, DLLs de runtime, diretórios de build e diagnósticos grandes.
- Ferramentas de terceiros redistribuídas sem uma auditoria de licença específica.

## Antes de uma release

1. Confirme que `git status --ignored` não revela dados do jogo fora das regras previstas no `.gitignore`.
2. Faça uma revisão de licenças das bibliotecas e ferramentas distribuídas.
3. Teste o iniciador a partir de uma pasta limpa com os dados legalmente fornecidos pelo jogador.
4. Registre os problemas conhecidos de gráficos, colisão e carregamento na release.

O repositório não contém uma licença global porque a situação de autoria e das dependências ainda precisa de revisão antes de uma publicação pública. Não presuma que o código ou os recursos possam ser reutilizados sem essa revisão.
