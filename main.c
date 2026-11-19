#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

static void limparTerminal(void) {
#ifdef _WIN32 //# indica instrução para o pré-processador
  system("cls"); //comndo para windows
#else
  system("clear"); //comando para linux
#endif
}

static void mostrarMenu(int aberto) {
  printf("=== Carteira Digital (%s) ===\n", aberto ? "ABERTO" : "FECHADO");
  printf("1 - %s o banco\n", aberto ? "Fechar" : "Abrir");
  puts("2 - Histórico do banco");
  puts("3 - Efetuar sorteio");
  puts("4 - Cadastrar cliente");
  puts("5 - Adicionar conta");
  puts("6 - Acessar conta");
  puts("'s' - Sair");
}

static void descartarRestoDaLinha(void) {
  int caractere;
  do {
    caractere = getchar();
  } while (caractere != '\n' && caractere != EOF);
}

static void adicionarConta(Banco* banco, int clienteId) {
  puts("Tipo de conta (1 - Corrente, 2 - Salário, 3 - Poupança):");
  char tipo;
  if (scanf(" %c", &tipo) != 1) {
    return;
  }
  descartarRestoDaLinha();

  if (tipo < '1' || tipo > '3') {
    puts("Tipo de conta inválido.");
    return;
  }

  Conta* conta = criarConta(banco, clienteId, (TipoConta)(tipo - '0'));
  if (conta == NULL) {
    puts(mensagemResultado(banco->ultimoResultado));
    return;
  }

  printf("Conta criada: %04d-%02d\n", conta->numeroBase, (int)conta->tipo);
}

static void cadastrar(Banco* banco) {
  const int capacidade = 100;
  char* nome = malloc(capacidade);
  if (nome == NULL) {
    puts("Não foi possível alocar memória para o nome.");
    return;
  }

  puts("Digite o nome do cliente:");
  if (fgets(nome, capacidade, stdin) == NULL) {
    free(nome);
    return;
  }

  if (strchr(nome, '\n') == NULL) { //procura primeira ocorrencia numa string
    int proximo = getchar();
    if (proximo != '\n' && proximo != EOF) { // verifica se ainda sobrou parte do nome
      descartarRestoDaLinha();
      puts("O nome excede o limite do cadastro.");
      free(nome);
      return;
    }
  }

  Cliente* cliente = nome[0] == '\n' ? NULL : cadastrarCliente(banco, nome);
  free(nome);
  if (cliente == NULL) {
    puts("Não foi possível cadastrar o cliente.");
    return;
  }

  printf("Cliente cadastrado. ID: %d\n", cliente->id);

  puts("Quantas contas deseja criar (1 ou 2)?");
  char quantidade;
  if (scanf(" %c", &quantidade) != 1) {
    return;
  }
  descartarRestoDaLinha();

  if (quantidade != '1' && quantidade != '2') {
    puts("Quantidade inválida. Use a opção 5 para adicionar uma conta.");
    return;
  }

  for (int i = 0; i < quantidade - '0' && !feof(stdin); i++) {
    adicionarConta(banco, cliente->id);
  }
}

static int lerValor(long long* centavos) {
  char texto[64];
  puts("Digite o valor no formato (10,50):");
  if (fgets(texto, sizeof texto, stdin) == NULL) {
    return 0;
  }
  if (strchr(texto, '\n') == NULL && !feof(stdin)) {
    descartarRestoDaLinha();
    return 0;
  }

  char inteira[18], decimal[3], resto = '\n';
  int lidos = sscanf(texto, "%17[0-9],%2[0-9]%c", inteira, decimal, &resto); //passando texto e dividindo em 3 partes
  if (lidos < 2 || decimal[1] == '\0' || resto != '\n') {
    return 0;
  }

  long long reais = atoll(inteira); //converte string para long long
  int fracao = atoi(decimal); //converte string para int
  if (reais > (LLONG_MAX - fracao) / 100) { //evita estouro no long lng
    return 0;
  }

  *centavos = reais * 100 + fracao;
  return *centavos > 0; //retorna se foi positivo ou negativo
}

static void exibirDinheiro(long long centavos) {
  printf("R$ %s%lld,%02lld", centavos < 0 ? "-" : "",
    llabs(centavos / 100), llabs(centavos % 100));
}

static void mostrarExtrato(Banco* banco, const char* numero) {
  const Movimentacao* movimento = consultarExtrato(banco, numero);
  if (banco->ultimoResultado != BANCO_OK) {
    puts(mensagemResultado(banco->ultimoResultado));
    return;
  }

  puts("\n=== Extrato (últimas 5 transações) ===");
  if (movimento == NULL) {
    puts("Nenhuma movimentação.");
    return;
  }

  for (int i = 0; i < 5 && movimento != NULL; i++) { //backend já retorna na ordem
    exibirDinheiro(movimento->valorCentavos);
    printf(" : %c\n", movimento->operacao);
    movimento = movimento->proximo;
  }
}

