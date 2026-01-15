#ifndef BASE_SQLITE_H
#define BASE_SQLITE_H
#include "modeles.h"

void init_db();
int ajouter_bdd(const char *c, const char *t, const char *a, const char *alb, int dur);
void supprimer_bdd(const char *c);

void modifier_infos_bdd(const char *chemin, const char *nouveau_titre, const char *nouveau_artiste, const char *nouveau_album);
void supprimer_playlist_bdd(const char *nom_playlist);

// V29 : Gestion des paroles en BDD (Table n°4)
void set_paroles_bdd(int id_musique, const char *contenu);
char* get_paroles_bdd(int id_musique);

int ajouter_playlist_bdd(const char *n);
int get_id_playlist(const char *n);
int get_id_musique(const char *c);
void ajouter_compo_bdd(int p, int m);

#endif
