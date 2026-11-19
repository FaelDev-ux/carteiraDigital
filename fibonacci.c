#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include "fibonacci.h"

// checa se o valor é par
int ehPar(long long valor) {
    return valor % 2 == 0;
}

// checa se o valor é primo
int ehPrimo(long long valor) {
    if (valor < 2) {
        return 0;
    }
    if (valor % 2 == 0) {
        return valor == 2;
    }
    for (long long divisor = 3; divisor <= valor / divisor; divisor += 2) {
        if (valor % divisor == 0) {
            return 0;
        }
    }
    return 1;
}

// soma os números da sequência de fibonacci estritamente menores que o limite
Resultado somaFibonacci(long long limite, long long *soma) {
    if (soma == NULL || limite < 0) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    long long a = 0, b = 1, c = 0;
    *soma = (limite > 1) ? 1 : 0;
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

// libera a memória das pontuações calculadas
void liberarClassificacaoFibonacci(ClassificacaoFibonacci *classificacao) {
    if (classificacao == NULL) {
        return;
    }
    free(classificacao->pontuacoes);
    *classificacao = (ClassificacaoFibonacci){ .maiorPrimo = -1, .maiorPar = -1, .primeiraAcimaMedia = -1 };
}

// calcula as pontuações e identifica o maior primo, maior par e primeira acima da média
Resultado classificarFibonacci(const long long *valores, int quantidade, ClassificacaoFibonacci *saida) {
    if (saida == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *saida = (ClassificacaoFibonacci){ .maiorPrimo = -1, .maiorPar = -1, .primeiraAcimaMedia = -1 };
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
        if ((saida->maiorPrimo == -1 || pontos != saida->pontuacoes[saida->maiorPrimo]) &&
            (saida->maiorPar == -1 || pontos != saida->pontuacoes[saida->maiorPar]) &&
            pontos > media) {
            saida->primeiraAcimaMedia = i;
            break;
        }
    }
    return BANCO_OK;
}

// decide qual dos 5 participantes ganhou pelo critério: primo > par > acima da média
Resultado escolherPorFibonacci(const Participante *participantes, int quantidade, int *indice) {
    if (indice == NULL || participantes == NULL || quantidade != 5) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    *indice = -1;
    long long valores[5];
    const Participante *participante = participantes;
    for (int i = 0; i < quantidade; i++, participante = participante->proximo) {
        if (participante == NULL || participante->conta == NULL) {
            return ERRO_ARGUMENTO_INVALIDO;
        }
        valores[i] = participante->conta->saldoCentavos;
    }
    ClassificacaoFibonacci classificacao;
    Resultado resultado = classificarFibonacci(valores, quantidade, &classificacao);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    if (classificacao.maiorPrimo >= 0) {
        *indice = classificacao.maiorPrimo;
    } else if (classificacao.maiorPar >= 0) {
        *indice = classificacao.maiorPar;
    } else if (classificacao.primeiraAcimaMedia >= 0) {
        *indice = classificacao.primeiraAcimaMedia;
    } else {
        *indice = 0;
    }
    liberarClassificacaoFibonacci(&classificacao);
    return BANCO_OK;
}
