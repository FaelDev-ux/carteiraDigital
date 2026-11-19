#include <limits.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

/* SQL fica apenas neste módulo. Parâmetros são ligados com bind, não concatenados.
   Ciclo de uma consulta: preparar -> preencher os ? -> executar -> finalizar.*/
/* Executa SQL sem parâmetros, como CREATE TABLE, BEGIN e COMMIT. */
int executarSql(sqlite3 *conexao, const char *sql) {
    return sqlite3_exec(conexao, sql, NULL, NULL, NULL) == SQLITE_OK;
}
/* Prepara SQL com ?; os valores serão preenchidos pelas chamadas bind.
   O SQLite recebe &consulta para colocar ali o endereço da consulta preparada. */
int prepararConsulta(sqlite3 *conexao, const char *sql, sqlite3_stmt **consulta) {
    return sqlite3_prepare_v2(conexao, sql, -1, consulta, NULL) == SQLITE_OK;
}
/* Grava uma linha e deixa a consulta pronta para receber a próxima. */
int gravarRegistro(sqlite3_stmt *consulta) {
    int sucesso = sqlite3_step(consulta) == SQLITE_DONE;
    sqlite3_reset(consulta);
    sqlite3_clear_bindings(consulta);
    return sucesso;
}
int gravarConfiguracao(sqlite3_stmt *consulta, const char *chave, long long valor) {
    sqlite3_bind_text(consulta, 1, chave, -1, SQLITE_STATIC);
    sqlite3_bind_int64(consulta, 2, valor);
    return gravarRegistro(consulta);
}
int lerConfiguracao(sqlite3 *conexao, const char *chave, long long padrao, long long *valor) {
    sqlite3_stmt *consulta = NULL;
    if (!prepararConsulta(conexao, "SELECT valor FROM meta WHERE chave=?", &consulta)) {
        return 0;
    }
    sqlite3_bind_text(consulta, 1, chave, -1, SQLITE_STATIC);
    int estadoConsulta = sqlite3_step(consulta);
    int sucesso = 1;
    *valor = padrao;
    if (estadoConsulta == SQLITE_ROW && sqlite3_column_type(consulta, 0) == SQLITE_INTEGER) {
        *valor = sqlite3_column_int64(consulta, 0);
    } else if (estadoConsulta != SQLITE_DONE) {
        sucesso = 0;
    }
    sqlite3_finalize(consulta);
    return sucesso;
}

/* Em qualquer falha, fecha a consulta e desfaz a transação do SQLite. */
Resultado desfazerGravacao(sqlite3 *conexao, sqlite3_stmt *consulta) {
    sqlite3_finalize(consulta);
    executarSql(conexao, "ROLLBACK");
    return ERRO_PERSISTENCIA;
}

/* Salva uma fotografia das listas. O COMMIT só confirma se todas as tabelas
   forem gravadas; em caso de erro, o ROLLBACK preserva os dados anteriores. */
