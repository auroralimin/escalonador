# Escalonador
Trabalho implementado como requisito da disciplina Sistemas Operacionais pelos alunos:
* Aurora Li Min de Freitas Wang       (13/0006408)
* Eduardo Scartezini Correia Carvalho (14/0137084)

# Dependências

Esse código depende da biblioteca padrão e do compilador "gcc" para a versão 11 (ou superior) da linguagem C++.

Além disso, assume-se que o ambiente de execução é Unix-like e contém um diretório temporário `/tmp`. 
Isso porque, apesar do código ***não usar*** arquivos para comunicação entre processos, é usado um padrão
que salva o pid de um deamon em um arquivo `/tmp/<deamon>.pid`.

# Para compilar

## Opção geral
Na raiz do diretório principal, execute:

    $ make
    
## Opção debug
Na raiz do diretório principal, execute:

    $ make debug

Ressalta-se que essa opção adiciona informações de debug, tais como prints durante a execução dos processos.

# Para limpar
Na raiz do diretório principal, execute:

    $ make clean
    
# Para executar

## Escalonador
Na raiz do diretório principal, execute:

    $ ./escalonador &
    
## Solicita execução
Na raiz do diretório principal, execute:

    $ ./solicita_execucao <hora:min> <copias> <prioridade> <nome arq exec>

O número de cópias deve ser maior que zero, a prioridade entre 1 e 3 e o executavel deve existir.

O retorno esperado é uma tabela de postergados com uma única entrada,
que contém as informações da execução postergada solicitada
(incluindo o seu `id`).

Observa-se que o `nome arq exec` pode ser o caminho/nome de qualquer arquivo executável.
Mas, para fins de exemplo, pode-se usar o executável `hello_world`

## Remove postergado
Na raiz do diretório principal, execute:

    $ ./remove_postergado <id job>
    
## Lista postergados
Na raiz do diretório principal, execute:

    $ ./lista_postergados
    
O retorno esperado é uma tabela de postergados com todos os processos que ainda estão esperando execução.

## Shutdown postergados
Na raiz do diretório principal, execute:

    $ ./shutdown_postergado
    
O retorno esperado são 3 tabelas:

1. Tabela de execuções postergadas, com os processos que nunca começaram a execução;
2. Tabela de jobs que começaram a executar e que foram interrompidos pelo shutdown antes de acabarem;
3. Tabela de jobs que executaram e terminaram sua execução normalmente.

# Especificação detalhada da implementação:

Os arquivos contendo o código do projeto estão organizados em dois diretórios: `headers/` e `src/`.
Para a comunicação entre processos, os mecanismos utilizados foram:
* Sinais: Para a notificação de eventos entre processos;
* Caixa de mensagens: Uma única caixa de mensagens foi compartilhada entre todos os processos,
                      para que eles comunicassem mensagens entre si.

## `headers/common.h`:
Inclui bibliotecas, macros, enums e estruturas de dados comuns a múltiplos arquivos fonte do projeto.

### Bibliotecas
As bibliotecas incluidas por esse "header" são em sua maioria bibliotecas de IPC, tratamento de stream e string,
dentre outras bibliotecas padrões.

### Macros
A diretiva `#define` foi utilizada para definir macros contendo informações compartilhadas por mais de um código fonte,
tais como valores relacionados a caixa de mensagem, strings/comandos para formatação de output, dentre outros.

### Enums
* `color`: ID do scape code da cor a ser printada no terminal;
* `msg_type`: Tipo da mensagem sendo trocada pela caixa de mensagens (Para permitir só uma caixa de mensagens,
              mas múltiplas vias de comunicação separadas)

### Structs
* `job`: Contém a abstração de um job e outras informações úteis para a execução do algoritmo de escalonamento;
* `bufferTuple`: Abstrai o buffer da mensagem a ser mandado quando a via de comunicação da caixa de mensagens estiver
                 trocando informações a respeito da tupla de informações sobre a execução de um postergado;
