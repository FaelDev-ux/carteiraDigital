#include <limits.h>
#include <stdlib.h>
#include "meu_banco.h"

// checa se o cliente pode comprar título (conta salário com saldo entre 1k e 1.5k)
int aptoTitulo(const Cliente *cliente) {
    if (cliente == NULL) {
        return 0;
    }
    Conta *salario = contaDoTipo(cliente, SALARIO);
    return (salario != NULL && !cliente->possuiTitulo &&
            salario->saldoCentavos >= 100000 && salario->saldoCentavos <= 150000);
}

// registra a compra do título de capitalização pro cliente
Resultado adquirirTitulo(Banco *banco, const char *numeroConta) {
    if (banco == NULL || numeroConta == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (!banco->aberto) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    Cliente *cliente = buscarTitular(banco, conta);
    if (conta->tipo != SALARIO || !aptoTitulo(cliente)) {
        return ERRO_TITULO_NAO_PERMITIDO;
    }
    /* O enunciado não fixa preço: apenas registramos um título por cliente. */
    cliente->possuiTitulo = 1;
    Resultado resultado = salvarBanco(banco);
    if (resultado != BANCO_OK) {
        cliente->possuiTitulo = 0;
    }
    return resultado;
}

// limpa a lista temporária de participantes do sorteio
void liberarParticipantes(Participante *participante) {
    while (participante != NULL) {
        Participante *proximo = participante->proximo;
        free(participante);
        participante = proximo;
    }
}

// busca e ordena os 5 clientes com título com os maiores saldos para o sorteio
Participante *selecionarParticipantes(Banco *banco) {
    if (banco == NULL) {
        return NULL;
    }
    if (!banco->aberto) {
        banco->ultimoResultado = ERRO_BANCO_FECHADO;
        return NULL;
    }
    Participante *inicio = NULL;
    for (Cliente *cliente = banco->clientes; cliente != NULL; cliente = cliente->proximo) {
        Conta *salario = contaDoTipo(cliente, SALARIO);
        if (cliente->possuiTitulo == 1 && salario != NULL &&
            salario->saldoCentavos >= 100000 && salario->saldoCentavos <= 150000) {
            Participante *novo = malloc(sizeof(Participante));
            if (novo == NULL) {
                liberarParticipantes(inicio);
                banco->ultimoResultado = ERRO_MEMORIA;
                return NULL;
            }
            novo->cliente = cliente;
            novo->conta = salario;
            novo->proximo = NULL;
            /* Inserção ordenada: maior saldo primeiro; empate pelo menor número. */
            Participante *anterior = NULL;
            Participante *atual = inicio;
            while (atual != NULL && (atual->conta->saldoCentavos > salario->saldoCentavos ||
                                     (atual->conta->saldoCentavos == salario->saldoCentavos &&
                                      atual->conta->numeroBase < salario->numeroBase))) {
                anterior = atual;
                atual = atual->proximo;
            }
            novo->proximo = atual;
            if (anterior == NULL) {
                inicio = novo;
            } else {
                anterior->proximo = novo;
            }
            /* A lista temporária conserva somente os cinco melhores. */
            Participante *ultimo = inicio;
            int quantidade = 1;
            while (ultimo != NULL && quantidade < 5) {
                ultimo = ultimo->proximo;
                quantidade++;
            }
            if (ultimo != NULL) {
                liberarParticipantes(ultimo->proximo);
                ultimo->proximo = NULL;
            }
        }
    }
    banco->ultimoResultado = BANCO_OK;
    return inicio;
}

// realiza o sorteio via fibonacci entre os 5 participantes e credita 10x o saldo
Premiacao *realizarSorteio(Banco *banco) {
    if (banco == NULL) {
        return NULL;
    }
    Participante *lista = selecionarParticipantes(banco);
    if (banco->ultimoResultado != BANCO_OK) {
        return NULL;
    }
    int quantidade = 0;
    for (Participante *participante = lista; participante != NULL; participante = participante->proximo) {
        quantidade++;
    }
    if (quantidade != 5) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_PARTICIPANTES_INSUFICIENTES;
        return NULL;
    }
    int indice = -1;
    Resultado resultado = escolherPorFibonacci(lista, quantidade, &indice);
    if (resultado != BANCO_OK || indice < 0 || indice >= quantidade) {
        liberarParticipantes(lista);
        banco->ultimoResultado = (resultado != BANCO_OK) ? resultado : ERRO_ARGUMENTO_INVALIDO;
        return NULL;
    }
    Participante *vencedor = lista;
    for (int i = 0; i < indice; i++) {
        vencedor = vencedor->proximo;
    }
    Conta *conta = vencedor->conta;
    if (conta->saldoCentavos > LLONG_MAX / 10 || banco->proximoSorteio == LLONG_MAX) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    long long premio = conta->saldoCentavos * 10;
    long long saldoNovo;
    if (somaSegura(conta->saldoCentavos, premio, &saldoNovo) == 0) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_LIMITE_NUMERICO;
        return NULL;
    }
    Premiacao *nova = malloc(sizeof(Premiacao));
    if (nova == NULL) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_MEMORIA;
        return NULL;
    }
    nova->id = banco->proximoSorteio;
    nova->cliente = vencedor->cliente;
    nova->conta = conta;
    nova->saldoBaseCentavos = conta->saldoCentavos;
    nova->valorCentavos = premio;
    nova->proximo = banco->premiacoes;
    long long saldoAnterior = conta->saldoCentavos;
    Movimentacao *historicoAnterior = conta->historico;
    /* O crédito do prêmio entra como depósito; a premiação identifica sua origem. */
    resultado = registrarMovimentacao(conta, premio, 'D');
    if (resultado != BANCO_OK) {
        free(nova);
        liberarParticipantes(lista);
        banco->ultimoResultado = resultado;
        return NULL;
    }
    conta->saldoCentavos = saldoNovo;
    banco->premiacoes = nova;
    banco->proximoSorteio++;
    resultado = concluirAlteracao(banco, conta, saldoAnterior, historicoAnterior);
    if (resultado != BANCO_OK) {
        banco->premiacoes = nova->proximo;
        banco->proximoSorteio--;
        free(nova);
        nova = NULL;
    }
    liberarParticipantes(lista);
    banco->ultimoResultado = resultado;
    return nova;
}
