#include <limits.h>
#include <stdlib.h>
#include "meu_banco.h"

int aptoTitulo(const Cliente *cliente) {
    Conta *salario = contaDoTipo(cliente, SALARIO);
    if (cliente == NULL || salario == NULL) {
        return 0;
    }
    if (cliente->possuiTitulo == 0 && salario->saldoCentavos >= 100000 &&
        salario->saldoCentavos <= 150000) {
        return 1;
    }
    return 0;
}

Resultado adquirirTitulo(Banco *banco, const char *numeroConta) {
    if (banco == NULL || numeroConta == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (banco->aberto == 0) {
        return ERRO_BANCO_FECHADO;
    }
    Conta *conta = buscarConta(banco, numeroConta);
    if (conta == NULL) {
        return ERRO_CONTA_NAO_ENCONTRADA;
    }
    Cliente *cliente = buscarTitular(banco, conta);
    if (conta->tipo != SALARIO || aptoTitulo(cliente) == 0) {
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

void liberarParticipantes(Participante *participante) {
    while (participante != NULL) {
        Participante *proximo = participante->proximo;
        free(participante);
        participante = proximo;
    }
}

Participante *selecionarParticipantes(Banco *banco) {
    if (banco == NULL) {
        return NULL;
    }
    if (banco->aberto == 0) {
        banco->ultimoResultado = ERRO_BANCO_FECHADO;
        return NULL;
    }
    Participante *inicio = NULL;
    Cliente *clienteAtual = banco->clientes;
    while (clienteAtual != NULL) {
        Conta *salario = contaDoTipo(clienteAtual, SALARIO);
        if (clienteAtual->possuiTitulo == 1 && salario != NULL &&
            salario->saldoCentavos >= 100000 && salario->saldoCentavos <= 150000) {
            Participante *novo = malloc(sizeof(Participante));
            if (novo == NULL) {
                liberarParticipantes(inicio);
                banco->ultimoResultado = ERRO_MEMORIA;
                return NULL;
            }
            novo->cliente = clienteAtual;
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
        clienteAtual = clienteAtual->proximo;
    }
    banco->ultimoResultado = BANCO_OK;
    return inicio;
}

Premiacao *realizarSorteio(Banco *banco) {
    if (banco == NULL) {
        return NULL;
    }
    Participante *lista = selecionarParticipantes(banco);
    if (banco->ultimoResultado != BANCO_OK) {
        return NULL;
    }
    int quantidade = 0;
    Participante *participante = lista;
    while (participante != NULL) {
        quantidade++;
        participante = participante->proximo;
    }
    if (quantidade != 5) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_PARTICIPANTES_INSUFICIENTES;
        return NULL;
    }
    int indice = -1;
    Resultado resultado = escolherPorFibonacci(lista, quantidade, &indice);
    if (resultado != BANCO_OK) {
        liberarParticipantes(lista);
        banco->ultimoResultado = resultado;
        return NULL;
    }
    if (indice < 0 || indice >= quantidade) {
        liberarParticipantes(lista);
        banco->ultimoResultado = ERRO_ARGUMENTO_INVALIDO;
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
