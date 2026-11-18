#include <stdio.h>
#include <stdlib.h>

static void limparTerminal(void) {
#ifdef _WIN32 //# indica instrução para o pré-processador
  system("cls"); //comndo para windows
#else
  system("clear"); //comando para linux
#endif
}

static void mostrarMenu(void) {
  puts("=== Carteira Digital ===");
  puts("1 - Abrir o banco");
  puts("2 - Histórico do banco");
  puts("3 - Efetuar sorteio");
  puts("'s' - Sair");
}

int main(void) {
  while (1) {
    limparTerminal();
    mostrarMenu();
    puts("Digite a opção:");

    char letra;
    if (scanf(" %c", &letra) != 1) {
      break;
    }

    switch (letra) {
    case '1':
      puts("Abertura do banco em breve.");
      break;
    case '2':
      puts("Histórico do banco em breve.");
      break;
    case '3':
      puts("Sorteio em breve.");
      break;
    case 's':
      puts("Até logo!");
      return 0;
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

  return 0;
}
