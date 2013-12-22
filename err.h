/* Radoslaw Piorkowski *
 * nr indeksu: 335451  */

/* plik pochodzi ze strony p. Marcina Engela */

#ifndef _ERR_
#define _ERR_

/* wypisuje informacje o blednym zakonczeniu funkcji systemowej 
i konczy dzialanie */
extern void syserr(const char *fmt, ...);

/* wypisuje informacje o bledzie i konczy dzialanie */
extern void fatal(const char *fmt, ...);

#endif