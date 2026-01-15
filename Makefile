CC = gcc

# ⚠️ CORRECTION ICI : on utilise 'taglib_c' au lieu de 'taglib'
CFLAGS = -Wall -Wextra -g -std=c99 `pkg-config --cflags gtk4 taglib_c sqlite3`

# ⚠️ CORRECTION ICI AUSSI : 'taglib_c' pour le linkeur
LIBS = `pkg-config --libs gtk4 taglib_c sqlite3` -lm -ldl -lpthread

# Liste des sources
SRC = src/main.c \
      src/base_sqlite.c \
      src/lecteur.c \
      src/karaoke.c \
      src/importation.c \
      src/catalogue.c \
      src/playlists.c \
      src/modeles.c

OBJ = $(SRC:.c=.o)

all: bin/app_musique

bin/app_musique: $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o bin/app_musique $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o bin/app_musique
