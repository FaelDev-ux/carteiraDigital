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
  puts("'s' - Sair");
}

static void descartarRestoDaLinha(void) {
  int caractere;
  do {
    caractere = getchar();
  } while (caractere != '\n' && caractere != EOF);
}

static void cadastrar(Banco *banco) {
  const int capacidade = 100;
  char *nome = malloc(capacidade);
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

  Cliente *cliente = nome[0] == '\n' ? NULL : cadastrarCliente(banco, nome);
  free(nome);
  if (cliente == NULL) {
    puts("Não foi possível cadastrar o cliente.");
    return;
  }

  printf("Cliente cadastrado. ID: %d\n", cliente->id);
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
    default:
      puts("Opção inválida. Digite 1, 2, 3, 4 ou s.");
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
