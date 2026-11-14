#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

Cliente *cadastrarCliente(Banco *banco, const char *nome) {
  if (banco == NULL || nome == NULL) {
    return NULL;
  }
  if (nome[0] == '\0') {
    return NULL;
  }

  Cliente *novoCliente = malloc(sizeof(Cliente));
  if (novoCliente == NULL) {
    return NULL;
  }
  
  novoCliente -> nome = malloc(strlen(nome) + 1);
  if (novoCliente -> nome == NULL) {
    free (novoCliente);
    return NULL;
  }

  strcpy(novoCliente -> nome, nome);

  novoCliente -> possuiEmprestimo = 0;
  novoCliente -> possuiTitulo = 0;
  novoCliente -> contas = NULL;
  novoCliente -> id = banco -> proximoIdCliente;
  novoCliente -> proximo = banco -> clientes;
  banco -> clientes = novoCliente;
  banco -> quantidadeClientes ++;
  banco -> proximoIdCliente ++;
  return novoCliente;
}

Cliente *buscarCliente(Banco *banco, int id) {
  if (banco == NULL || id <= 0) {
    return NULL;
  }

  Cliente *clienteAtual = banco -> clientes;

  while (clienteAtual != NULL) {
    if (clienteAtual -> id == id) {
      return clienteAtual;
    }
    clienteAtual = clienteAtual -> proximo;
  }
  return NULL;
}