Resultado salvarBanco(Banco *banco) {
    if (banco == NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    if (banco->conexao == NULL) {
        return BANCO_OK;
    }
    if (banco->revisao == LLONG_MAX) {
        return ERRO_LIMITE_NUMERICO;
    }
    sqlite3 *conexao = banco->conexao;
    sqlite3_stmt *consulta = NULL;
    if (!executarSql(conexao, "BEGIN IMMEDIATE")) {
        return ERRO_PERSISTENCIA;
    }
    long long revisao = -1;
    /* Impede que uma segunda execução sobrescreva um estado já atualizado. */
    if (!lerConfiguracao(conexao, "revisao", 0, &revisao) || revisao != banco->revisao) {
        return desfazerGravacao(conexao, consulta);
    }
    if (!executarSql(conexao, "DELETE FROM premiacoes; DELETE FROM movimentacoes;"
                              "DELETE FROM contas; DELETE FROM clientes; DELETE FROM meta;")) {
        return desfazerGravacao(conexao, consulta);
    }
    if (!prepararConsulta(
            conexao,
            "INSERT INTO clientes(id,nome,possui_emprestimo,possui_titulo) VALUES(?,?,?,?)",
            &consulta)) {
        return desfazerGravacao(conexao, consulta);
    }
    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        sqlite3_bind_int(consulta, 1, clienteAtual->id);
        sqlite3_bind_text(consulta, 2, clienteAtual->nome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(consulta, 3, clienteAtual->possuiEmprestimo);
        sqlite3_bind_int(consulta, 4, clienteAtual->possuiTitulo);
        if (!gravarRegistro(consulta)) {
            return desfazerGravacao(conexao, consulta);
        }
    }
    sqlite3_finalize(consulta);
    consulta = NULL;
    if (!prepararConsulta(conexao,
                          "INSERT INTO contas(numero,cliente_id,tipo,saldo) VALUES(?,?,?,?)",
                          &consulta)) {
        return desfazerGravacao(conexao, consulta);
    }
    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        for (Conta *contaAtual = clienteAtual->contas; contaAtual != NULL;
             contaAtual = contaAtual->proximo) {
            sqlite3_bind_int(consulta, 1, contaAtual->numeroBase);
            sqlite3_bind_int(consulta, 2, clienteAtual->id);
            sqlite3_bind_int(consulta, 3, contaAtual->tipo);
            sqlite3_bind_int64(consulta, 4, contaAtual->saldoCentavos);
            if (!gravarRegistro(consulta)) {
                return desfazerGravacao(conexao, consulta);
            }
        }
    }
    sqlite3_finalize(consulta);
    consulta = NULL;
    if (!prepararConsulta(
            conexao, "INSERT INTO movimentacoes(conta_numero,ordem,valor,operacao) VALUES(?,?,?,?)",
            &consulta)) {
        return desfazerGravacao(conexao, consulta);
    }
    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        for (Conta *contaAtual = clienteAtual->contas; contaAtual != NULL;
             contaAtual = contaAtual->proximo) {
            long long ordem = 0;
            for (Movimentacao *movimentacaoAtual = contaAtual->historico; movimentacaoAtual != NULL;
                 movimentacaoAtual = movimentacaoAtual->proximo) {
                sqlite3_bind_int(consulta, 1, contaAtual->numeroBase);
                sqlite3_bind_int64(consulta, 2, ordem);
                sqlite3_bind_int64(consulta, 3, movimentacaoAtual->valorCentavos);
                sqlite3_bind_text(consulta, 4, &movimentacaoAtual->operacao, 1, SQLITE_TRANSIENT);
                if (!gravarRegistro(consulta) || ordem == LLONG_MAX) {
                    return desfazerGravacao(conexao, consulta);
                }
                ordem++;
            }
        }
    }
    sqlite3_finalize(consulta);
    consulta = NULL;
    if (!prepararConsulta(
            conexao,
            "INSERT INTO premiacoes(id,cliente_id,conta_numero,saldo_base,valor) VALUES(?,?,?,?,?)",
            &consulta)) {
        return desfazerGravacao(conexao, consulta);
    }
    for (Premiacao *premiacaoAtual = banco->premiacoes; premiacaoAtual != NULL;
         premiacaoAtual = premiacaoAtual->proximo) {
        sqlite3_bind_int64(consulta, 1, premiacaoAtual->id);
        sqlite3_bind_int(consulta, 2, premiacaoAtual->cliente->id);
        sqlite3_bind_int(consulta, 3, premiacaoAtual->conta->numeroBase);
        sqlite3_bind_int64(consulta, 4, premiacaoAtual->saldoBaseCentavos);
        sqlite3_bind_int64(consulta, 5, premiacaoAtual->valorCentavos);
        if (!gravarRegistro(consulta)) {
            return desfazerGravacao(conexao, consulta);
        }
    }
    sqlite3_finalize(consulta);
    consulta = NULL;
    if (!prepararConsulta(conexao, "INSERT INTO meta(chave,valor) VALUES(?,?)", &consulta)) {
        return desfazerGravacao(conexao, consulta);
    }
    if (!gravarConfiguracao(consulta, "versao", 1) ||
        !gravarConfiguracao(consulta, "revisao", revisao + 1) ||
        !gravarConfiguracao(consulta, "proximo_cliente", banco->proximoIdCliente) ||
        !gravarConfiguracao(consulta, "proxima_conta", banco->proximoNumero) ||
        !gravarConfiguracao(consulta, "proximo_sorteio", banco->proximoSorteio)) {
        return desfazerGravacao(conexao, consulta);
    }
    sqlite3_finalize(consulta);
    consulta = NULL;
    if (!executarSql(conexao, "COMMIT")) {
        return desfazerGravacao(conexao, consulta);
    }
    banco->revisao = revisao + 1;
    return BANCO_OK;
}

