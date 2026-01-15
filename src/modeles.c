#include "modeles.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Fonction pour nettoyer les espaces
void trim(char *s) {
    if (!s) return;
    char *p = s;
    int l = strlen(p);
    if (l == 0) return;
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}

// Fonction pour afficher les messages d'état (Vert/Rouge)
void set_status(const char *message, int is_error) {
    // On vérifie que le label existe avant d'écrire dedans
    if (!lbl_status) return;

    char *markup;
    if (is_error) {
        markup = g_markup_printf_escaped("<span foreground='#ff5555'><b>%s</b></span>", message);
    } else {
        markup = g_markup_printf_escaped("<span foreground='#1db954'><b>%s</b></span>", message);
    }
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

// Fonction pour trouver l'image ou le texte associé au MP3
char* get_associated_file(const char *mp3_path, const char *new_ext) {
    char *new_path = g_strdup(mp3_path);
    char *dot = strrchr(new_path, '.');
    if (dot) {
        *dot = '\0';
        char *final_path = g_strconcat(new_path, new_ext, NULL);
        g_free(new_path);
        if (access(final_path, F_OK) != -1) return final_path;
        g_free(final_path);
    } else {
        g_free(new_path);
    }
    return NULL;
}

// Fonction pour lire tout le texte d'un fichier
char* lire_contenu_fichier(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    fclose(f);
    return buffer;
}
