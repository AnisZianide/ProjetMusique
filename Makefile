CC = gcc
CFLAGS = -Wall `pkg-config --cflags gtk4`
LIBS_GTK = `pkg-config --libs gtk4`
LIBS_SQL = -lsqlite3

# Par défaut, on fabrique TOUT (l'appli et les tests)
all: app test_bdd test_gui

# 1. Ton nouveau programme principal (La fusion)
app: src/main.c
	$(CC) src/main.c -o bin/app_musique $(CFLAGS) $(LIBS_GTK) $(LIBS_SQL)

# 2. Ton ancien test de base de données (on le garde au cas où)
test_bdd: src/test_bdd.c
	$(CC) src/test_bdd.c -o bin/test_bdd $(LIBS_SQL)

# 3. Ton ancien test de fenêtre (on le garde aussi)
test_gui: src/test_gui.c
	$(CC) src/test_gui.c -o bin/test_gui $(CFLAGS) $(LIBS_GTK)

clean:
	rm -f bin/*
