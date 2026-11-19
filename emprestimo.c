#include <limits.h>
#include "meu_banco.h"

Resultado consultarLimiteEmprestimo(Banco *banco, int clienteId, long long *limite) {
    if (banco == NULL || limite == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *limite = 0;
    if (!banco->aberto) {
        return ERRO_BANCO_FECHADO;
    }
    Cliente *cliente = buscarCliente(banco, clienteId);
    if (cliente == NULL) {
        return ERRO_CLIENTE_NAO_ENCONTRADO;
    }
    Conta *corrente = contaDoTipo(cliente, CORRENTE);
    Conta *poupanca = contaDoTipo(cliente, POUPANCA);
    if (cliente->possuiEmprestimo || corrente == NULL || poupanca == NULL ||
        poupanca->saldoCentavos <= 0 || corrente->saldoCentavos <= 0) {
        return ERRO_EMPRESTIMO_NAO_PERMITIDO;
    }
    /* Interpretação combinada: base = saldo da corrente.
       25% de dez vezes equivale a 5/2; arredondamos centavos para baixo.
       Dividir antes de multiplicar evita estouro intermediário. */
    long long metade = corrente->saldoCentavos / 2;
    int resto = (int)(corrente->saldoCentavos % 2);
    if (metade > LLONG_MAX / 5) {
        return ERRO_LIMITE_NUMERICO;
    }
    long long calculado = metade * 5;
    if (resto && calculado > LLONG_MAX - 2) {
        return ERRO_LIMITE_NUMERICO;
    }
    *limite = calculado + resto * 2;
    return BANCO_OK;
}

Resultado solicitarEmprestimo(Banco *banco, const char *numeroConta, long long valorCentavos) {
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
    if (conta->tipo != CORRENTE) {
        return ERRO_EMPRESTIMO_NAO_PERMITIDO;
    }
    Cliente *cliente = buscarTitular(banco, conta);
    long long limite = 0;
    resultado = consultarLimiteEmprestimo(banco, cliente->id, &limite);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    if (valorCentavos > limite) {
        return ERRO_EMPRESTIMO_NAO_PERMITIDO;
    }
    long long saldoNovo;
    if (!somaSegura(conta->saldoCentavos, valorCentavos, &saldoNovo)) {
        return ERRO_LIMITE_NUMERICO;
    }
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    resultado = registrarMovimentacao(conta, valorCentavos, 'E');
    if (resultado != BANCO_OK) {
        return resultado;
    }
    conta->saldoCentavos = saldoNovo;
    cliente->possuiEmprestimo = 1;
    resultado = concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
    if (resultado != BANCO_OK) {
        cliente->possuiEmprestimo = 0;
    }
    return resultado;
}
