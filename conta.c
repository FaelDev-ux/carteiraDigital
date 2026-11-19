#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

Conta *criarConta(Banco *banco, int clienteId, TipoConta tipo) {
    if (banco == NULL) {
        return NULL;
    }
    banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
    if (tipo != CORRENTE && tipo != SALARIO && tipo != POUPANCA) {
        return NULL;
    }
    Cliente *titular = buscarCliente(banco, clienteId);
    if (titular == NULL) {
        banco->ultimoResultado = ERRO_CLIENTE_NAO_ENCONTRADO;
        return NULL;
    }
    for (Conta *a = titular->contas; a != NULL; a = a->proximo) {
        if (a->tipo == tipo || (a->tipo == CORRENTE && tipo == SALARIO) ||
            (a->tipo == SALARIO && tipo == CORRENTE)) {
            banco->ultimoResultado = ERRO_COMBINACAO_CONTAS;
            return NULL;
        }
    }
    if (banco->proximoNumero < 1000 || banco->proximoNumero > 9999) {
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    Conta *nova = malloc(sizeof(Conta));
    if (nova == NULL) {
        banco->ultimoResultado = ERRO_MEMORIA;
        return NULL;
    }
    nova->numeroBase = banco->proximoNumero;
    nova->tipo = tipo;
    nova->saldoCentavos = 0;
    nova->historico = NULL;
    nova->proximo = titular->contas;
    titular->contas = nova;
    banco->proximoNumero++;
    Resultado resultado = salvarBanco(banco);
    if (resultado != BANCO_OK) {
        titular->contas = nova->proximo;
        banco->proximoNumero--;
        free(nova);
        nova = NULL;
    }
    banco->ultimoResultado = resultado;
    return nova;
}

Conta *buscarConta(Banco *banco, const char *numeroConta) {
    if (banco == NULL || numeroConta == NULL || strlen(numeroConta) != 7 || numeroConta[4] != '-') {
        return NULL;
    }
    for (int i = 0; i < 7; i++) {
        if (i != 4 && (numeroConta[i] < '0' || numeroConta[i] > '9')) {
            return NULL;
        }
    }
    int numeroBase = 0, codigoTipo = 0;
    for (int i = 0; i < 4; i++) {
        numeroBase = numeroBase * 10 + numeroConta[i] - '0';
    }
    for (int i = 5; i < 7; i++) {
        codigoTipo = codigoTipo * 10 + numeroConta[i] - '0';
    }
    if (numeroBase < 1000 || codigoTipo < CORRENTE || codigoTipo > POUPANCA) {
        return NULL;
    }
    TipoConta tipo = (TipoConta)codigoTipo;
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        for (Conta *a = c->contas; a != NULL; a = a->proximo) {
            if (a->numeroBase == numeroBase && a->tipo == tipo) {
                return a;
            }
        }
    }
    return NULL;
}

/* Cada operação primeiro modifica a memória e tenta persistir numa transação.
   Se a gravação falhar, desfazemos saldo e novas movimentações. */
Resultado concluirAlteracao(Banco *banco, Conta *conta, long long saldoAnterior,
    Movimentacao *historicoAnterior) {
    Resultado resultado = salvarBanco(banco);
    if (resultado != BANCO_OK) {
        conta->saldoCentavos = saldoAnterior;
        while (conta->historico != historicoAnterior) {
            Movimentacao *nova = conta->historico;
            conta->historico = nova->proximo;
            free(nova);
        }
    }
    return resultado;
}

Resultado depositar(Banco *banco, const char *numeroConta, long long valorCentavos) {
    if (banco == NULL || numeroConta == NULL || valorCentavos <= 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (banco->aberto == 0) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    Resultado resultado;
    long long saldoNovo;
    if (!somaSegura(conta->saldoCentavos, valorCentavos, &saldoNovo)) {
        return ERRO_LIMITE_NUMERICO;
    }
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    resultado = registrarMovimentacao(conta, valorCentavos, 'D');
    if (resultado != BANCO_OK) {
        return resultado;
    }
    conta->saldoCentavos = saldoNovo;
    return concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
}

Resultado sacar(Banco *banco, const char *numeroConta, long long valorCentavos) {
    if (banco == NULL || numeroConta == NULL || valorCentavos <= 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (banco->aberto == 0) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    Resultado resultado;
    /* Decisão final do projeto: nenhuma conta pode sacar além do saldo. */
    if (valorCentavos > conta->saldoCentavos) {
        return ERRO_SALDO_INSUFICIENTE;
    }
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    resultado = registrarMovimentacao(conta, valorCentavos, 'S');
    if (resultado != BANCO_OK) {
        return resultado;
    }
    conta->saldoCentavos -= valorCentavos;
    return concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
}
