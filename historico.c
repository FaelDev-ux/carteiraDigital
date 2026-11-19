#include <stdlib.h>
#include "meu_banco.h"

// adiciona uma movimentação (saque 'S', depósito 'D' ou empréstimo 'E') na conta
Resultado registrarMovimentacao(Conta *conta, long long valorCentavos, char operacao) {
    if (conta == NULL || valorCentavos <= 0 ||
        (operacao != 'S' && operacao != 'D' && operacao != 'E')) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    Movimentacao *nova = malloc(sizeof(Movimentacao));
    if (nova == NULL) {
        return ERRO_MEMORIA;
    }
    nova->valorCentavos = valorCentavos;
    nova->operacao = operacao;
    nova->proximo = conta->historico;
    conta->historico = nova;
    return BANCO_OK;
}

// limpa toda a lista de movimentações de uma conta
void liberarMovimentacoes(Movimentacao *historico) {
    while (historico != NULL) {
        Movimentacao *proxima = historico->proximo;
        free(historico);
        historico = proxima;
    }
}

// pega o extrato (lista de movimentações) da conta
const Movimentacao *consultarExtrato(Banco *banco, const char *numeroConta) {
    if (banco == NULL) {
        return NULL;
    }
    banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
    if (numeroConta == NULL) {
        return NULL;
    }
    if (!banco->aberto) {
        banco->ultimoResultado = ERRO_BANCO_FECHADO;
        return NULL;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        banco->ultimoResultado = ERRO_CONTA_NAO_ENCONTRADA;
        return NULL;
    }
    banco->ultimoResultado = BANCO_OK;
    return conta->historico;
}

// calcula a soma de todos os saldos de todas as contas do banco
Resultado consultarSaldoTotal(Banco *banco, long long *total) {
    if (banco == NULL || total == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *total = 0;
    if (!banco->aberto) {
        return ERRO_BANCO_FECHADO;
    }
    for (Cliente *cliente = banco->clientes; cliente != NULL; cliente = cliente->proximo) {
        for (Conta *conta = cliente->contas; conta != NULL; conta = conta->proximo) {
            if (!somaSegura(*total, conta->saldoCentavos, total)) {
                return ERRO_LIMITE_NUMERICO;
            }
        }
    }
    return BANCO_OK;
}
