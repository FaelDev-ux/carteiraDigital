#include <stdio.h>
#include <stdlib.h>
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
  puts("'s' - Sair");
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
    default:
      puts("Opção inválida. Digite 1, 2, 3 ou s.");
      break;
    }

    int caractere;
    do {
      caractere = getchar();
    } while (caractere != '\n' && caractere != EOF);

    if (caractere == EOF) {
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
