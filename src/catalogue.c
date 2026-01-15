#include "catalogue.h"
#include "lecteur.h"
#include "base_sqlite.h"
#include "playlists.h" // Pour rafraichir si besoin
#include <stdio.h>

GtkWidget *entry_recherche;
GtkWidget *dd_tri;

void on_supprimer_clicked(GtkButton *b, gpointer u) {
    GtkWidget *r=GTK_WIDGET(u); char *c=(char*)g_object_get_data(G_OBJECT(b), "mon_chemin");
    if(c){ stop_musique(); supprimer_bdd(c); }
    GtkWidget *p=gtk_widget_get_parent(r); gtk_list_box_remove(GTK_LIST_BOX(p), r); set_status("Supprimé.", 0);
}

// V28 : Logique de modification
void on_edit_response(GtkDialog *d, int r, gpointer u) {
    if (r == GTK_RESPONSE_OK) {
        // On récupère les widgets passés en data
        GtkWidget *content = gtk_dialog_get_content_area(d);
        // On navigue dans les enfants (c'est un peu "hacky" mais simple)
        // Structure : Box -> [Label, Entry(Titre), Label, Entry(Artiste), Label, Entry(Album)]
        GtkWidget *box = gtk_widget_get_first_child(content);
       
        GtkWidget *w = gtk_widget_get_first_child(box); // Label 1
        w = gtk_widget_get_next_sibling(w); // Entry Titre
        const char *nt = gtk_editable_get_text(GTK_EDITABLE(w));
       
        w = gtk_widget_get_next_sibling(w); // Label 2
        w = gtk_widget_get_next_sibling(w); // Entry Artiste
        const char *na = gtk_editable_get_text(GTK_EDITABLE(w));

        w = gtk_widget_get_next_sibling(w); // Label 3
        w = gtk_widget_get_next_sibling(w); // Entry Album
        const char *nal = gtk_editable_get_text(GTK_EDITABLE(w));
       
        char *chemin = (char*)u;
       
        // Mise à jour BDD
        modifier_infos_bdd(chemin, nt, na, nal);
       
        // Rafraichir tout
        charger_catalogue();
        charger_listes_annexes();
    }
    g_free(u); // Libérer le chemin dupliqué
    gtk_window_destroy(GTK_WINDOW(d));
}

void on_edit_clicked(GtkButton *b, gpointer u) {
    (void)u;
    char *c = (char*)g_object_get_data(G_OBJECT(b), "mon_chemin");
    if(!c) return;

    // Création fenêtre modale
    GtkWidget *d = gtk_dialog_new_with_buttons("Modifier Infos", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Enregistrer", GTK_RESPONSE_OK, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(d));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 20); gtk_widget_set_margin_end(box, 20);
    gtk_widget_set_margin_top(box, 20); gtk_widget_set_margin_bottom(box, 20);
   
    // Champs de saisie
    gtk_box_append(GTK_BOX(box), gtk_label_new("Nouveau Titre :"));
    GtkWidget *e_titre = gtk_entry_new(); gtk_box_append(GTK_BOX(box), e_titre);
   
    gtk_box_append(GTK_BOX(box), gtk_label_new("Nouvel Artiste :"));
    GtkWidget *e_artiste = gtk_entry_new(); gtk_box_append(GTK_BOX(box), e_artiste);
   
    gtk_box_append(GTK_BOX(box), gtk_label_new("Nouvel Album :"));
    GtkWidget *e_album = gtk_entry_new(); gtk_box_append(GTK_BOX(box), e_album);
   
    gtk_box_append(GTK_BOX(content), box);
   
    // On passe le chemin en data
    g_signal_connect(d, "response", G_CALLBACK(on_edit_response), g_strdup(c));
    gtk_window_present(GTK_WINDOW(d));
}

void on_playlist_select_resp(GtkDialog *d, int r, gpointer u) {
    int mid=(int)(intptr_t)u;
    if(r==GTK_RESPONSE_OK){
        GtkWidget *c=gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *ch=gtk_widget_get_first_child(c); GtkWidget *cb=NULL;
        while(ch){if(GTK_IS_DROP_DOWN(ch)){cb=ch;break;}ch=gtk_widget_get_next_sibling(ch);}
        if(cb){
            GtkStringList *m=GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(cb)));
            const char *n=gtk_string_list_get_string(m, gtk_drop_down_get_selected(GTK_DROP_DOWN(cb)));
            int p=get_id_playlist(n); if(p!=-1) ajouter_compo_bdd(p, mid);
        }
    } gtk_window_destroy(GTK_WINDOW(d));
}

