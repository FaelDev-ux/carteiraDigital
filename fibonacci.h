#ifndef FIBONACCI_H
#define FIBONACCI_H
#include "meu_banco.h"

typedef struct ClassificacaoFibonacci {
    long long *pontuacoes;
    int quantidade;
    int maiorPrimo;
    int maiorPar;
    int primeiraAcimaMedia;
} ClassificacaoFibonacci;
Resultado somaFibonacci(long long limite, long long *soma);
int ehPar(long long valor);
int ehPrimo(long long valor);
Resultado classificarFibonacci(const long long *valores, int quantidade, ClassificacaoFibonacci *classificacao);
void liberarClassificacaoFibonacci(ClassificacaoFibonacci *classificacao);
#endif
