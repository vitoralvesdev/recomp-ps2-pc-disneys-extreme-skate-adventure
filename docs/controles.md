# Controles de PC

O teclado permanece ativo com um controle conectado. O primeiro gamepad detectado
entre quatro slots e usado; analogicos possuem zona morta de 18%.
Mapeamento por posicao de botao, mantendo os comandos definidos pelo jogo PS2:

| PS2 | Xbox | Teclado |
|---|---|---|
| Direcional | D-pad / analogico esquerdo | WASD / setas |
| Cruz | A | Espaco ou X |
| Quadrado | X | Z |
| Circulo | B | C |
| Triangulo | Y | V |
| L1 / R1 | LB / RB | Q / E |
| L2 / R2 | LT / RT | Shift esquerdo / direito, ou 1 / 3 |
| Start | Menu | Enter |
| Select | View | Tab |
| L3 / R3 | Clique nos analogicos | Ctrl esquerdo / direito |
| Analogico direito | Analogico direito | I J K L |

Escape fecha a janela. A atribuicao de manobras e determinada pelo jogo.
Controles Xbox reconhecidos pelo backend recebem o layout acima.
Um controle universal pode precisar do modo XInput ou de um mapeamento GLFW/SDL
compativel em gamecontrollerdb.txt na raiz do projeto. Nao presumir compatibilidade
com todo dispositivo USB; ainda e necessario testar seu modelo fisicamente.

Teste automatizado: scripts/Testar-Controles.cmd.
Verifica teclado e controle simultaneos, descoberta de slot, zona morta,
limites analogicos e retorno ao teclado apos desconexao, usando entrada simulada.
