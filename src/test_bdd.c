#include <stdio.h>
#include <sqlite3.h>

// Fonction qui sera appelée pour chaque ligne trouvée dans la base
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    for(int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

int main(int argc, char* argv[]) {
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    // 1. Ouverture de la base de données "musique.db"
    rc = sqlite3_open("musique.db", &db);

    if(rc) {
        fprintf(stderr, "Impossible d'ouvrir la base: %s\n", sqlite3_errmsg(db));
        return(0);
    } else {
        fprintf(stderr, "Base de données ouverte avec succès !\n");
    }

    // 2. Envoi de la requête SQL
    // On reprend exactement ce que tu as fait à la main tout à l'heure
    char *sql = "SELECT * FROM chansons;";

    rc = sqlite3_exec(db, sql, callback, 0, &zErrMsg);

    if( rc != SQLITE_OK ) {
        fprintf(stderr, "Erreur SQL: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    } else {
        fprintf(stdout, "Requête exécutée avec succès.\n");
    }

    // 3. Fermeture
    sqlite3_close(db);
    return 0;
}