Conta *buscarContaPeloNumero(Banco *banco, int numeroConta) {
    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        for (Conta *contaAtual = clienteAtual->contas; contaAtual != NULL;
             contaAtual = contaAtual->proximo) {
            if (contaAtual->numeroBase == numeroConta) {
                return contaAtual;
            }
        }
    }
    return NULL;
}

/* Fecha a consulta e devolve o resultado. Se houver erro, os registros já
   recuperados serão liberados por liberarBanco. */
Resultado finalizarRecuperacao(sqlite3_stmt *consulta, Resultado resultado) {
    sqlite3_finalize(consulta);
    return resultado;
}

/* Recupera clientes e seus nomes. */
Resultado recuperarClientes(Banco *banco) {
    sqlite3 *conexao = banco->conexao;
    sqlite3_stmt *consulta = NULL;
    int estadoConsulta;
    if (!prepararConsulta(
            conexao, "SELECT id,nome,possui_emprestimo,possui_titulo FROM clientes ORDER BY id",
            &consulta)) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    estadoConsulta = sqlite3_step(consulta);
    while (estadoConsulta == SQLITE_ROW) {
        long long identificador = sqlite3_column_int64(consulta, 0);
        const char *nome = (const char *)sqlite3_column_text(consulta, 1);
        int possuiEmprestimo = sqlite3_column_int(consulta, 2);
        int possuiTitulo = sqlite3_column_int(consulta, 3);
        if (sqlite3_column_type(consulta, 0) != SQLITE_INTEGER || identificador < 1 ||
            identificador >= INT_MAX || nome == NULL ||
            sqlite3_column_bytes(consulta, 1) != (int)strlen(nome) ||
            (possuiEmprestimo != 0 && possuiEmprestimo != 1) ||
            (possuiTitulo != 0 && possuiTitulo != 1) || banco->quantidadeClientes == INT_MAX) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        const char *texto = nome;
        while (*texto && ehEspaco(*texto) == 1) {
            texto++;
        }
        if (!*texto) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Cliente *novoCliente = malloc(sizeof(Cliente));
        if (novoCliente == NULL) {
            return finalizarRecuperacao(consulta, ERRO_MEMORIA);
        }
        novoCliente->id = (int)identificador;
        novoCliente->nome = NULL;
        novoCliente->possuiEmprestimo = possuiEmprestimo;
        novoCliente->possuiTitulo = possuiTitulo;
        novoCliente->contas = NULL;
        novoCliente->proximo = banco->clientes;
        novoCliente->nome = malloc(strlen(nome) + 1);
        if (novoCliente->nome == NULL) {
            free(novoCliente);
            return finalizarRecuperacao(consulta, ERRO_MEMORIA);
        }
        strcpy(novoCliente->nome, nome);
        banco->clientes = novoCliente;
        banco->quantidadeClientes++;
        estadoConsulta = sqlite3_step(consulta);
    }
    if (estadoConsulta != SQLITE_DONE) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    return finalizarRecuperacao(consulta, BANCO_OK);
}

