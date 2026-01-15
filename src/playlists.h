#ifndef PLAYLISTS_H
#define PLAYLISTS_H
#include "modeles.h"

void charger_listes_annexes();
void charger_contenu_playlist(const char *n);
void charger_contenu_artiste(const char *nom_artiste);
void on_creer_pl_clicked(GtkWidget *b, gpointer u);
void on_artiste_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u);
void on_pl_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u);

#endif
