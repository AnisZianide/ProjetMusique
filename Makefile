# Compilateur
CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags gtk4)

# C'est ICI qu'on relie la base de données (-lsqlite3)
LIBS = $(shell pkg-config --libs gtk4) -lsqlite3 -lm -ldl -lpthread

# Fichiers
SRC = src/main.c
TARGET = bin/app_musique
DATA_DIR = data

# Règle par défaut
all: directories $(TARGET)

# Compilation
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

# Création automatique des dossiers bin et data
directories:
	@mkdir -p bin
	@mkdir -p $(DATA_DIR)

# Nettoyage
clean:
	rm -f $(TARGET)
	rm -f $(DATA_DIR)/*.db

.PHONY: all clean directories
