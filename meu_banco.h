#ifndef MEU_BANCO_H
#define MEU_BANCO_H

typedef enum {
  CORRENTE = 1,
  SALARIO = 2,
  POUPANCA = 3
} TipoConta;

typedef struct Movimentacao{
  long long valorCentavos;
  char operacao;

  struct Movimentacao *proximo;
} Movimentacao;

typedef struct Conta {
  int numeroBase;
  TipoConta tipo;
  long long saldoCentavos;
  Movimentacao *historico;

  struct Conta *proximo;
} Conta;

typedef struct Cliente {
  int id;
  char *nome;
  int possuiEmprestimo;
  int possuiTitulo;
  Conta *contas;

  struct Cliente *proximo;
} Cliente;

  typedef struct {
    Cliente *clientes;
    int quantidadeClientes;
    int proximoNumero;
    int aberto;
  } Banco;

  Banco *criarBanco(void);
#endif