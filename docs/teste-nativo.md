> Documento histórico. Os executáveis e atalhos desta versão foram removidos na limpeza. Para abrir o candidato atual, use `Disney's Extreme Skate Adventure.exe` na pasta principal.

# Estado atual do laboratório

Esta aplicação foi rejeitada como solução para o port: ela combina recursos
reais com uma simulação de física escrita à parte, sem executar a lógica
original. Está preservada apenas para inspeção de recursos e fora do build
padrão e do atalho Jogar.cmd.

Para o objetivo atual e as evidências do código original, consulte
`port-original.md`. O jogo original nativo ainda não está pronto.

O atalho separado `Jogar-Teste-Nativo.cmd` continua acessível somente como
laboratório, com aviso explícito. `Compilar-Laboratorio-Recursos.ps1` usa a opção
`DISNEY_BUILD_RESOURCE_LAB=ON`. Seus resultados de renderização e colisão não
comprovam equivalência ao jogo original.