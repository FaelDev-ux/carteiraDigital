#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"
#include "fibonacci.h"

void limparTerminal(void) {
#ifdef _WIN32      // # indica instrução para o pré-processador
    system("cls"); // comndo para windows
#else
    // comando para linux; evita limpar a tela em testes e terminais sem suporte.
    const char *terminal = getenv("TERM");
    if (terminal != NULL && strcmp(terminal, "dumb") != 0) {
        system("clear");
    }
#endif
}

void mostrarMenu(int aberto) {
    if (aberto == 1) {
        puts("=== Carteira Digital (ABERTO) ===");
    } else {
        puts("=== Carteira Digital (FECHADO) ===");
    }
    if (aberto == 1) {
        puts("1 - Fechar o banco");
    } else {
        puts("1 - Abrir o banco");
    }
    puts("2 - Histórico do banco");
    puts("3 - Efetuar sorteio");
    puts("4 - Cadastrar cliente");
    puts("5 - Adicionar conta");
    puts("6 - Acessar conta");
    puts("7 - Listar clientes e contas");
    puts("'s' - Sair");
}

/* Lê uma linha inteira em memória dinâmica, sem limitar o tamanho do nome.
   Começamos com um espaço pequeno e ampliamos com realloc quando necessário. */
char *lerTexto(const char *mensagem) {
    if (mensagem != NULL) {
        fputs(mensagem, stdout);
        fflush(stdout);
    }
    size_t capacidade = 32, tamanho = 0;
    char *linha = malloc(capacidade);
    if (linha == NULL) {
        return NULL;
    }
    int caractere;
    while ((caractere = getchar()) != '\n' && caractere != EOF) {
        if (caractere == '\0') {
            free(linha);
            return NULL;
        }
        if (tamanho + 1 == capacidade) {
            if (capacidade > SIZE_MAX / 2) {
                free(linha);
                return NULL;
            }
            size_t novaCapacidade = capacidade * 2;
            char *ampliada = realloc(linha, novaCapacidade);
            if (ampliada == NULL) {
                free(linha);
                return NULL;
            }
            linha = ampliada;
            capacidade = novaCapacidade;
        }
        linha[tamanho++] = (char)caractere;
    }
    if (caractere == EOF && tamanho == 0) {
        free(linha);
        return NULL;
    }
    if (tamanho > 0 && linha[tamanho - 1] == '\r') {
        tamanho--;
    }
    linha[tamanho] = '\0';
    return linha;
}

int converterInteiro(const char *texto, long long *valor) {
    if (texto == NULL || valor == NULL) {
        return 0;
    }
    errno = 0;
    char *fim = NULL;
    long long numero = strtoll(texto, &fim, 10);
    if (fim == texto || errno == ERANGE) {
        return 0;
    }
    while (ehEspaco(*fim) == 1) {
        fim++;
    }
    if (*fim != '\0') {
        return 0;
    }
    *valor = numero;
    return 1;
}

int converterDinheiro(const char *texto, long long *centavos) {
    if (texto == NULL || centavos == NULL) {
        return 0;
    }
    const char *p = texto;
    while (ehEspaco(*p) == 1) {
        p++;
    }
    int negativo = 0;
    if (*p == '-') {
        negativo = 1;
        p++;
    } else if (*p == '+') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }
    long long inteiro = 0;
    long long fracao = 0;
    /* Conversão decimal exata: não usamos float nem arredondamento binário. */
    while (*p >= '0' && *p <= '9') {
        int digito = *p - '0';
        if (inteiro > (LLONG_MAX / 100 - digito) / 10) {
            return 0;
        }
        inteiro = inteiro * 10 + digito;
        if (inteiro > LLONG_MAX / 100) {
            return 0;
        }
        p++;
    }
    if (*p == ',' || *p == '.') {
        p++;
        if (*p < '0' || *p > '9') {
            return 0;
        }
        fracao = (*p - '0') * 10;
        p++;
        if (*p >= '0' && *p <= '9') {
            fracao = fracao + (*p - '0');
            p++;
        }
    }
    while (ehEspaco(*p) == 1) {
        p++;
    }
    if (*p != '\0') {
        return 0;
    }
    long long valorInteiro = inteiro * 100;
    if (negativo == 1) {
        if (-valorInteiro < LLONG_MIN + fracao) {
            return 0;
        }
        *centavos = -valorInteiro - fracao;
    } else {
        if (valorInteiro > LLONG_MAX - fracao) {
            return 0;
        }
        *centavos = valorInteiro + fracao;
    }
    return 1;
}

