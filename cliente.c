#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

// cadastra um novo cliente, valida nome e persiste no banco de dados
Cliente *cadastrarCliente(Banco *banco, const char *nome) {
    if (banco == NULL) {
        return NULL;
    }
    banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
    if (nome == NULL || *nome == '\0') {
        return NULL;
    }
    const char *caractere = nome;
    while (*caractere != '\0' && ehEspaco(*caractere)) {
        caractere++;
    }
    if (*caractere == '\0') {
        return NULL;
    }
    if (banco->proximoIdCliente == INT_MAX || banco->quantidadeClientes == INT_MAX) {
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    Cliente *novoCliente = calloc(1, sizeof(Cliente));
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
    novoCliente->id = banco->proximoIdCliente++;
    novoCliente->proximo = banco->clientes;
    banco->clientes = novoCliente;
    banco->quantidadeClientes++;
    Resultado resultado = salvarBanco(banco);
    if (resultado != BANCO_OK) {
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

// busca um cliente pelo id
Cliente *buscarCliente(Banco *banco, int id) {
    if (banco == NULL || id <= 0) {
        return NULL;
    }
    for (Cliente *cliente = banco->clientes; cliente != NULL; cliente = cliente->proximo) {
        if (cliente->id == id) {
            return cliente;
        }
    }
    return NULL;
}

// acha o cliente dono de uma conta
Cliente *buscarTitular(Banco *banco, const Conta *conta) {
    if (banco == NULL || conta == NULL) {
        return NULL;
    }
    for (Cliente *cliente = banco->clientes; cliente != NULL; cliente = cliente->proximo) {
        for (Conta *contaItem = cliente->contas; contaItem != NULL; contaItem = contaItem->proximo) {
            if (contaItem == conta) {
                return cliente;
            }
        }
    }
    return NULL;
}

// retorna a conta de um tipo específico do cliente (corrente, salario ou poupanca)
Conta *contaDoTipo(const Cliente *cliente, TipoConta tipo) {
    if (cliente == NULL) {
        return NULL;
    }
    for (Conta *conta = cliente->contas; conta != NULL; conta = conta->proximo) {
        if (conta->tipo == tipo) {
            return conta;
        }
    }
    return NULL;
}

// checa se o cliente está com a conta corrente com saldo negativo
int clienteComCorrenteNegativa(const Cliente *cliente) {
    Conta *corrente = contaDoTipo(cliente, CORRENTE);
    return (corrente != NULL && corrente->saldoCentavos < 0);
}