void on_add_to_pl_clicked(GtkButton *b, gpointer u) {
    (void)u; char *c=(char*)g_object_get_data(G_OBJECT(b), "mon_chemin"); int m=get_id_musique(c); if(m==-1)return;
    GtkWidget *d=gtk_dialog_new_with_buttons("Choisir Playlist", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Ajouter", GTK_RESPONSE_OK, NULL);
    GtkWidget *ct=gtk_dialog_get_content_area(GTK_DIALOG(d));
    const char *a[100]; int k=0; sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); sqlite3_prepare_v2(db, "SELECT nom FROM playlists;", -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW && k<99) a[k++]=g_strdup((const char*)sqlite3_column_text(s, 0)); a[k]=NULL; sqlite3_finalize(s); sqlite3_close(db);
    if(k==0){ set_status("Aucune playlist !", 1); gtk_window_destroy(GTK_WINDOW(d)); return; }
    GtkWidget *dd=gtk_drop_down_new_from_strings(a); gtk_widget_set_margin_top(dd,20); gtk_widget_set_margin_bottom(dd,20); gtk_widget_set_margin_start(dd,20); gtk_widget_set_margin_end(dd,20);
    gtk_box_append(GTK_BOX(ct), dd); g_signal_connect(d, "response", G_CALLBACK(on_playlist_select_resp), (gpointer)(intptr_t)m); gtk_window_present(GTK_WINDOW(d));
}

void ajouter_ligne_visuelle_generique(GtkListBox *lb, const char *c, const char *t, const char *a, const char *alb, int dur, int pl) {
    GtkWidget *r=gtk_list_box_row_new();
    GtkWidget *x=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_widget_set_hexpand(x, TRUE);
   
    int min = dur / 60; int sec = dur % 60;
    char *txt = g_markup_printf_escaped("<b>%s</b>\n<span size='small' foreground='gray'>%s • %s</span>", t, a, alb ? alb : "Inconnu");
    char *txt_dur = g_strdup_printf("%02d:%02d", min, sec);

    GtkWidget *l=gtk_label_new(NULL); gtk_label_set_markup(GTK_LABEL(l), txt); g_free(txt);
    gtk_widget_set_hexpand(l, TRUE); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END);
    GtkWidget *l_dur = gtk_label_new(txt_dur); gtk_widget_set_margin_end(l_dur, 10); g_free(txt_dur);

    gtk_box_append(GTK_BOX(x), gtk_image_new_from_icon_name("audio-x-generic"));
    gtk_box_append(GTK_BOX(x), l);
    gtk_box_append(GTK_BOX(x), l_dur);

    // V28 : Bouton Modifier (Crayon)
    GtkWidget *b_edit = gtk_button_new_from_icon_name("document-edit-symbolic");
    g_object_set_data_full(G_OBJECT(b_edit), "mon_chemin", g_strdup(c), g_free);
    g_signal_connect(b_edit, "clicked", G_CALLBACK(on_edit_clicked), NULL);
    gtk_box_append(GTK_BOX(x), b_edit);

    if(pl){
        GtkWidget *ba=gtk_button_new_from_icon_name("list-add-symbolic");
        g_object_set_data_full(G_OBJECT(ba), "mon_chemin", g_strdup(c), g_free);
        g_signal_connect(ba, "clicked", G_CALLBACK(on_add_to_pl_clicked), NULL); gtk_box_append(GTK_BOX(x), ba);
    }
    GtkWidget *bd=gtk_button_new_from_icon_name("user-trash-symbolic");
    g_object_set_data_full(G_OBJECT(bd), "mon_chemin", g_strdup(c), g_free);
    g_signal_connect(bd, "clicked", G_CALLBACK(on_supprimer_clicked), r);
    gtk_box_append(GTK_BOX(x), bd);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), x); gtk_list_box_append(lb, r);
}

void charger_catalogue() {
    GtkWidget *c=gtk_widget_get_first_child(list_box_catalogue);
    while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), c); c=n; }
   
    const char *recherche = gtk_editable_get_text(GTK_EDITABLE(entry_recherche));
    int tri_id = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd_tri));
   
    char sql[1024];
    char *order_by = "id DESC";
    if (tri_id == 1) order_by = "titre ASC";
    if (tri_id == 2) order_by = "artiste ASC";

    snprintf(sql, sizeof(sql), "SELECT chemin, titre, artiste, album, duree FROM musiques WHERE (titre LIKE '%%%s%%' OR artiste LIKE '%%%s%%') ORDER BY %s;",
        recherche ? recherche : "", recherche ? recherche : "", order_by);

    sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, sql, -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW) {
        ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_catalogue),
            (const char*)sqlite3_column_text(s,0), (const char*)sqlite3_column_text(s,1), (const char*)sqlite3_column_text(s,2),
            (const char*)sqlite3_column_text(s,3), sqlite3_column_int(s,4), 1);
    }
    sqlite3_finalize(s); sqlite3_close(db);
}
