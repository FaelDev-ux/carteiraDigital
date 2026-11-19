#ifndef MEU_BANCO_H
#define MEU_BANCO_H
#include <stddef.h>

/* Valores monetários são inteiros em centavos. Cada lista pertence ao banco. */
typedef enum { CORRENTE = 1, SALARIO = 2, POUPANCA = 3 } TipoConta;
typedef enum {
    BANCO_OK = 0,
    ERRO_ARGUMENTO_INVALIDO = 1,
    ERRO_BANCO_FECHADO = 2,
    ERRO_MEMORIA = 3,
    ERRO_CONTA_NAO_ENCONTRADA = 4,
    ERRO_LIMITE_NUMERICO = 5,
    ERRO_SALDO_INSUFICIENTE = 6,
    ERRO_COMBINACAO_CONTAS = 7,
    ERRO_CLIENTE_NAO_ENCONTRADO = 8,
    ERRO_EMPRESTIMO_NAO_PERMITIDO = 9,
    ERRO_TITULO_NAO_PERMITIDO = 10,
    ERRO_PARTICIPANTES_INSUFICIENTES = 11,
    ERRO_PERSISTENCIA = 12
} Resultado;

typedef struct Movimentacao {
    long long valorCentavos;
    char operacao;
    struct Movimentacao *proximo;
} Movimentacao;

typedef struct Conta {
    int numeroBase;
    TipoConta tipo;
    long long saldoCentavos;
    Movimentacao *historico; /* Mais recente primeiro; não apagamos as antigas. */
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

typedef struct Premiacao {
    long long id;
    Cliente *cliente;
    Conta *conta;
    long long saldoBaseCentavos;
    long long valorCentavos;
    struct Premiacao *proximo;
} Premiacao;

typedef struct Participante {
    Cliente *cliente;
    Conta *conta;
    struct Participante *proximo;
} Participante;

struct sqlite3;
typedef struct Banco {
    Cliente *clientes;
    int quantidadeClientes;
    int proximoIdCliente;
    int proximoNumero;
    int aberto;
    Premiacao *premiacoes;
    long long proximoSorteio;
    long long revisao; /* Detecta alterações feitas por outra instância do programa. */
    struct sqlite3 *conexao;
    Resultado ultimoResultado; /* Detalha falhas das funções que retornam ponteiro. */
} Banco;

Banco *criarBanco(void);
void liberarBanco(Banco *banco);
Resultado abrirBanco(Banco *banco);
const char *mensagemResultado(Resultado resultado);
Cliente *cadastrarCliente(Banco *banco, const char *nome);
Cliente *buscarCliente(Banco *banco, int id);
Cliente *buscarTitular(Banco *banco, const Conta *conta);
int clienteComCorrenteNegativa(const Cliente *cliente);
Conta *contaDoTipo(const Cliente *cliente, TipoConta tipo);
Conta *criarConta(Banco *banco, int clienteId, TipoConta tipo);
Conta *buscarConta(Banco *banco, const char *numeroConta);
Resultado registrarMovimentacao(Conta *conta, long long valorCentavos, char operacao);
void liberarMovimentacoes(Movimentacao *historico);
Resultado depositar(Banco *banco, const char *numeroConta, long long valorCentavos);
Resultado sacar(Banco *banco, const char *numeroConta, long long valorCentavos);
Resultado consultarLimiteEmprestimo(Banco *banco, int clienteId, long long *limite);
Resultado solicitarEmprestimo(Banco *banco, const char *numeroConta, long long valorCentavos);
Resultado adquirirTitulo(Banco *banco, const char *numeroConta);
/* Consulta a lista da conta; a interface mostra até cinco registros. */
const Movimentacao *consultarExtrato(Banco *banco, const char *numeroConta);
Resultado consultarSaldoTotal(Banco *banco, long long *total);
Participante *selecionarParticipantes(Banco *banco);
void liberarParticipantes(Participante *participantes);
Premiacao *realizarSorteio(Banco *banco);
Resultado escolherPorFibonacci(const Participante *participantes, int quantidade, int *indice);

/* Persistência SQLite: implementação em database.c. */
Resultado abrirPersistencia(Banco *banco, const char *caminho);
Resultado salvarBanco(Banco *banco);

/* Auxiliares usados apenas entre funções do backend. */
Resultado concluirAlteracao(Banco *banco, Conta *conta, long long saldoAnterior,
                            Movimentacao *historicoAnterior);
int somaSegura(long long a, long long b, long long *resultado);
int aptoTitulo(const Cliente *cliente);
int ehEspaco(char caractere);
#endif
