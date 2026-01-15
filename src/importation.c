#include "importation.h"
#include "base_sqlite.h"
#include "catalogue.h"
#include "playlists.h"
#include <gtk/gtk.h>

void recuperer_infos_mp3(const char *chemin, char *t_out, char *a_out, size_t size) {
    int use_filename = 1;
    // TagLib détecte automatiquement le format (MP3, FLAC, OGG, WAV)
    TagLib_File *f = taglib_file_new(chemin);
   
    if(f && taglib_file_is_valid(f)){
        TagLib_Tag *t = taglib_file_tag(f);
        if(t){
            char *tt = taglib_tag_title(t);
            char *ta = taglib_tag_artist(t);
            if(tt && strlen(tt) > 0 && ta && strlen(ta) > 0) {
                snprintf(t_out, size, "%s", tt);
                snprintf(a_out, size, "%s", ta);
                use_filename = 0;
            }
        }
        taglib_file_free(f);
    }
   
    if(use_filename) {
        char *nom = g_path_get_basename(chemin);
        char *dot = strrchr(nom, '.');
        if(dot) *dot='\0';
       
        // Tentative de découpage "Artiste - Titre" si le tag échoue
        char *sep = strstr(nom, " - ");
        if(sep) {
            *sep = '\0';
            snprintf(a_out, size, "%s", nom);
            snprintf(t_out, size, "%s", sep + 3);
        } else {
            snprintf(t_out, size, "%s", nom);
            snprintf(a_out, size, "Artiste inconnu");
        }
        g_free(nom);
    }
    trim(t_out); trim(a_out);
}

void on_file_resp(GtkNativeDialog *d, int r, gpointer u) {
    (void)u;
    if(r==GTK_RESPONSE_ACCEPT){
        GFile *f = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));
        char *p = g_file_get_path(f);
        if(p){
            char t[256], a[256];
            recuperer_infos_mp3(p,t,a,256);
           
            if(ajouter_bdd(p,t,a)){
                charger_catalogue();
                charger_listes_annexes();
                set_status("Importé.", 0);
            } else {
                set_status("Doublon ou Erreur.", 1);
            }
            g_free(p);
        }
        g_object_unref(f);
    }
    g_object_unref(d);
}

// C'EST ICI QUE CA CHANGE (V24)
void on_add_clicked(GtkButton *b, gpointer u) {
    (void)b; (void)u;
    GtkFileChooserNative *n = gtk_file_chooser_native_new("Ajouter Musique", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, "Ouvrir", "Annuler");
   
    GtkFileFilter *f = gtk_file_filter_new();
    gtk_file_filter_set_name(f, "Fichiers Audio (MP3, WAV, OGG, FLAC)");
   
    // On autorise tous les formats demandés par le sujet
    gtk_file_filter_add_pattern(f, "*.mp3");
    gtk_file_filter_add_pattern(f, "*.wav");
    gtk_file_filter_add_pattern(f, "*.ogg");
    gtk_file_filter_add_pattern(f, "*.flac");
   
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(n), f);
    g_signal_connect(n, "response", G_CALLBACK(on_file_resp), NULL);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(n));
}
