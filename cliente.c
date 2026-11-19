#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

Cliente *cadastrarCliente(Banco *banco, const char *nome) {
    if (banco == NULL) {
        return NULL;
    }
    banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
    if (nome == NULL || nome[0] == '\0') {
        return NULL;
    }
    /* Não confundimos texto composto só de espaços com nome preenchido. */
    size_t posicao = 0;
    while (nome[posicao] != '\0' && ehEspaco(nome[posicao]) == 1) {
        posicao++;
    }
    if (nome[posicao] == '\0') {
        return NULL;
    }
    if (banco->proximoIdCliente == INT_MAX || banco->quantidadeClientes == INT_MAX) {
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    Cliente *novoCliente = malloc(sizeof(Cliente));
    if (novoCliente == NULL) {
        banco->ultimoResultado = ERRO_MEMORIA;
        return NULL;
    }
    novoCliente->nome = malloc(strlen(nome) + 1);
    if (novoCliente->nome == NULL) {
        free(novoCliente);
        banco->ultimoResultado = ERRO_MEMORIA;
        return NULL;
    }
    strcpy(novoCliente->nome, nome);
    novoCliente->id = banco->proximoIdCliente;
    novoCliente->possuiEmprestimo = 0;
    novoCliente->possuiTitulo = 0;
    novoCliente->contas = NULL;
    novoCliente->proximo = banco->clientes;
    banco->clientes = novoCliente;
    banco->quantidadeClientes++;
    banco->proximoIdCliente++;
    Resultado resultado = salvarBanco(banco);
    if (resultado != BANCO_OK) {
        /* A transação falhou: desfazemos a inclusão também na memória. */
        banco->clientes = novoCliente->proximo;
        banco->quantidadeClientes--;
        banco->proximoIdCliente--;
        free(novoCliente->nome);
        free(novoCliente);
        novoCliente = NULL;
    }
    banco->ultimoResultado = resultado;
    return novoCliente;
}

Cliente *buscarCliente(Banco *banco, int id) {
    if (banco == NULL || id <= 0) {
        return NULL;
    }
    Cliente *clienteAtual = banco->clientes;
    while (clienteAtual != NULL) {
        if (clienteAtual->id == id) {
            return clienteAtual;
        }
        clienteAtual = clienteAtual->proximo;
    }
    return NULL;
}

Cliente *buscarTitular(Banco *banco, const Conta *conta) {
    if (banco == NULL || conta == NULL) {
        return NULL;
    }
    Cliente *clienteAtual = banco->clientes;
    while (clienteAtual != NULL) {
        Conta *contaAtual = clienteAtual->contas;
        while (contaAtual != NULL) {
            if (contaAtual == conta) {
                return clienteAtual;
            }
            contaAtual = contaAtual->proximo;
        }
        clienteAtual = clienteAtual->proximo;
    }
    return NULL;
}

Conta *contaDoTipo(const Cliente *cliente, TipoConta tipo) {
    if (cliente == NULL) {
        return NULL;
    }
    Conta *contaAtual = cliente->contas;
    while (contaAtual != NULL) {
        if (contaAtual->tipo == tipo) {
            return contaAtual;
        }
        contaAtual = contaAtual->proximo;
    }
    return NULL;
}

int clienteComCorrenteNegativa(const Cliente *cliente) {
    Conta *corrente = contaDoTipo(cliente, CORRENTE);
    if (corrente != NULL && corrente->saldoCentavos < 0) {
        return 1;
    }
    return 0;
}
