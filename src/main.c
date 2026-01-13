#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

sqlite3 *db;
GtkWidget *main_listbox;

// --- FONCTIONS SQL ---

// Fonction pour supprimer une musique
void supprimer_chanson_bdd(int id) {
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM chansons WHERE id = %d;", id);
   
    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
   
    if (rc != SQLITE_OK) {
        g_print("Erreur suppression : %s\n", errMsg);
        sqlite3_free(errMsg);
    } else {
        g_print("Chanson ID %d supprimée.\n", id);
    }
}

// Fonction pour insérer
void ajouter_chanson_bdd(const char *t, const char *a, const char *an) {
    char sql[512];
    snprintf(sql, sizeof(sql), "INSERT INTO chansons (titre, artiste, annee) VALUES ('%s', '%s', %s);", t, a, an);
    sqlite3_exec(db, sql, 0, 0, 0);
}

// --- INTERFACE ---

// Callback du bouton suppression
static void on_delete_clicked(GtkWidget *btn, gpointer data) {
    // On récupère l'ID caché dans le bouton
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "id_chanson"));
   
    // 1. On supprime dans la BDD
    supprimer_chanson_bdd(id);
   
    // 2. On supprime visuellement la ligne (le parent du bouton est la box, le parent de la box est la ligne)
    GtkWidget *box = gtk_widget_get_parent(btn);
    GtkWidget *row = gtk_widget_get_parent(box); // La ligne de la ListBox
   
    // Astuce GTK4 : On demande à la liste de retirer cette ligne spécifique
    gtk_list_box_remove(GTK_LIST_BOX(main_listbox), row);
}

void rafraichir_liste() {
    // Vider la liste
    GtkWidget *child = gtk_widget_get_first_child(main_listbox);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(main_listbox), child);
        child = next;
    }

    // Remplir la liste
    const char *sql = "SELECT id, titre, artiste, annee FROM chansons ORDER BY id DESC";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *titre = (const char *)sqlite3_column_text(stmt, 1);
        const char *artiste = (const char *)sqlite3_column_text(stmt, 2);
        int annee = sqlite3_column_int(stmt, 3);
        char annee_str[10];
        sprintf(annee_str, "%d", annee);

        // Conteneur horizontal
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
       
        // Textes
        GtkWidget *lbl_titre = gtk_label_new(titre);
        GtkWidget *lbl_infos = gtk_label_new(NULL);
        char infos[256];
        snprintf(infos, sizeof(infos), "<span color='gray'>%s (%s)</span>", artiste, annee_str);
        gtk_label_set_markup(GTK_LABEL(lbl_infos), infos);

        // Bouton Supprimer (Rouge)
        GtkWidget *btn_del = gtk_button_new_with_label("X");
        gtk_widget_add_css_class(btn_del, "destructive-action"); // Style rouge standard GTK
       
        // *** MAGIE : On colle l'ID sur le bouton pour plus tard ***
        g_object_set_data(G_OBJECT(btn_del), "id_chanson", GINT_TO_POINTER(id));
        g_signal_connect(btn_del, "clicked", G_CALLBACK(on_delete_clicked), NULL);

        // Mise en page
        gtk_widget_set_hexpand(lbl_titre, TRUE);
        gtk_widget_set_halign(lbl_titre, GTK_ALIGN_START);
        gtk_label_set_markup(GTK_LABEL(lbl_titre), titre);
       
        // On rend le titre gras
        char markup[256];
        snprintf(markup, sizeof(markup), "<b>%s</b>", titre);
        gtk_label_set_markup(GTK_LABEL(lbl_titre), markup);

        gtk_box_append(GTK_BOX(box), lbl_titre);
        gtk_box_append(GTK_BOX(box), lbl_infos);
        gtk_box_append(GTK_BOX(box), btn_del);

        gtk_list_box_append(GTK_LIST_BOX(main_listbox), box);
    }
    sqlite3_finalize(stmt);
}

// --- RESTE DU CODE (Ajout, Main...) inchangé ou presque ---

static void on_save_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget **entries = (GtkWidget **)data;
    const char *titre = gtk_editable_get_text(GTK_EDITABLE(entries[0]));
    const char *artiste = gtk_editable_get_text(GTK_EDITABLE(entries[1]));
    const char *annee = gtk_editable_get_text(GTK_EDITABLE(entries[2]));

    if (g_utf8_strlen(titre, -1) > 0) {
        ajouter_chanson_bdd(titre, artiste, annee);
        rafraichir_liste();
        GtkWidget *win = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
        gtk_window_close(GTK_WINDOW(win));
    }
    free(entries);
}

static void on_add_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Ajout");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 20); gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20); gtk_widget_set_margin_end(box, 20);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget *e_titre = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e_titre), "Titre");
    GtkWidget *e_artiste = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e_artiste), "Artiste");
    GtkWidget *e_annee = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e_annee), "Année");

    gtk_box_append(GTK_BOX(box), e_titre);
    gtk_box_append(GTK_BOX(box), e_artiste);
    gtk_box_append(GTK_BOX(box), e_annee);

    GtkWidget *btn = gtk_button_new_with_label("Valider");
    gtk_box_append(GTK_BOX(box), btn);

    GtkWidget **entries = malloc(3 * sizeof(GtkWidget*));
    entries[0] = e_titre; entries[1] = e_artiste; entries[2] = e_annee;
    g_signal_connect(btn, "clicked", G_CALLBACK(on_save_clicked), entries);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Ma Playlist ESIEA");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    GtkWidget *header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *btn_add = gtk_button_new_with_label("+ Ajouter");
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), btn_add);

    sqlite3_open("data/musique.db", &db);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window), scroll);

    main_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(main_listbox), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), main_listbox);

    rafraichir_liste();
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.esiea.projet", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    if(db) sqlite3_close(db);
    return status;
}
