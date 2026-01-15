#ifndef CATALOGUE_H
#define CATALOGUE_H
#include "modeles.h"

extern GtkWidget *entry_recherche;
extern GtkWidget *dd_tri;

void charger_catalogue();
void ajouter_ligne_visuelle_generique(GtkListBox *lb, const char *c, const char *t, const char *a, int pl);
void on_supprimer_clicked(GtkButton *b, gpointer u);
void on_add_to_pl_clicked(GtkButton *b, gpointer u);

#endif
