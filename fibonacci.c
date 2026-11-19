#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "fibonacci.h"

int ehPar(long long valor) {
    return valor % 2 == 0;
}

int ehPrimo(long long valor) {
    if (valor < 2) {
        return 0;
    }
    if (valor % 2 == 0) {
        return valor == 2;
    }
    /* Divisão no limite evita o estouro de divisor * divisor do original. */
    for (long long divisor = 3; divisor <= valor / divisor; divisor += 2) {
        if (valor % divisor == 0) {
            return 0;
        }
    }
    return 1;
}

Resultado somaFibonacci(long long limite, long long *soma) {
    if (soma == NULL || limite < 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    long long a = 0, b = 1, c = 0;
    if (limite > 1) {
        *soma = 1;
    } else {
        *soma = 0;
    }
    /* Preserva a aula: soma os termos estritamente menores que o limite,
       incluindo as duas ocorrências de 1. Não é a soma de N termos. */
    while (c < limite) {
        if (a > LLONG_MAX - b) {
            break;
        }
        c = a + b;
        if (c < limite) {
            if (*soma > LLONG_MAX - c) {
                return ERRO_LIMITE_NUMERICO;
            }
            *soma += c;
        }
        a = b;
        b = c;
    }
    return BANCO_OK;
}

void liberarClassificacaoFibonacci(ClassificacaoFibonacci *c) {
    if (c == NULL) {
        return;
    }
    free(c->pontuacoes);
    c->pontuacoes = NULL;
    c->quantidade = 0;
    c->maiorPrimo = -1;
    c->maiorPar = -1;
    c->primeiraAcimaMedia = -1;
}

Resultado classificarFibonacci(const long long *valores, int quantidade, ClassificacaoFibonacci *saida) {
    if (saida == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    saida->pontuacoes = NULL;
    saida->quantidade = 0;
    saida->maiorPrimo = -1;
    saida->maiorPar = -1;
    saida->primeiraAcimaMedia = -1;
    if (valores == NULL || quantidade <= 0 ||
        (size_t)quantidade > SIZE_MAX / sizeof *saida->pontuacoes) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    saida->pontuacoes = malloc((size_t)quantidade * sizeof *saida->pontuacoes);
    if (saida->pontuacoes == NULL) {
        return ERRO_MEMORIA;
    }
    saida->quantidade = quantidade;
    long long media = 0, restos = 0;
    for (int i = 0; i < quantidade; i++) {
        long long soma = 0;
        Resultado resultado = somaFibonacci(valores[i], &soma);
        if (resultado != BANCO_OK) {
            liberarClassificacaoFibonacci(saida);
            return resultado;
        }
        /* Alteração solicitada: somente o fatorial foi removido da fórmula.
           Divisão inteira preserva o truncamento do resultado inteiro da aula. */
        long long pontos = soma / 3;
        saida->pontuacoes[i] = pontos;
        media += pontos / quantidade;
        restos += pontos % quantidade;
        if (ehPrimo(pontos) &&
            (saida->maiorPrimo == -1 || pontos > saida->pontuacoes[saida->maiorPrimo])) {
            saida->maiorPrimo = i;
        }
        if (ehPar(pontos) &&
            (saida->maiorPar == -1 || pontos > saida->pontuacoes[saida->maiorPar])) {
            saida->maiorPar = i;
        }
    }
    media += restos / quantidade;
    for (int i = 0; i < quantidade; i++) {
        long long pontos = saida->pontuacoes[i];
        /* Nunca acessamos posição -1 quando uma categoria não tem vencedor.
           Preservamos a comparação por VALOR do código original. */
        if ((saida->maiorPrimo == -1 || pontos != saida->pontuacoes[saida->maiorPrimo]) &&
            (saida->maiorPar == -1 || pontos != saida->pontuacoes[saida->maiorPar]) &&
            pontos > media) {
            saida->primeiraAcimaMedia = i;
            break;
        }
    }
    return BANCO_OK;
}

Resultado escolherPorFibonacci(const Participante *participantes, int quantidade, int *indice) {
    /* O código da aula classifica os valores sem gerar números aleatórios. */
    if (indice == NULL || participantes == NULL || quantidade != 5) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *indice = -1;
    long long *valores = malloc((size_t)quantidade * sizeof *valores);
    if (valores == NULL) {
        return ERRO_MEMORIA;
    }
    const Participante *p = participantes;
    for (int i = 0; i < quantidade; i++, p = p->proximo) {
        if (p == NULL || p->conta == NULL) {
            free(valores);
            return ERRO_ARGUMENTO_INVALIDO;
        }
        valores[i] = p->conta->saldoCentavos;
    }
    ClassificacaoFibonacci classificacao;
    Resultado resultado = classificarFibonacci(valores, quantidade, &classificacao);
    free(valores);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    /* A carteira concede um único prêmio: usa a melhor categoria disponível.
       Empates ficam com o primeiro da lista, já ordenada por saldo e conta. */
    if (classificacao.maiorPrimo >= 0) {
        *indice = classificacao.maiorPrimo;
    } else if (classificacao.maiorPar >= 0) {
        *indice = classificacao.maiorPar;
    } else if (classificacao.primeiraAcimaMedia >= 0) {
        *indice = classificacao.primeiraAcimaMedia;
    } else {
        *indice = 0; /* Empate geral: primeiro da classificação decrescente. */
    }
    liberarClassificacaoFibonacci(&classificacao);
    return BANCO_OK;
}
