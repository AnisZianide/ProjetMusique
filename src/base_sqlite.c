#include "base_sqlite.h"
#include <stdio.h>
#include <string.h>

extern const char *DB_FILE; // Défini dans modeles.h/main.c

void init_db() {
    sqlite3 *db; char *err = 0;
    if(sqlite3_open(DB_FILE, &db) != SQLITE_OK) return;
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);
   
    // V26 : Ajout des colonnes 'album' et 'duree'
    const char *sql_musiques = "CREATE TABLE IF NOT EXISTS musiques ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "chemin TEXT UNIQUE, "
                               "titre TEXT, "
                               "artiste TEXT, "
                               "album TEXT, "
                               "duree INTEGER);"; // Durée en secondes
                               
    sqlite3_exec(db, sql_musiques, 0, 0, &err);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, nom TEXT UNIQUE);", 0, 0, &err);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS compositions (playlist_id INTEGER, musique_id INTEGER, PRIMARY KEY (playlist_id, musique_id), FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE, FOREIGN KEY(musique_id) REFERENCES musiques(id) ON DELETE CASCADE);", 0, 0, &err);
    sqlite3_close(db);
}

// V26 : Mise à jour de la signature pour inclure album et durée
int ajouter_bdd(const char *c, const char *t, const char *a, const char *alb, int dur) {
    sqlite3 *db; sqlite3_stmt *st;
    sqlite3_open(DB_FILE, &db);

    // Vérifier si le chemin existe déjà (Sécurisé avec bind)
    sqlite3_prepare_v2(db, "SELECT id FROM musiques WHERE chemin=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC);
    if(sqlite3_step(st) == SQLITE_ROW) {
        sqlite3_finalize(st);
        sqlite3_close(db);
        return 0; // Déjà présent
    }
    sqlite3_finalize(st);

    // Insertion sécurisée (V26)
    sqlite3_prepare_v2(db, "INSERT INTO musiques (chemin, titre, artiste, album, duree) VALUES (?, ?, ?, ?, ?);", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 2, t, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 3, a, -1, SQLITE_STATIC);
    sqlite3_bind_text(st, 4, alb ? alb : "Album Inconnu", -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 5, dur);

    int success = 0;
    if(sqlite3_step(st) == SQLITE_DONE) success = 1;
   
    sqlite3_finalize(st);
    sqlite3_close(db);
    return success;
}

void supprimer_bdd(const char *c) {
    sqlite3 *db; sqlite3_stmt *st;
    sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, "DELETE FROM musiques WHERE chemin=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC);
    sqlite3_step(st);
    sqlite3_finalize(st);
    sqlite3_close(db);
}

int ajouter_playlist_bdd(const char *n){
    sqlite3 *d; sqlite3_stmt *s; int r=0;
    sqlite3_open(DB_FILE, &d);
    sqlite3_prepare_v2(d, "INSERT INTO playlists(nom)VALUES(?);", -1, &s, 0);
    sqlite3_bind_text(s, 1, n, -1, SQLITE_STATIC);
    if(sqlite3_step(s) == SQLITE_DONE) r=1;
    sqlite3_finalize(s); sqlite3_close(d);
    return r;
}

int get_id_playlist(const char *n){
    sqlite3 *d; sqlite3_stmt *s; int i=-1;
    sqlite3_open(DB_FILE, &d);
    sqlite3_prepare_v2(d, "SELECT id FROM playlists WHERE nom=?;", -1, &s, 0);
    sqlite3_bind_text(s, 1, n, -1, SQLITE_STATIC);
    if(sqlite3_step(s) == SQLITE_ROW) i = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); sqlite3_close(d);
    return i;
}

int get_id_musique(const char *c){
    sqlite3 *d; sqlite3_stmt *s; int i=-1;
    sqlite3_open(DB_FILE, &d);
    sqlite3_prepare_v2(d, "SELECT id FROM musiques WHERE chemin=?;", -1, &s, 0);
    sqlite3_bind_text(s, 1, c, -1, SQLITE_STATIC);
    if(sqlite3_step(s) == SQLITE_ROW) i = sqlite3_column_int(s, 0);
    sqlite3_finalize(s); sqlite3_close(d);
    return i;
}

void ajouter_compo_bdd(int p, int m){
    sqlite3 *d; sqlite3_stmt *s;
    sqlite3_open(DB_FILE, &d);
    sqlite3_prepare_v2(d, "INSERT OR IGNORE INTO compositions(playlist_id,musique_id)VALUES(?,?);", -1, &s, 0);
    sqlite3_bind_int(s, 1, p);
    sqlite3_bind_int(s, 2, m);
    if(sqlite3_step(s) == SQLITE_DONE) set_status("Ajouté !", 0);
    else set_status("Erreur.", 1);
    sqlite3_finalize(s); sqlite3_close(d);
}