/* Recupera contas e seus saldos. */
Resultado recuperarContas(Banco *banco) {
    sqlite3 *conexao = banco->conexao;
    sqlite3_stmt *consulta = NULL;
    int estadoConsulta;
    if (!prepararConsulta(conexao,
                          "SELECT numero,cliente_id,tipo,saldo FROM contas ORDER BY numero",
                          &consulta)) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    estadoConsulta = sqlite3_step(consulta);
    while (estadoConsulta == SQLITE_ROW) {
        long long numeroConta = sqlite3_column_int64(consulta, 0);
        long long identificador = sqlite3_column_int64(consulta, 1);
        int tipo = sqlite3_column_int(consulta, 2);
        if (numeroConta < 1000 || numeroConta > 9999 || identificador < 1 ||
            identificador >= INT_MAX || tipo < CORRENTE || tipo > POUPANCA ||
            sqlite3_column_type(consulta, 3) != SQLITE_INTEGER) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Cliente *clienteAtual = buscarCliente(banco, (int)identificador);
        if (clienteAtual == NULL || contaDoTipo(clienteAtual, (TipoConta)tipo) != NULL ||
            (tipo == CORRENTE && contaDoTipo(clienteAtual, SALARIO) != NULL) ||
            (tipo == SALARIO && contaDoTipo(clienteAtual, CORRENTE) != NULL)) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Conta *novaConta = malloc(sizeof(Conta));
        if (novaConta == NULL) {
            return finalizarRecuperacao(consulta, ERRO_MEMORIA);
        }
        novaConta->numeroBase = (int)numeroConta;
        novaConta->tipo = (TipoConta)tipo;
        novaConta->saldoCentavos = sqlite3_column_int64(consulta, 3);
        novaConta->historico = NULL;
        novaConta->proximo = clienteAtual->contas;
        clienteAtual->contas = novaConta;
        estadoConsulta = sqlite3_step(consulta);
    }
    if (estadoConsulta != SQLITE_DONE) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        if ((clienteAtual->possuiTitulo && contaDoTipo(clienteAtual, SALARIO) == NULL) ||
            (clienteAtual->possuiEmprestimo && (contaDoTipo(clienteAtual, CORRENTE) == NULL ||
                                                contaDoTipo(clienteAtual, POUPANCA) == NULL))) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
    }
    return finalizarRecuperacao(consulta, BANCO_OK);
}

/* Recupera o histórico, mantendo a ordem das operações. */
Resultado recuperarMovimentacoes(Banco *banco) {
    sqlite3 *conexao = banco->conexao;
    sqlite3_stmt *consulta = NULL;
    int estadoConsulta;
    if (!prepararConsulta(conexao,
                          "SELECT conta_numero,valor,operacao FROM movimentacoes ORDER BY "
                          "conta_numero,ordem DESC",
                          &consulta)) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    estadoConsulta = sqlite3_step(consulta);
    while (estadoConsulta == SQLITE_ROW) {
        long long numeroConta = sqlite3_column_int64(consulta, 0);
        long long valorCentavos = sqlite3_column_int64(consulta, 1);
        const char *operacao = (const char *)sqlite3_column_text(consulta, 2);
        if (numeroConta < 1000 || numeroConta > 9999 || valorCentavos <= 0 || operacao == NULL ||
            sqlite3_column_bytes(consulta, 2) != 1 ||
            (*operacao != 'S' && *operacao != 'D' && *operacao != 'E') ||
            sqlite3_column_type(consulta, 1) != SQLITE_INTEGER) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Conta *conta = buscarContaPeloNumero(banco, (int)numeroConta);
        if (conta == NULL) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Resultado resultado = registrarMovimentacao(conta, valorCentavos, *operacao);
        if (resultado != BANCO_OK) {
            return finalizarRecuperacao(consulta, resultado);
        }
        estadoConsulta = sqlite3_step(consulta);
    }
    if (estadoConsulta != SQLITE_DONE) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    return finalizarRecuperacao(consulta, BANCO_OK);
}

