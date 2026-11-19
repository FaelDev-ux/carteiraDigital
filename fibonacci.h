#ifndef FIBONACCI_H
#define FIBONACCI_H
#include "meu_banco.h"

/* Biblioteca adaptada dos três arquivos de aula enviados pelo usuário.
   Não contém main, scanf, nomes fixos nem fatorial. */
typedef struct ClassificacaoFibonacci {
    /* Registro com as pontuações e posições calculadas; não possui funções. */
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
