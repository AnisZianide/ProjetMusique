#ifndef BASE_SQLITE_H
#define BASE_SQLITE_H
#include "modeles.h"

void init_db();
// V26 : Ajout album et durée dans les arguments
int ajouter_bdd(const char *c, const char *t, const char *a, const char *alb, int dur);
void supprimer_bdd(const char *c);
int ajouter_playlist_bdd(const char *n);
int get_id_playlist(const char *n);
int get_id_musique(const char *c);
void ajouter_compo_bdd(int p, int m);

#endif
