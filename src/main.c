#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>

sqlite3 *db;

// Fonction qui fabrique une "ligne" visuelle pour une chanson
GtkWidget* creer_ligne_chanson(const char *titre, const char *artiste, int annee) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);

    char annee_str[10];
    sprintf(annee_str, "%d", annee);

    GtkWidget *lbl_titre = gtk_label_new(titre);
    GtkWidget *lbl_artiste = gtk_label_new(artiste);
    GtkWidget *lbl_annee = gtk_label_new(annee_str);

    // Le titre prend toute la place
    gtk_widget_set_hexpand(lbl_titre, TRUE);
    gtk_widget_set_halign(lbl_titre, GTK_ALIGN_START);

    gtk_box_append(GTK_BOX(box), lbl_titre);
    gtk_box_append(GTK_BOX(box), lbl_artiste);
    gtk_box_append(GTK_BOX(box), lbl_annee);

    return box;
}

// Fonction qui va lire la BDD et remplir la liste
void charger_chansons(GtkWidget *listbox) {
    const char *sql = "SELECT titre, artiste, annee FROM chansons";
    sqlite3_stmt *stmt;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    if (rc != SQLITE_OK) {
        g_print("Erreur SQL : %s\n", sqlite3_errmsg(db));
        return;
    }

    // Tant qu'il y a des lignes, on les ajoute
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *titre = (const char *)sqlite3_column_text(stmt, 0);
        const char *artiste = (const char *)sqlite3_column_text(stmt, 1);
        int annee = sqlite3_column_int(stmt, 2);

        GtkWidget *ligne = creer_ligne_chanson(titre, artiste, annee);
        gtk_list_box_append(GTK_LIST_BOX(listbox), ligne);
    }

    sqlite3_finalize(stmt);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *listbox;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Ma Playlist ESIEA");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    // Connexion
    sqlite3_open("data/musique.db", &db);

    // Zone de défilement (Scroll)
    scrolled_window = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window), scrolled_window);

    // Liste
    listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), listbox);

    // Remplissage
    charger_chansons(listbox);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.esiea.projet", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    if(db) sqlite3_close(db);
    return status;
}