/* Recupera os prêmios e suas referências a clientes e contas. */
Resultado recuperarPremiacoes(Banco *banco) {
    sqlite3 *conexao = banco->conexao;
    sqlite3_stmt *consulta = NULL;
    int estadoConsulta;
    if (!prepararConsulta(
            conexao,
            "SELECT id,cliente_id,conta_numero,saldo_base,valor FROM premiacoes ORDER BY id",
            &consulta)) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    estadoConsulta = sqlite3_step(consulta);
    while (estadoConsulta == SQLITE_ROW) {
        long long identificador = sqlite3_column_int64(consulta, 0);
        long long clienteId = sqlite3_column_int64(consulta, 1);
        long long numeroConta = sqlite3_column_int64(consulta, 2);
        long long saldoBase = sqlite3_column_int64(consulta, 3);
        long long valorPremio = sqlite3_column_int64(consulta, 4);
        if (identificador <= 0 || identificador == LLONG_MAX || clienteId < 1 ||
            clienteId >= INT_MAX || numeroConta < 1000 || numeroConta > 9999 ||
            saldoBase < 100000 || saldoBase > 150000 || valorPremio != saldoBase * 10) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Cliente *clienteAtual = buscarCliente(banco, (int)clienteId);
        Conta *contaAtual = buscarContaPeloNumero(banco, (int)numeroConta);
        if (clienteAtual == NULL || contaAtual == NULL || contaAtual->tipo != SALARIO ||
            buscarTitular(banco, contaAtual) != clienteAtual) {
            return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
        }
        Premiacao *novaPremiacao = malloc(sizeof(Premiacao));
        if (novaPremiacao == NULL) {
            return finalizarRecuperacao(consulta, ERRO_MEMORIA);
        }
        novaPremiacao->id = identificador;
        novaPremiacao->cliente = clienteAtual;
        novaPremiacao->conta = contaAtual;
        novaPremiacao->saldoBaseCentavos = saldoBase;
        novaPremiacao->valorCentavos = valorPremio;
        novaPremiacao->proximo = banco->premiacoes;
        banco->premiacoes = novaPremiacao;
        estadoConsulta = sqlite3_step(consulta);
    }
    if (estadoConsulta != SQLITE_DONE) {
        return finalizarRecuperacao(consulta, ERRO_PERSISTENCIA);
    }
    return finalizarRecuperacao(consulta, BANCO_OK);
}

/* Mantém os números das próximas inclusões mesmo depois de fechar o programa. */
Resultado recuperarConfiguracoes(Banco *banco) {
    sqlite3 *conexao = banco->conexao;
    int maiorCliente = 0;
    int maiorConta = 999;
    long long maiorSorteio = 0;
    long long valor = 0;

    for (Cliente *clienteAtual = banco->clientes; clienteAtual != NULL;
         clienteAtual = clienteAtual->proximo) {
        if (clienteAtual->id > maiorCliente) {
            maiorCliente = clienteAtual->id;
        }
        for (Conta *contaAtual = clienteAtual->contas; contaAtual != NULL;
             contaAtual = contaAtual->proximo) {
            if (contaAtual->numeroBase > maiorConta) {
                maiorConta = contaAtual->numeroBase;
            }
        }
    }
    for (Premiacao *premiacaoAtual = banco->premiacoes; premiacaoAtual != NULL;
         premiacaoAtual = premiacaoAtual->proximo) {
        if (premiacaoAtual->id > maiorSorteio) {
            maiorSorteio = premiacaoAtual->id;
        }
    }
    if (!lerConfiguracao(conexao, "proximo_cliente", maiorCliente + 1LL, &valor) ||
        valor <= maiorCliente || valor > INT_MAX) {
        return ERRO_PERSISTENCIA;
    }
    banco->proximoIdCliente = (int)valor;
    if (!lerConfiguracao(conexao, "proxima_conta", maiorConta + 1LL, &valor) ||
        valor <= maiorConta || valor > 10000) {
        return ERRO_PERSISTENCIA;
    }
    banco->proximoNumero = (int)valor;
    if (!lerConfiguracao(conexao, "proximo_sorteio", maiorSorteio + 1, &valor) ||
        valor <= maiorSorteio) {
        return ERRO_PERSISTENCIA;
    }
    banco->proximoSorteio = valor;
    if (!lerConfiguracao(conexao, "revisao", 0, &valor) || valor < 0) {
        return ERRO_PERSISTENCIA;
    }
    banco->revisao = valor;
    return BANCO_OK;
}

