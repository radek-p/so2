/* Radoslaw Piorkowski *
 * nr indeksu: 335451  */

#ifndef _MESSAGE_
#define _MESSAGE_

#include <sys/types.h>

/* Klucze kolejek komunikatow do serwera */
#define	REQUEST_Q_KEY 1928375L
#define	RELEASE_Q_KEY 1928376L

/* Klucz kolejki komunikatow do klienta */
#define	PERMISSION_Q_KEY 1928377L

/* Prosba do serwera o przydzielenie zasobow */
typedef struct {

	long  msg_type;     /* PID klienta            */
	/* PID klienta musi byc rzutowany na typ long */
	int   res_type;     /* typ zasobu (1..K)      */
	int   res_quantity; /* ilość                  */

} Msg_request;

#define MSG_REQUEST_SIZE (2 * sizeof(int))

/* Zawiadomienie o zwolnieniu zasobow */
typedef struct {

	long  msg_type;     /* PID zwalniajacego      */
	int   res_type;     /* typ zasobu (1..K)      */
	int   res_quantity; /* ilość                  */

} Msg_release;

#define MSG_RELEASE_SIZE (2 * sizeof(int))

/* Wiadomosc do klienta o dostepnosci zasobow */
typedef struct {

	long  msg_type;     /* typ == PID adresata    */
	pid_t partner_pid;  /* PID partnera klienta   */

} Msg_permission;

#define MSG_PERMISSION_SIZE (sizeof(pid_t))

#endif