* `bufferMap`: Abstrai o buffer da mensagem a ser mandado quando a via de comunicação da caixa de mensagens estiver
               trocando informações a respeito do mapa contendo a listagem dos postergados;
* `bufferJob`: Abstrai o buffer da mensagem a ser mandado quando a via de comunicação da caixa de mensagens estiver
               trocando informações a respeito de um job (Utiliza `struct job` para o conteúdo da mensage);

## `src/escalonador.cpp`
Implementa a primeira parte da lógica do escalonador, tendo assim as seguintes responsabilidades:
* Criar a caixa de mensagens a ser utilizada por todas as trocas de mensagens entre processos;
* Fazer um fork e mandar o executavel do `escalona_jobs` executar;
* Em loop, esperar uma mensagem chegar informando sobre uma execução postergada;
* Para cada execução postergada que chegar, fazer um fork para cuidar dessa execução;
* O fork que cuida de uma execução deve parsear as informações da execução, esperar o tempo de postergação,
  para então mandar para o processo do `escalona_jobs` via caixa de mensagens uma tupla para cada cópia da
  execução a ser feita;
* O processo pai é responsável também por tratar alguns sinais recebidos, de modo a dar "wait" em seus filhos mortos,
  morrer graciosamente quando necessário e mandar por mensagem a lista de postergados quando assim sinalizado.

## `src/escalona_jobs.cpp`
Implementa a segunda parte da lógica do escalonador, tendo assim as seguintes responsabilidades:
* Abrir a caixa de mensagens;
* Em loop, esperar uma mensagem chegar informando sobre job(s) a ser(em) executado(s);
* Incluir cada job que chega em alguma das filas de execução de acordo com sua prioridade;
* Executar um por vez o job com maior prioridade, durante 5 segundos (tal mecanismo é implementado com alarme);
* Mudar prioridades dinâmicamente conforme a execução;
* O processo pai é responsável também por tratar alguns sinais recebidos, de modo a dar "wait" em seus filhos mortos,
  morrer graciosamente quando necessário e mandar por mensagem a lista de jobs que estavam executando e que foram
  finalizados quando assim sinalizado.

## `src/hello_world.cpp`
Implementação que dura por volta de 30 segundos e faz prints de 1 em 1 segundo quando executando
(serve de exemplo como executavel a passar para o escalonador executar).

## `src/lista_postergados.cpp`
Responsável pelas seguintes implementações:
* Abrir a caixa de mensagens;
* Mandar um sinal para o processo do `escalonador` pedindo a lista de postergados;
* Esperar para receber essa lista pela caixa de mensagens;
* Imprimir a lista de postergados.

## `src/remove_postergado.cpp`
Responsável pelas seguintes implementações:
* Conferir se recebeu como argumento o id do postergado a ser removido;
* Manda um sinal `SIGTERM` para o processo que está cuidando desse postergado
  (na implementação o id do postergado é o mesmo do pid do processo).

## `src/shutdown_postergado.cpp`
Responsável pelas seguintes implementações:
* Abrir a caixa de mensagens
* Sinalizar ambos os processos principais do escalonador (`escalonador` e `escalona_jobs`) do shutdown;
* Receber via caixa de mensagens a lista de postergados cancelados e printar;
* Receber via caixa de mensagens os jobs que foram forçados a pararem de executar devido o shutdown e printar;
* Receber via caixa de mensagens os jobs que já haviam executado e finalizado a execução e printar.

## `src/solicita_execucao.cpp`:
Responsável pelas seguintes implementações:
* Conferir se os argumentos esperado com informações sobre a execução postergada foram passados;
* Abrir a caixa de mensagens;
* Mandar pela caixa de mensagens as informações da execução postergada;
* Esperar confirmação de recebimento também pela caixa de mensagens, contendo o id da execução;
* Printar as informações da execução postergada, incluindo o id
