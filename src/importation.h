#ifndef IMPORTATION_H
#define IMPORTATION_H
#include "modeles.h"
#include <taglib/tag_c.h>

void recuperer_infos_mp3(const char *chemin, char *t_out, char *a_out, size_t size);
void on_add_clicked(GtkButton *b, gpointer u);

#endif
