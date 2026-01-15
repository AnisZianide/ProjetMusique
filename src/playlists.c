#include "playlists.h"
#include "catalogue.h"
#include "base_sqlite.h"
#include <stdio.h>

// On doit déclarer la fonction ici car elle est dans catalogue.c
void ajouter_ligne_visuelle_generique(GtkListBox *lb, const char *c, const char *t, const char *a, const char *alb, int dur, int pl);

void charger_contenu_playlist(const char *n) {
    GtkWidget *c=gtk_widget_get_first_child(list_box_contenu_playlist); while(c){ GtkWidget *x=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_contenu_playlist), c); c=x; }
    sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db);
    char q[512];
    // V26 : On récupère aussi album et durée
    sprintf(q, "SELECT m.chemin, m.titre, m.artiste, m.album, m.duree FROM musiques m JOIN compositions c ON m.id=c.musique_id JOIN playlists p ON p.id=c.playlist_id WHERE p.nom='%s';", n);
    sqlite3_prepare_v2(db, q, -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW)
        ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_contenu_playlist),
            (const char*)sqlite3_column_text(s,0),
            (const char*)sqlite3_column_text(s,1),
            (const char*)sqlite3_column_text(s,2),
            (const char*)sqlite3_column_text(s,3),
            sqlite3_column_int(s,4),
            0);
    sqlite3_finalize(s); sqlite3_close(db);
}

void charger_contenu_artiste(const char *nom_artiste) {
    GtkWidget *c = gtk_widget_get_first_child(list_box_contenu_artiste); while(c) { GtkWidget *x=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_contenu_artiste), c); c=x; }
    sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db);
    char q[512];
    sprintf(q, "SELECT chemin, titre, artiste, album, duree FROM musiques WHERE artiste='%s';", nom_artiste);
    sqlite3_prepare_v2(db, q, -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW) {
        ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_contenu_artiste),
            (const char*)sqlite3_column_text(s, 0),
            (const char*)sqlite3_column_text(s, 1),
            (const char*)sqlite3_column_text(s, 2),
            (const char*)sqlite3_column_text(s, 3),
            sqlite3_column_int(s, 4),
            1);
    }
    sqlite3_finalize(s); sqlite3_close(db);
}

void on_artiste_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) { (void)b; (void)u; GtkWidget *label = gtk_list_box_row_get_child(r); const char *nom_artiste = gtk_label_get_text(GTK_LABEL(label)); charger_contenu_artiste(nom_artiste); }
void on_pl_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) { (void)b; (void)u; GtkWidget *x=gtk_list_box_row_get_child(r); GtkWidget *l=gtk_widget_get_last_child(x); charger_contenu_playlist(gtk_label_get_text(GTK_LABEL(l))); }

void charger_listes_annexes() {
    GtkWidget *c; c=gtk_widget_get_first_child(list_box_artistes); while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_artistes), c); c=n; }
    sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); sqlite3_prepare_v2(db, "SELECT DISTINCT artiste FROM musiques ORDER BY artiste;", -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW){
        GtkWidget *r=gtk_list_box_row_new(); GtkWidget *l=gtk_label_new((const char*)sqlite3_column_text(s,0));
        gtk_widget_set_margin_start(l,10); gtk_widget_set_margin_top(l,10); gtk_widget_set_margin_bottom(l,10); gtk_widget_set_halign(l, GTK_ALIGN_START);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), l); gtk_list_box_append(GTK_LIST_BOX(list_box_artistes), r);
    } sqlite3_finalize(s);
    c=gtk_widget_get_first_child(list_box_playlists); while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_playlists), c); c=n; }
    sqlite3_prepare_v2(db, "SELECT nom FROM playlists ORDER BY nom;", -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW){
        GtkWidget *r=gtk_list_box_row_new(); GtkWidget *b=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); GtkWidget *l=gtk_label_new((const char*)sqlite3_column_text(s,0));
        gtk_box_append(GTK_BOX(b), gtk_image_new_from_icon_name("folder-music-symbolic")); gtk_box_append(GTK_BOX(b), l); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), b); gtk_list_box_append(GTK_LIST_BOX(list_box_playlists), r);
    } sqlite3_finalize(s); sqlite3_close(db);
}

void on_creer_pl_resp(GtkDialog *d, int r, gpointer u) {
    if(r==GTK_RESPONSE_OK){ const char *t=gtk_editable_get_text(GTK_EDITABLE(u)); if(t&&strlen(t)>0){ ajouter_playlist_bdd(t); charger_listes_annexes(); set_status("Playlist créée !", 0); } }
    gtk_window_destroy(GTK_WINDOW(d));
}
void on_creer_pl_clicked(GtkWidget *b, gpointer u) {
    (void)b; (void)u; GtkWidget *d=gtk_dialog_new_with_buttons("Nouvelle", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Créer", GTK_RESPONSE_OK, NULL);
    GtkWidget *c=gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *e=gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e), "Nom...");
    gtk_widget_set_margin_start(e,20); gtk_widget_set_margin_end(e,20); gtk_widget_set_margin_top(e,20); gtk_widget_set_margin_bottom(e,20);
    gtk_box_append(GTK_BOX(c), e); g_signal_connect(d, "response", G_CALLBACK(on_creer_pl_resp), e); gtk_window_present(GTK_WINDOW(d));
}
