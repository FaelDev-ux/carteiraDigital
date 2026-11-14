#include <stdlib.h>
#include "meu_banco.h"

Banco *criarBanco(void) {
  Banco *banco = malloc(sizeof(Banco));

  if (banco == NULL) {
    return NULL;
  }

  banco -> clientes = NULL;
  banco -> quantidadeClientes = 0;
  banco -> proximoNumero = 1000;
  banco -> aberto = 0;
  banco -> proximoIdCliente = 1;

  return banco;
}

void liberarBanco(Banco *banco) {
  if (banco == NULL) {
    return; 
  }

  Cliente *clienteAtual = banco -> clientes;

  while (clienteAtual != NULL) {
    Cliente *proximoCliente = clienteAtual -> proximo;
    Conta *contaAtual = clienteAtual -> contas; 
  
    while (contaAtual != NULL) {
      Conta *proximaConta = contaAtual -> proximo;
      Movimentacao *movimentacaoAtual = contaAtual -> historico;

      while (movimentacaoAtual != NULL) {
        Movimentacao *proximaMovimentacao = movimentacaoAtual -> proximo;
        free(movimentacaoAtual);
        movimentacaoAtual = proximaMovimentacao;
      }
      free(contaAtual);
      contaAtual = proximaConta;
    }
    free(clienteAtual -> nome);
    free(clienteAtual);
    clienteAtual = proximoCliente;
  }
  free(banco);

}
Resultado abrirBanco(Banco *banco) {
  if (banco == NULL) {
    return ERRO_ARGUMENTO_INVALIDO;
  }

  banco -> aberto = 1;
  return BANCO_OK;
}
