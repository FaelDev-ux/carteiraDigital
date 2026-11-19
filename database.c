#include <limits.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include "meu_banco.h"

/* SQL fica apenas neste módulo. Parâmetros são ligados com bind, não concatenados. */
int executar(sqlite3 *db, const char *sql) {
    return sqlite3_exec(db, sql, NULL, NULL, NULL) == SQLITE_OK;
}
int preparar(sqlite3 *db, const char *sql, sqlite3_stmt **stmt) {
    return sqlite3_prepare_v2(db, sql, -1, stmt, NULL) == SQLITE_OK;
}
int inserir(sqlite3_stmt *stmt) {
    int ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    return ok;
}
int inserirMeta(sqlite3_stmt *stmt, const char *chave, long long valor) {
    sqlite3_bind_text(stmt, 1, chave, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, valor);
    return inserir(stmt);
}
int meta(sqlite3 *db, const char *chave, long long padrao, long long *valor) {
    sqlite3_stmt *s = NULL;
    if (!preparar(db, "SELECT valor FROM meta WHERE chave=?", &s)) {
        return 0;
    }
    sqlite3_bind_text(s, 1, chave, -1, SQLITE_STATIC);
    int passo = sqlite3_step(s), ok = 1;
    *valor = padrao;
    if (passo == SQLITE_ROW && sqlite3_column_type(s, 0) == SQLITE_INTEGER) {
        *valor = sqlite3_column_int64(s, 0);
    } else if (passo != SQLITE_DONE) {
        ok = 0;
    }
    sqlite3_finalize(s);
    return ok;
}

/* Em qualquer falha, fecha a consulta e desfaz a transação do SQLite. */
Resultado cancelarGravacao(sqlite3 *db, sqlite3_stmt *consulta) {
    sqlite3_finalize(consulta);
    executar(db, "ROLLBACK");
    return ERRO_PERSISTENCIA;
}

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
    sqlite3 *db = banco->conexao;
    sqlite3_stmt *s = NULL;
    if (!executar(db, "BEGIN IMMEDIATE")) {
        return ERRO_PERSISTENCIA;
    }
    long long revisao = -1;
    /* Impede que uma segunda execução sobrescreva um estado já atualizado. */
    if (!meta(db, "revisao", 0, &revisao) || revisao != banco->revisao) {
        return cancelarGravacao(db, s);
    }
    if (!executar(db, "DELETE FROM premiacoes; DELETE FROM movimentacoes;"
                      "DELETE FROM contas; DELETE FROM clientes; DELETE FROM meta;")) {
        return cancelarGravacao(db, s);
    }
    if (!preparar(db, "INSERT INTO clientes VALUES(?,?,?,?)", &s)) {
        return cancelarGravacao(db, s);
    }
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        sqlite3_bind_int(s, 1, c->id);
        sqlite3_bind_text(s, 2, c->nome, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(s, 3, c->possuiEmprestimo);
        sqlite3_bind_int(s, 4, c->possuiTitulo);
        if (!inserir(s)) {
            return cancelarGravacao(db, s);
        }
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db, "INSERT INTO contas VALUES(?,?,?,?)", &s)) {
        return cancelarGravacao(db, s);
    }
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        for (Conta *a = c->contas; a != NULL; a = a->proximo) {
            sqlite3_bind_int(s, 1, a->numeroBase);
            sqlite3_bind_int(s, 2, c->id);
            sqlite3_bind_int(s, 3, a->tipo);
            sqlite3_bind_int64(s, 4, a->saldoCentavos);
            if (!inserir(s)) {
                return cancelarGravacao(db, s);
            }
        }
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db,
                  "INSERT INTO movimentacoes(conta_numero,ordem,valor,operacao) VALUES(?,?,?,?)",
                  &s)) {
        return cancelarGravacao(db, s);
    }
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        for (Conta *a = c->contas; a != NULL; a = a->proximo) {
            long long ordem = 0;
            for (Movimentacao *m = a->historico; m != NULL; m = m->proximo) {
                sqlite3_bind_int(s, 1, a->numeroBase);
                sqlite3_bind_int64(s, 2, ordem);
                sqlite3_bind_int64(s, 3, m->valorCentavos);
                sqlite3_bind_text(s, 4, &m->operacao, 1, SQLITE_TRANSIENT);
                if (!inserir(s) || ordem == LLONG_MAX) {
                    return cancelarGravacao(db, s);
                }
                ordem++;
            }
        }
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db, "INSERT INTO premiacoes VALUES(?,?,?,?,?)", &s)) {
        return cancelarGravacao(db, s);
    }
    for (Premiacao *p = banco->premiacoes; p != NULL; p = p->proximo) {
        sqlite3_bind_int64(s, 1, p->id);
        sqlite3_bind_int(s, 2, p->cliente->id);
        sqlite3_bind_int(s, 3, p->conta->numeroBase);
        sqlite3_bind_int64(s, 4, p->saldoBaseCentavos);
        sqlite3_bind_int64(s, 5, p->valorCentavos);
        if (!inserir(s)) {
            return cancelarGravacao(db, s);
        }
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db, "INSERT INTO meta VALUES(?,?)", &s)) {
        return cancelarGravacao(db, s);
    }
    if (!inserirMeta(s, "versao", 1) || !inserirMeta(s, "revisao", revisao + 1) ||
        !inserirMeta(s, "proximo_cliente", banco->proximoIdCliente) ||
        !inserirMeta(s, "proxima_conta", banco->proximoNumero) ||
        !inserirMeta(s, "proximo_sorteio", banco->proximoSorteio)) {
        return cancelarGravacao(db, s);
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!executar(db, "COMMIT")) {
        return cancelarGravacao(db, s);
    }
    banco->revisao = revisao + 1;
    return BANCO_OK;
}