static int acessarConta(Banco* banco) {
  if (!banco->aberto) {
    puts("Abra o banco para acessar uma conta.");
    return 0;
  }

  char numero[32];
  puts("Digite o número completo da conta (ex.: 1000-01):");
  if (scanf(" %31s", numero) != 1) {
    return 0;
  }
  descartarRestoDaLinha();

  const Conta* conta = buscarConta(banco, numero);
  if (conta == NULL) {
    puts(mensagemResultado(ERRO_CONTA_NAO_ENCONTRADA));
    return 0;
  }

  const Cliente* titular = buscarTitular(banco, conta);
  if (titular == NULL) {
    puts("Titular não encontrado.");
    return 0;
  }

  limparTerminal();
  printf("Conta %04d-%02d\n", conta->numeroBase, (int)conta->tipo);
  printf("Cliente: %s%s", titular->nome, strchr(titular->nome, '\n') ? "" : "\n");

  while (!feof(stdin)) {
    printf("\nSaldo: ");
    exibirDinheiro(conta->saldoCentavos);
    putchar('\n');
    if (clienteComCorrenteNegativa(titular)) {
      puts("Atenção: sua conta corrente está com saldo negativo.");
    }
    puts("1 - Depositar\n2 - Sacar\n3 - Solicitar empréstimo\n"
      "4 - Adquirir título\n5 - Extrato\n0 - Voltar");

    char opcao;
    if (scanf(" %c", &opcao) != 1) {
      break;
    }
    descartarRestoDaLinha();
    if (opcao == '0') {
      break;
    }
    if (opcao == '5') {
      mostrarExtrato(banco, numero);
      continue;
    }
    if (opcao == '4') {
      puts(mensagemResultado(adquirirTitulo(banco, numero)));
      continue;
    }
    if (opcao < '1' || opcao > '3') {
      puts("Opção inválida.");
      continue;
    }

    if (opcao == '3') {
      long long limite;
      Resultado consulta = conta->tipo == CORRENTE
        ? consultarLimiteEmprestimo(banco, titular->id, &limite)
        : ERRO_EMPRESTIMO_NAO_PERMITIDO;
      if (consulta != BANCO_OK) {
        puts(mensagemResultado(consulta));
        continue;
      }
      printf("Limite disponível: ");
      exibirDinheiro(limite);
      putchar('\n');
    }

    long long valor;
    if (!lerValor(&valor)) {
      puts("Informe um valor positivo no formato 10,50.");
      continue;
    }

    Resultado resultado;
    if (opcao == '1') {
      resultado = depositar(banco, numero, valor);
    }
    else if (opcao == '2') {
      resultado = sacar(banco, numero, valor);
    }
    else {
      resultado = solicitarEmprestimo(banco, numero, valor);
    }
    puts(mensagemResultado(resultado));
  }
  return 1;
}

int main(void) {
  Banco* banco = criarBanco();
  if (banco == NULL) {
    puts("Não foi possível criar o banco.");
    return EXIT_FAILURE;
  }

  while (1) {
    int aberto = banco->aberto;
    limparTerminal();
    mostrarMenu(aberto);
    puts("Digite a opção:");

    char letra;
    if (scanf(" %c", &letra) != 1) {
      break;
    }

    if (letra == 's') {
      puts("Até logo!");
      break;
    }

    descartarRestoDaLinha();

    switch (letra) {
    case '1':
      if (aberto) {
        banco->aberto = 0;
        puts("Banco fechado com sucesso.");
      }
      else {
        puts(abrirBanco(banco) == BANCO_OK ? "Banco aberto com sucesso."
          : "Não foi possível abrir o banco.");
      }
      break;
    case '2':
      puts("Histórico do banco em breve.");
      break;
    case '3':
      puts("Sorteio em breve.");
      break;
    case '4':
      cadastrar(banco);
      break;
    case '5': {
      puts("Digite o ID do cliente:");
      int id;
      int leu = scanf("%d", &id);
      descartarRestoDaLinha();
      if (leu == 1) {
        adicionarConta(banco, id);
      }
      else if (!feof(stdin)) {
        puts("ID inválido.");
      }
      break;
    }
    case '6':
      if (acessarConta(banco)) {
        continue;
      }
      break;
    default:
      puts("Opção inválida. Digite um número de 1 a 6 ou s.");
      break;
    }

    if (feof(stdin)) {
      break;
    }

    puts("Pressione Enter para voltar ao menu.");
    if (getchar() == EOF) {
      break;
    }
  }

  liberarBanco(banco);
  return 0;
}
