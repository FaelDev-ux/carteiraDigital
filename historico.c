#include <stdlib.h>
#include "meu_banco.h"

Resultado registrarMovimentacao(Conta *conta, long long valorCentavos, char operacao) {
    if (conta == NULL || valorCentavos <= 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (operacao != 'S' && operacao != 'D' && operacao != 'E') {
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

void liberarMovimentacoes(Movimentacao *historico) {
    while (historico != NULL) {
        Movimentacao *proxima = historico->proximo;
        free(historico);
        historico = proxima;
    }
}

/* Retorna a lista existente. A interface exibe até cinco registros.
   Quem consulta não deve liberar os registros: eles pertencem à conta. */
const Movimentacao *consultarExtrato(Banco *banco, const char *numeroConta) {
    if (banco == NULL) {
        return NULL;
    }
    banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
    if (numeroConta == NULL) {
        return NULL;
    }
    if (banco->aberto == 0) {
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

/* A regra calcula o total; apenas a apresentação fica na main.c. */
Resultado consultarSaldoTotal(Banco *banco, long long *total) {
    if (banco == NULL || total == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *total = 0;
    if (banco->aberto == 0) {
        return ERRO_BANCO_FECHADO;
    }
    Cliente *clienteAtual = banco->clientes;
    while (clienteAtual != NULL) {
        Conta *contaAtual = clienteAtual->contas;
        while (contaAtual != NULL) {
            if (somaSegura(*total, contaAtual->saldoCentavos, total) == 0) {
                return ERRO_LIMITE_NUMERICO;
            }
            contaAtual = contaAtual->proximo;
        }
        clienteAtual = clienteAtual->proximo;
    }
    return BANCO_OK;
}
