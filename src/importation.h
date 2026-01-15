#ifndef IMPORTATION_H
#define IMPORTATION_H
#include "modeles.h"
#include <taglib/tag_c.h>

// V26 : Mise à jour de la signature (Album + Durée)
void recuperer_infos_mp3(const char *chemin, char *t_out, char *a_out, char *alb_out, int *dur_out, size_t size);
void on_add_clicked(GtkButton *b, gpointer u);

#endif
