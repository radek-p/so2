# Radoslaw Piorkowski
# nr indeksu: 335451

# ten Makefile bazuje na pliku pochodzacym
# ze strony p. Marcina Engela

CC          := gcc
CFLAGS      := -Wall -Wextra -std=c99 -pedantic -D_SVID_SOURCE -pthread
LDFLAGS     := -Wall -Wextra -std=c99 -pedantic

# pliki zrodlowe
SOURCES     := $(wildcard *.c)

# pliki obiektowe, ktore zawieraja definicje funkcji main
MAINOBJECTS := $(subst .c,.o,$(shell grep -l main $(SOURCES)))

# pliki wykonywalne (powstaja ze zrodel zawierajacych definicje main)
ALL         := $(subst .o,,$(MAINOBJECTS))

# makefefiles dla kazdego z plikow zrodlowych
DEPENDS     := $(subst .c,.d,$(SOURCES))

# wszystkie pliki obiektowe
ALLOBJECTS  := $(subst .c,.o,$(SOURCES))

# pliki obiektowe, ktore nie zawieraja definicji main
OBJECTS	    := $(filter-out $(MAINOBJECTS),$(ALLOBJECTS)) 

all: $(DEPENDS) $(ALL)

# tworzenie submakefiles
# 1. zaleznosc
# 2. polecenie kompilacji
$(DEPENDS) : %.d : %.c
	$(CC) -MM $< > $@ 
	@echo -e: "\t"$(CC) $(CFLAGS) $< >> $@

# konsolidacja
$(ALL) : % : %.o $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^		

# dolaczenie submakefiles
-include $(DEPENDS)

clean:
	-rm -f *.o $(ALL) $(ALLOBJECTS) $(DEPENDS)