#include <limits.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "meu_banco.h"

Banco *criarBanco(void) {
    Banco *banco = malloc(sizeof *banco);
    if (banco == NULL) {
        return NULL;
    }
    /* Inicializamos cada ponteiro e contador antes de publicar a estrutura. */
    banco->clientes = NULL;
    banco->quantidadeClientes = 0;
    banco->proximoIdCliente = 1;
    banco->proximoNumero = 1000;
    banco->aberto = 0;
    banco->premiacoes = NULL;
    banco->proximoSorteio = 1;
    banco->revisao = 0;
    banco->conexao = NULL;
    banco->ultimoResultado = BANCO_OK;
    return banco;
}

void liberarBanco(Banco *banco) {
    if (banco == NULL) {
        return;
    }
    /* Premiações referenciam clientes/contas; não são donas desses registros. */
    while (banco->premiacoes != NULL) {
        Premiacao *proxima = banco->premiacoes->proximo;
        free(banco->premiacoes);
        banco->premiacoes = proxima;
    }
    Cliente *cliente = banco->clientes;
    while (cliente != NULL) {
        Cliente *proximoCliente = cliente->proximo;
        Conta *conta = cliente->contas;
        while (conta != NULL) {
            Conta *proximaConta = conta->proximo;
            liberarMovimentacoes(conta->historico);
            free(conta);
            conta = proximaConta;
        }
        free(cliente->nome);
        free(cliente);
        cliente = proximoCliente;
    }
    if (banco->conexao != NULL) {
        sqlite3_close(banco->conexao);
    }
    free(banco);
}

Resultado abrirBanco(Banco *banco) {
    if (banco == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    banco->aberto = 1;
    return BANCO_OK;
}

/* Testamos os limites antes da soma, inclusive para saldos negativos. */
int somaSegura(long long a, long long b, long long *resultado) {
    if ((b > 0 && a > LLONG_MAX - b) || (b < 0 && a < LLONG_MIN - b)) {
        return 0;
    }
    *resultado = a + b;
    return 1;
}

const char *mensagemResultado(Resultado resultado) {
    switch (resultado) {
    case BANCO_OK:
        return "Operação realizada.";
    case ERRO_ARGUMENTO_INVALIDO:
        return "Dados inválidos.";
    case ERRO_BANCO_FECHADO:
        return "Abra o banco primeiro.";
    case ERRO_MEMORIA:
        return "Memória insuficiente.";
    case ERRO_CONTA_NAO_ENCONTRADA:
        return "Conta inválida ou não encontrada.";
    case ERRO_LIMITE_NUMERICO:
        return "O valor ultrapassa a capacidade numérica do sistema.";
    case ERRO_SALDO_INSUFICIENTE:
        return "Saldo insuficiente.";
    case ERRO_COMBINACAO_CONTAS:
        return "Tipo repetido ou combinação de contas proibida.";
    case ERRO_CLIENTE_NAO_ENCONTRADO:
        return "Cliente não encontrado.";
    case ERRO_EMPRESTIMO_NAO_PERMITIDO:
        return "Empréstimo indisponível para este cliente/valor.";
    case ERRO_TITULO_NAO_PERMITIDO:
        return "Título indisponível: confira conta salário, faixa de saldo e aquisição anterior.";
    case ERRO_PARTICIPANTES_INSUFICIENTES:
        return "São necessários cinco clientes elegíveis com título.";
    case ERRO_PERSISTENCIA:
        return "Falha ao ler/gravar o banco de dados.";
    }
    return "Erro desconhecido.";
}

/* Reconhece os caracteres usados como espaço no texto de entrada. */
int ehEspaco(char caractere) {
    if (caractere == ' ' || caractere == '\t' || caractere == '\n' || caractere == '\r' ||
        caractere == '\v' || caractere == '\f') {
        return 1;
    }
    return 0;
}