void exibirDinheiro(long long centavos) {
    /* A transformação também funciona para LLONG_MIN, sem negar esse valor. */
    long long reais = centavos / 100;
    long long fracao = centavos % 100;
    if (centavos < 0) {
        printf("R$ -");
        reais = -reais;
        fracao = -fracao;
    } else {
        printf("R$ ");
    }
    printf("%lld,%02lld", reais, fracao);
}

/* entrada, navegação e apresentação.
   Regras, alterações de saldo e SQL são responsabilidade do backend. */
void mostrarResultado(Resultado resultado) { puts(mensagemResultado(resultado)); }
void mostrarConta(const Conta *conta) { printf("%04d-%02d", conta->numeroBase, (int)conta->tipo); }
int pedirInteiro(const char *mensagem, long long minimo, long long maximo, long long *valor) {
    while (1) {
        char *texto = lerTexto(mensagem);
        if (texto == NULL) {
            return 0;
        }
        int ok = converterInteiro(texto, valor) && *valor >= minimo && *valor <= maximo;
        free(texto);
        if (ok) {
            return 1;
        }
        puts("Informe um número inteiro dentro das opções indicadas.");
    }
}
int pedirValor(long long *valor) {
    while (1) {
        char *texto = lerTexto("Valor (ex.: 1000,50, sem separador de milhares): ");
        if (texto == NULL) {
            return 0;
        }
        int ok = converterDinheiro(texto, valor) && *valor > 0;
        free(texto);
        if (ok) {
            return 1;
        }
        puts("Informe um valor positivo, com até duas casas decimais.");
    }
}
void listarClientes(const Banco *banco) {
    if (banco->clientes == NULL) {
        puts("Nenhum cliente cadastrado.");
        return;
    }
    for (const Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        printf("ID %d | %s\n", c->id, c->nome);
        if (c->contas == NULL) {
            puts("  Sem contas. Use Adicionar conta.");
        }
        for (const Conta *a = c->contas; a != NULL; a = a->proximo) {
            printf("  ");
            mostrarConta(a);
            printf(" | ");
            exibirDinheiro(a->saldoCentavos);
            putchar('\n');
        }
    }
}
int adicionarConta(Banco *banco, int id) {
    long long tipo;
    if (!pedirInteiro("Tipo (1 corrente, 2 salário, 3 poupança): ", 1, 3, &tipo)) {
        return 0;
    }
    Conta *conta = criarConta(banco, id, (TipoConta)tipo);
    if (conta == NULL) {
        mostrarResultado(banco->ultimoResultado);
    } else {
        printf("Conta criada com saldo zero: ");
        mostrarConta(conta);
        putchar('\n');
    }
    return 1;
}
void cadastrar(Banco *banco) {
    char *nome = lerTexto("Nome do cliente: ");
    if (nome == NULL) {
        return;
    }
    Cliente *cliente = cadastrarCliente(banco, nome);
    free(nome); /* O backend já fez sua própria cópia dinâmica. */
    if (cliente == NULL) {
        mostrarResultado(banco->ultimoResultado);
        return;
    }
    printf("Cliente cadastrado. ID: %d\n", cliente->id);
    long long quantidade;
    if (!pedirInteiro("Quantidade de contas (1 ou 2): ", 1, 2, &quantidade)) {
        return;
    }
    for (long long i = 0; i < quantidade; i++) {
        if (!adicionarConta(banco, cliente->id)) {
            return;
        }
    }
}
void mostrarExtrato(Banco *banco, const char *numero) {
    const Movimentacao *movimentacao = consultarExtrato(banco, numero);
    if (banco->ultimoResultado != BANCO_OK) {
        mostrarResultado(banco->ultimoResultado);
        return;
    }
    puts("== Últimas 5 transações, da mais recente para a mais antiga ==");
    if (movimentacao == NULL) {
        puts("Nenhuma movimentação.");
    }
    int quantidade = 0;
    while (movimentacao != NULL && quantidade < 5) {
        exibirDinheiro(movimentacao->valorCentavos);
        printf(" : %c\n", movimentacao->operacao);
        movimentacao = movimentacao->proximo;
        quantidade++;
    }
}

