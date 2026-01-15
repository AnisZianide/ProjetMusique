CC = gcc

# ICI C'EST TRES IMPORTANT : taglib_c
CFLAGS = -Wall -Wextra -g -std=c99 `pkg-config --cflags gtk4 taglib_c sqlite3`

# ICI AUSSI : taglib_c
LIBS = `pkg-config --libs gtk4 taglib_c sqlite3` -lm -ldl -lpthread

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