/* Primeiro os clientes, depois os registros que dependem deles. */
Resultado recuperarBanco(Banco *banco) {
    long long versao = 0;
    if (!lerConfiguracao(banco->conexao, "versao", 1, &versao) || versao != 1) {
        return ERRO_PERSISTENCIA;
    }
    Resultado resultado = recuperarClientes(banco);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    resultado = recuperarContas(banco);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    resultado = recuperarMovimentacoes(banco);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    resultado = recuperarPremiacoes(banco);
    if (resultado != BANCO_OK) {
        return resultado;
    }
    return recuperarConfiguracoes(banco);
}

/* CREATE TABLE IF NOT EXISTS preserva as tabelas e os dados já existentes. */
Resultado criarTabelas(sqlite3 *conexao) {
    const char *tabelas =
        "PRAGMA foreign_keys=ON;"
        "CREATE TABLE IF NOT EXISTS meta(chave TEXT PRIMARY KEY,valor INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS clientes(id INTEGER PRIMARY KEY CHECK(id>0),nome TEXT NOT NULL,"
        "possui_emprestimo INTEGER NOT NULL CHECK(possui_emprestimo IN(0,1)),"
        "possui_titulo INTEGER NOT NULL CHECK(possui_titulo IN(0,1)));"
        "CREATE TABLE IF NOT EXISTS contas(numero INTEGER PRIMARY KEY CHECK(numero BETWEEN 1000 "
        "AND 9999),"
        "cliente_id INTEGER NOT NULL REFERENCES clientes(id),tipo INTEGER NOT NULL CHECK(tipo "
        "IN(1,2,3)),"
        "saldo INTEGER NOT NULL,UNIQUE(cliente_id,tipo));"
        "CREATE TABLE IF NOT EXISTS movimentacoes(id INTEGER PRIMARY KEY,"
        "conta_numero INTEGER NOT NULL REFERENCES contas(numero),ordem INTEGER NOT NULL "
        "CHECK(ordem>=0),"
        "valor INTEGER NOT NULL CHECK(valor>0),operacao TEXT NOT NULL CHECK(operacao "
        "IN('S','D','E')),"
        "UNIQUE(conta_numero,ordem));"
        "CREATE TABLE IF NOT EXISTS premiacoes(id INTEGER PRIMARY KEY,"
        "cliente_id INTEGER NOT NULL REFERENCES clientes(id),"
        "conta_numero INTEGER NOT NULL REFERENCES contas(numero),"
        "saldo_base INTEGER NOT NULL CHECK(saldo_base BETWEEN 100000 AND 150000),"
        "valor INTEGER NOT NULL CHECK(valor=saldo_base*10));";
    if (!executarSql(conexao, tabelas)) {
        return ERRO_PERSISTENCIA;
    }
    return BANCO_OK;
}

Resultado abrirPersistencia(Banco *banco, const char *caminho) {
    if (banco == NULL || caminho == NULL || *caminho == '\0' || banco->conexao != NULL ||
        banco->clientes != NULL || banco->premiacoes != NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    sqlite3 *conexao = NULL;
    if (sqlite3_open(caminho, &conexao) != SQLITE_OK) {
        sqlite3_close(conexao);
        return ERRO_PERSISTENCIA;
    }
    sqlite3_busy_timeout(conexao, 3000);
    Resultado resultado = criarTabelas(conexao);
    if (resultado != BANCO_OK) {
        sqlite3_close(conexao);
        return resultado;
    }
    banco->conexao = conexao;
    /* Uma transação de leitura mantém as tabelas no mesmo instante lógico. */
    if (!executarSql(conexao, "BEGIN")) {
        return ERRO_PERSISTENCIA;
    }
    resultado = recuperarBanco(banco);
    if (resultado != BANCO_OK || !executarSql(conexao, "COMMIT")) {
        executarSql(conexao, "ROLLBACK");
        if (resultado == BANCO_OK) {
            return ERRO_PERSISTENCIA;
        }
        return resultado;
    }
    return BANCO_OK;
}