void acessarConta(Banco *banco) {
    if (!banco->aberto) {
        mostrarResultado(ERRO_BANCO_FECHADO);
        return;
    }
    char *numero = lerTexto("Número da conta (ex.: 1000-01): ");
    if (numero == NULL) {
        return;
    }
    const Conta *conta = buscarConta(banco, numero);
    Cliente *titular = buscarTitular(banco, conta);
    if (conta == NULL || titular == NULL) {
        mostrarResultado(ERRO_CONTA_NAO_ENCONTRADA);
        free(numero);
        return;
    }
    while (1) {
        printf("\n%s | ", titular->nome);
        mostrarConta(conta);
        printf(" | ");
        exibirDinheiro(conta->saldoCentavos);
        putchar('\n');
        if (clienteComCorrenteNegativa(titular)) {
            puts("ALERTA: sua conta corrente está com saldo negativo.");
        }
        puts(
            "1 Depositar\n2 Sacar\n3 Solicitar empréstimo\n4 Adquirir título\n5 Extrato\n0 Voltar");
        long long opcao, valor;
        if (!pedirInteiro("Opção: ", 0, 5, &opcao) || opcao == 0) {
            break;
        }
        Resultado resultado;
        if (opcao == 5) {
            mostrarExtrato(banco, numero);
            continue;
        }
        if (opcao == 4) {
            mostrarResultado(adquirirTitulo(banco, numero));
            continue;
        }
        if (opcao == 3) {
            long long limite = 0;
            if (conta->tipo == CORRENTE) {
                resultado = consultarLimiteEmprestimo(banco, titular->id, &limite);
            } else {
                resultado = ERRO_EMPRESTIMO_NAO_PERMITIDO;
            }
            if (resultado != BANCO_OK) {
                mostrarResultado(resultado);
                continue;
            }
            printf("Limite disponível: ");
            exibirDinheiro(limite);
            putchar('\n');
        }
        if (!pedirValor(&valor)) {
            break;
        }
        if (opcao == 1) {
            resultado = depositar(banco, numero, valor);
        } else if (opcao == 2) {
            resultado = sacar(banco, numero, valor);
        } else {
            resultado = solicitarEmprestimo(banco, numero, valor);
        }
        mostrarResultado(resultado);
    }
    free(numero);
}
void mostrarLinhaCliente(const Cliente *cliente, const Conta *conta, long long valor) {
    printf("%s | ", cliente->nome);
    mostrarConta(conta);
    printf(" | ");
    exibirDinheiro(valor);
    putchar('\n');
}

void historicoBanco(Banco *banco) {
    long long total = 0;
    Resultado resultado = consultarSaldoTotal(banco, &total);
    if (resultado != BANCO_OK) {
        mostrarResultado(resultado);
        return;
    }
    puts("\n== Saldo total do banco ==");
    printf("%d clientes | ", banco->quantidadeClientes);
    exibirDinheiro(total);
    putchar('\n');

    puts("\n== Clientes aptos a empréstimos ==");
    Cliente *cliente = banco->clientes;
    int encontrados = 0;
    while (cliente != NULL) {
        long long limite = 0;
        resultado = consultarLimiteEmprestimo(banco, cliente->id, &limite);
        if (resultado == BANCO_OK) {
            mostrarLinhaCliente(cliente, contaDoTipo(cliente, CORRENTE), limite);
            encontrados++;
        } else if (resultado == ERRO_LIMITE_NUMERICO) {
            mostrarResultado(resultado);
        }
        cliente = cliente->proximo;
    }
    if (encontrados == 0) {
        puts("Nenhum cliente.");
    }

    puts("\n== Clientes com saldo negativo na conta corrente ==");
    cliente = banco->clientes;
    encontrados = 0;
    while (cliente != NULL) {
        if (clienteComCorrenteNegativa(cliente) == 1) {
            Conta *corrente = contaDoTipo(cliente, CORRENTE);
            mostrarLinhaCliente(cliente, corrente, corrente->saldoCentavos);
            encontrados++;
        }
        cliente = cliente->proximo;
    }
    if (encontrados == 0) {
        puts("Nenhum cliente.");
    }

    puts("\n== Clientes aptos à compra de títulos ==");
    cliente = banco->clientes;
    encontrados = 0;
    while (cliente != NULL) {
        if (aptoTitulo(cliente) == 1) {
            Conta *salario = contaDoTipo(cliente, SALARIO);
            mostrarLinhaCliente(cliente, salario, salario->saldoCentavos);
            encontrados++;
        }
        cliente = cliente->proximo;
    }
    if (encontrados == 0) {
        puts("Nenhum cliente.");
    }

    puts("\n== Clientes contemplados no sorteio ==");
    Premiacao *premio = banco->premiacoes;
    if (premio == NULL) {
        puts("Nenhum cliente.");
    }
    while (premio != NULL) {
        mostrarLinhaCliente(premio->cliente, premio->conta, premio->valorCentavos);
        printf("Sorteio %lld\n", premio->id);
        premio = premio->proximo;
    }
}

