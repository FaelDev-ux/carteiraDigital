#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

// cria uma nova conta pro cliente respeitando as regras de tipos permitidos
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
    for (Conta *conta = titular->contas; conta != NULL; conta = conta->proximo) {
        if (conta->tipo == tipo || (conta->tipo == CORRENTE && tipo == SALARIO) ||
            (conta->tipo == SALARIO && tipo == CORRENTE)) {
            banco->ultimoResultado = ERRO_COMBINACAO_CONTAS;
            return NULL;
        }
    }
    if (banco->proximoNumero < 1000 || banco->proximoNumero > 9999) {
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    Conta *nova = calloc(1, sizeof(Conta));
    if (nova == NULL) {
        banco->ultimoResultado = ERRO_MEMORIA;
        return NULL;
    }
    nova->numeroBase = banco->proximoNumero++;
    nova->tipo = tipo;
    nova->proximo = titular->contas;
    titular->contas = nova;
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

// busca uma conta pelo formato "1000-01" (número base e tipo)
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
    for (Cliente *cliente = banco->clientes; cliente != NULL; cliente = cliente->proximo) {
        for (Conta *conta = cliente->contas; conta != NULL; conta = conta->proximo) {
            if (conta->numeroBase == numeroBase && conta->tipo == tipo) {
                return conta;
            }
        }
    }
    return NULL;
}

// salva as mudanças no sqlite; se der erro, desfaz o saldo e o histórico novo
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

// realiza depósito na conta e registra no histórico
Resultado depositar(Banco *banco, const char *numeroConta, long long valorCentavos) {
    if (banco == NULL || numeroConta == NULL || valorCentavos <= 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (!banco->aberto) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    long long saldoNovo;
    if (!somaSegura(conta->saldoCentavos, valorCentavos, &saldoNovo)) {
        return ERRO_LIMITE_NUMERICO;
    }
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    Resultado resultado = registrarMovimentacao(conta, valorCentavos, 'D');
    if (resultado != BANCO_OK) {
        return resultado;
    }
    conta->saldoCentavos = saldoNovo;
    return concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
}

// faz saque na conta se tiver saldo disponível
Resultado sacar(Banco *banco, const char *numeroConta, long long valorCentavos) {
    if (banco == NULL || numeroConta == NULL || valorCentavos <= 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (!banco->aberto) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    /* Decisão final do projeto: nenhuma conta pode sacar além do saldo. */
    if (valorCentavos > conta->saldoCentavos) {
        return ERRO_SALDO_INSUFICIENTE;
    }
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    Resultado resultado = registrarMovimentacao(conta, valorCentavos, 'S');
    if (resultado != BANCO_OK) {
        return resultado;
    }
    conta->saldoCentavos -= valorCentavos;
    return concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
}