Conta *porNumero(Banco *banco, int numero) {
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        for (Conta *a = c->contas; a != NULL; a = a->proximo) {
            if (a->numeroBase == numero) {
                return a;
            }
        }
    }
    return NULL;
}

/* Os registros já recuperados serão liberados por liberarBanco se houver erro. */
Resultado cancelarLeitura(sqlite3_stmt *consulta, Resultado erro) {
    sqlite3_finalize(consulta);
    return erro;
}

Resultado carregar(Banco *banco) {
    sqlite3 *db = banco->conexao;
    sqlite3_stmt *s = NULL;
    Resultado erro = ERRO_PERSISTENCIA;
    int passo, maiorCliente = 0, maiorConta = 999;
    long long maiorSorteio = 0, valor = 0;
    if (!meta(db, "versao", 1, &valor) || valor != 1) {
        return erro;
    }
    if (!preparar(db, "SELECT id,nome,possui_emprestimo,possui_titulo FROM clientes ORDER BY id",
                  &s)) {
        return cancelarLeitura(s, erro);
    }
    while ((passo = sqlite3_step(s)) == SQLITE_ROW) {
        long long id = sqlite3_column_int64(s, 0);
        const char *nome = (const char *)sqlite3_column_text(s, 1);
        int e = sqlite3_column_int(s, 2), t = sqlite3_column_int(s, 3);
        if (sqlite3_column_type(s, 0) != SQLITE_INTEGER || id < 1 || id >= INT_MAX ||
            nome == NULL || sqlite3_column_bytes(s, 1) != (int)strlen(nome) || (e != 0 && e != 1) ||
            (t != 0 && t != 1) || banco->quantidadeClientes == INT_MAX) {
            return cancelarLeitura(s, erro);
        }
        const char *texto = nome;
        while (*texto && ehEspaco(*texto) == 1) {
            texto++;
        }
        if (!*texto) {
            return cancelarLeitura(s, erro);
        }
        Cliente *novo = malloc(sizeof(Cliente));
        if (novo == NULL) {
            erro = ERRO_MEMORIA;
            return cancelarLeitura(s, erro);
        }
        novo->id = (int)id;
        novo->nome = NULL;
        novo->possuiEmprestimo = e;
        novo->possuiTitulo = t;
        novo->contas = NULL;
        novo->proximo = banco->clientes;
        novo->nome = malloc(strlen(nome) + 1);
        if (novo->nome == NULL) {
            free(novo);
            erro = ERRO_MEMORIA;
            return cancelarLeitura(s, erro);
        }
        strcpy(novo->nome, nome);
        banco->clientes = novo;
        banco->quantidadeClientes++;
        maiorCliente = (int)id;
    }
    if (passo != SQLITE_DONE) {
        return cancelarLeitura(s, erro);
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db, "SELECT numero,cliente_id,tipo,saldo FROM contas ORDER BY numero", &s)) {
        return cancelarLeitura(s, erro);
    }
    while ((passo = sqlite3_step(s)) == SQLITE_ROW) {
        long long numero = sqlite3_column_int64(s, 0), id = sqlite3_column_int64(s, 1);
        int tipo = sqlite3_column_int(s, 2);
        if (numero < 1000 || numero > 9999 || id < 1 || id >= INT_MAX || tipo < CORRENTE ||
            tipo > POUPANCA || sqlite3_column_type(s, 3) != SQLITE_INTEGER) {
            return cancelarLeitura(s, erro);
        }
        Cliente *c = buscarCliente(banco, (int)id);
        if (c == NULL || contaDoTipo(c, (TipoConta)tipo) != NULL ||
            (tipo == CORRENTE && contaDoTipo(c, SALARIO) != NULL) ||
            (tipo == SALARIO && contaDoTipo(c, CORRENTE) != NULL)) {
            return cancelarLeitura(s, erro);
        }
        Conta *nova = malloc(sizeof(Conta));
        if (nova == NULL) {
            erro = ERRO_MEMORIA;
            return cancelarLeitura(s, erro);
        }
        nova->numeroBase = (int)numero;
        nova->tipo = (TipoConta)tipo;
        nova->saldoCentavos = sqlite3_column_int64(s, 3);
        nova->historico = NULL;
        nova->proximo = c->contas;
        c->contas = nova;
        maiorConta = (int)numero;
    }
    if (passo != SQLITE_DONE) {
        return cancelarLeitura(s, erro);
    }
    sqlite3_finalize(s);
    s = NULL;
    for (Cliente *c = banco->clientes; c != NULL; c = c->proximo) {
        if ((c->possuiTitulo && contaDoTipo(c, SALARIO) == NULL) ||
            (c->possuiEmprestimo &&
             (contaDoTipo(c, CORRENTE) == NULL || contaDoTipo(c, POUPANCA) == NULL))) {
            return cancelarLeitura(s, erro);
        }
    }
    if (!preparar(db,
                  "SELECT conta_numero,valor,operacao FROM movimentacoes ORDER BY "
                  "conta_numero,ordem DESC",
                  &s)) {
        return cancelarLeitura(s, erro);
    }
    while ((passo = sqlite3_step(s)) == SQLITE_ROW) {
        long long numero = sqlite3_column_int64(s, 0), quantia = sqlite3_column_int64(s, 1);
        const char *op = (const char *)sqlite3_column_text(s, 2);
        if (numero < 1000 || numero > 9999 || quantia <= 0 || op == NULL ||
            sqlite3_column_bytes(s, 2) != 1 || (*op != 'S' && *op != 'D' && *op != 'E') ||
            sqlite3_column_type(s, 1) != SQLITE_INTEGER) {
            return cancelarLeitura(s, erro);
        }
        Conta *conta = porNumero(banco, (int)numero);
        if (conta == NULL) {
            return cancelarLeitura(s, erro);
        }
        Resultado resultado = registrarMovimentacao(conta, quantia, *op);
        if (resultado != BANCO_OK) {
            erro = resultado;
            return cancelarLeitura(s, erro);
        }
    }
    if (passo != SQLITE_DONE) {
        return cancelarLeitura(s, erro);
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!preparar(db,
                  "SELECT id,cliente_id,conta_numero,saldo_base,valor FROM premiacoes ORDER BY id",
                  &s)) {
        return cancelarLeitura(s, erro);
    }
    while ((passo = sqlite3_step(s)) == SQLITE_ROW) {
        long long id = sqlite3_column_int64(s, 0), clienteId = sqlite3_column_int64(s, 1);
        long long numero = sqlite3_column_int64(s, 2), base = sqlite3_column_int64(s, 3);
        long long premio = sqlite3_column_int64(s, 4);
        if (id <= 0 || id == LLONG_MAX || clienteId < 1 || clienteId >= INT_MAX || numero < 1000 ||
            numero > 9999 || base < 100000 || base > 150000 || premio != base * 10) {
            return cancelarLeitura(s, erro);
        }
        Cliente *c = buscarCliente(banco, (int)clienteId);
        Conta *a = porNumero(banco, (int)numero);
        if (c == NULL || a == NULL || a->tipo != SALARIO || buscarTitular(banco, a) != c) {
            return cancelarLeitura(s, erro);
        }
        Premiacao *p = malloc(sizeof(Premiacao));
        if (p == NULL) {
            erro = ERRO_MEMORIA;
            return cancelarLeitura(s, erro);
        }
        p->id = id;
        p->cliente = c;
        p->conta = a;
        p->saldoBaseCentavos = base;
        p->valorCentavos = premio;
        p->proximo = banco->premiacoes;
        banco->premiacoes = p;
        maiorSorteio = id;
    }
    if (passo != SQLITE_DONE) {
        return cancelarLeitura(s, erro);
    }
    sqlite3_finalize(s);
    s = NULL;
    if (!meta(db, "proximo_cliente", maiorCliente + 1LL, &valor) || valor <= maiorCliente ||
        valor > INT_MAX) {
        return cancelarLeitura(s, erro);
    }
    banco->proximoIdCliente = (int)valor;
    if (!meta(db, "proxima_conta", maiorConta + 1LL, &valor) || valor <= maiorConta ||
        valor > 10000) {
        return cancelarLeitura(s, erro);
    }
    banco->proximoNumero = (int)valor;
    if (!meta(db, "proximo_sorteio", maiorSorteio + 1, &valor) || valor <= maiorSorteio) {
        return cancelarLeitura(s, erro);
    }
    banco->proximoSorteio = valor;
    if (!meta(db, "revisao", 0, &valor) || valor < 0) {
        return cancelarLeitura(s, erro);
    }
    banco->revisao = valor;
    return BANCO_OK;
}

Resultado abrirPersistencia(Banco *banco, const char *caminho) {
    if (banco == NULL || caminho == NULL || *caminho == '\0' || banco->conexao != NULL ||
        banco->clientes != NULL || banco->premiacoes != NULL) {
        return ERRO_ARGUMENTO_INVALIDO;
    }
    sqlite3 *db = NULL;
    if (sqlite3_open(caminho, &db) != SQLITE_OK) {
        sqlite3_close(db);
        return ERRO_PERSISTENCIA;
    }
    sqlite3_busy_timeout(db, 3000);
    const char *schema =
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
    if (!executar(db, schema)) {
        sqlite3_close(db);
        return ERRO_PERSISTENCIA;
    }
    banco->conexao = db;
    /* Uma transação de leitura mantém as tabelas no mesmo instante lógico. */
    if (!executar(db, "BEGIN")) {
        return ERRO_PERSISTENCIA;
    }
    Resultado resultado = carregar(banco);
    if (resultado != BANCO_OK || !executar(db, "COMMIT")) {
        executar(db, "ROLLBACK");
        if (resultado == BANCO_OK) {
            return ERRO_PERSISTENCIA;
        }
        return resultado;
    }
    return BANCO_OK;
}