void sortear(Banco *banco) {
    Participante *lista = selecionarParticipantes(banco);
    Resultado resultado = banco->ultimoResultado;
    if (resultado != BANCO_OK) {
        mostrarResultado(resultado);
        return;
    }
    int quantidade = 0;
    puts("== Participantes por saldo decrescente ==");
    for (Participante *p = lista; p != NULL; p = p->proximo) {
        printf("%d. %s | ", ++quantidade, p->cliente->nome);
        mostrarConta(p->conta);
        printf(" | ");
        exibirDinheiro(p->conta->saldoCentavos);
        putchar('\n');
        long long soma = 0;
        if (somaFibonacci(p->conta->saldoCentavos, &soma) == BANCO_OK) {
            printf("   Pontuação Fibonacci: %lld\n", soma / 3);
        }
    }
    liberarParticipantes(lista);
    if (quantidade != 5) {
        mostrarResultado(ERRO_PARTICIPANTES_INSUFICIENTES);
        return;
    }
    puts("Classificação pelo algoritmo Fibonacci da aula, sem fatorial.");
    Premiacao *premio = realizarSorteio(banco);
    if (premio == NULL) {
        mostrarResultado(banco->ultimoResultado);
        return;
    }
    printf("Vencedor: %s | ", premio->cliente->nome);
    mostrarConta(premio->conta);
    printf(" | Prêmio: ");
    exibirDinheiro(premio->valorCentavos);
    putchar('\n');
}
int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Uso: %s [caminho-do-banco.db]\n", argv[0]);
        return EXIT_FAILURE;
    }
    Banco *banco = criarBanco();
    if (banco == NULL) {
        fputs("Memória insuficiente.\n", stderr);
        return EXIT_FAILURE;
    }
    const char *caminho = "carteira.db";
    if (argc == 2) {
        caminho = argv[1];
    }
    Resultado resultado = abrirPersistencia(banco, caminho);
    if (resultado != BANCO_OK) {
        fprintf(stderr, "%s\n", mensagemResultado(resultado));
        liberarBanco(banco);
        return EXIT_FAILURE;
    }
    while (1) {
        limparTerminal();
        mostrarMenu(banco->aberto);
        printf("Clientes cadastrados: %d\n", banco->quantidadeClientes);
        char *opcao = lerTexto("Opção: ");
        if (opcao == NULL) {
            break; /* EOF encerra de forma limpa, inclusive em testes. */
        }
        if (opcao[0] == 's' && opcao[1] == '\0') {
            free(opcao);
            break;
        }
        long long escolha;
        int valida = converterInteiro(opcao, &escolha);
        free(opcao);
        if (!valida) {
            puts("Opção inválida.");
            continue;
        }
        switch (escolha) {
        case 1:
            if (banco->aberto == 1) {
                banco->aberto = 0;
                puts("Banco fechado com sucesso.");
            } else {
                mostrarResultado(abrirBanco(banco));
            }
            break;
        case 2:
            historicoBanco(banco);
            break;
        case 3:
            sortear(banco);
            break;
        case 4:
            cadastrar(banco);
            break;
        case 5: {
            listarClientes(banco);
            long long id;
            if (pedirInteiro("ID do titular: ", 1, INT_MAX - 1LL, &id)) {
                adicionarConta(banco, (int)id);
            }
            break;
        }
        case 6:
            acessarConta(banco);
            break;
        case 7:
            listarClientes(banco);
            break;
        default:
            puts("Opção inválida.");
            break;
        }
        char *continuar = lerTexto("Pressione Enter para voltar ao menu.");
        if (continuar == NULL) {
            break;
        }
        free(continuar);
    }
    /* Cada alteração já foi gravada; não regravamos estados antigos ao sair. */
    liberarBanco(banco);
    return EXIT_SUCCESS;
}
