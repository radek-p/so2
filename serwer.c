/* Radoslaw Piorkowski *
 * nr indeksu: 335451  */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "message.h"
#include "err.h"

/* Zmienne globalne: ============================================== */

int K, N;

/* Identyfikatory kolejek */
int request_msq_id;
int release_msq_id;
int permission_msq_id;


int * resources_count; /* Tablica o rozmiarze K (malloc) */

/* Procedury pomocnicze: ========================================== */

/* Zwalnia zaalokowana pamiec */
void free_memory() {
	free(resources_count);
}

/* Usuwa kolejki komunikatow */
void free_resources() {
	if (msgctl(request_msq_id, IPC_RMID, 0) == -1)
		syserr("Nie mozna usunac kolejki zadan.");

	if (msgctl(release_msq_id, IPC_RMID, 0) == -1)
		syserr("Nie mozna usunac kolejki wiad. o zwolnieniach zasobow.");

	if (msgctl(permission_msq_id, IPC_RMID, 0) == -1)
		syserr("Nie mozna usunac kolejki zezwolen.");
}

/* Zwalnianie zasoby i przygotowuje serwer do zakonczenia pracy     *
 * Moze byc wywolywana dopiero po zakonczeniu procedury init_server */
void clean_up() {
	free_memory();
	free_resources();
}

/* Zwalnia zasoby i konczy dzialanie serwera pod wplywam sygnalu */
void exit_server(int sig) {
	fprintf(stderr, "Serwer konczy prace wskutek sygnalu %d.\n", sig);

	clean_up();
	exit(0);
}

/* Fukcje zwalniajace zasoby i konczace dzialanie *
 * serwera 'wyjatkowo'.                           */
void fatal_error(const char * error_message) {
	clean_up();
	fatal(error_message);
}

void system_error(const char * error_message) {
	clean_up();
	syserr(error_message);
}

/* Sprawdza poprawnosc argumentow wywolania i wczytuje *
 * je na odpowienie zmienne                            */
void check_args(int argc, char const * argv[]) {
	if (argc != 3)
		fatal("Sposob uzycia: %s "
			"<liczba typow zasobow> "
			"<liczba dostepnych zasobow kazdego typu>\n", argv[0]);

	if (sscanf(argv[1], "%d", &K) < 1)
		fatal("Nie udalo sie wczytac liczby typow zasobow (K).");

	if (!(1 <= K && K <= 99))
		fatal("Niepoprawna liczba typow zasobow (K): %d", K);

	if (sscanf(argv[2], "%d", &N) < 1)
		fatal("Nie udalo sie wczytac liczby dostepnych zasobow (N).");

	if (!(2 <= N && N <= 9999))
		fatal("Niepoprawna liczba zasobow danego typu (N): %d", N);
}

/* Inicjalizuje zmienne globalne przed rozpoczeciem nasluchiwania */
void init_server() {

	/* Alokowanie pamieci */

	if ((resources_count = malloc(sizeof(int) * K)) == NULL)
		syserr("Nie udalo sie zaalokowac tablicy 'resources_count' rozmiaru K");

	/* Tworzenie kolejek komunikatow */
	fprintf(stderr, "%s\n", "Serwer tworzy kolejki.");

	if ((request_msq_id = msgget(REQUEST_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
		free_memory();
		syserr("Nie mozna utworzyc kolejki zadan.");
	}

	if ((release_msq_id = msgget(RELEASE_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
		free_memory();
		msgctl(request_msq_id, IPC_RMID, 0);
		syserr("Nie mozna utworzyc kolejki wiad. o zwolnieniach zasobow.");
	}

	if ((permission_msq_id = msgget(PERMISSION_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
		free_memory();
		msgctl(request_msq_id, IPC_RMID, 0);
		msgctl(release_msq_id, IPC_RMID, 0);
		syserr("Nie mozna utworzyc kolejki zezwolen.");
	}

	/* Po zakonczeniu tej procedury wszystkie zasoby sa zainicjalizowane */

	if (signal(SIGINT, exit_server) == SIG_ERR)
		system_error("signal");
}

/* Funkcja main i glowna petla serwera ============================ */

int main(int argc, char const *argv[]) {

	check_args(argc, argv);
	init_server();

	/* Serwer odczytuje kolejne zadania i tworzy *
	 * dla kazdego z nich nowy watek.            */
	Msg_request m1;

	for(;;) {
		fprintf(stderr, "Czekam na wiadomosc.\n");

		if (msgrcv(request_msq_id, &m1, MSG_REQUEST_SIZE, 0L, 0) <= 0)
			system_error("Nie powiodlo sie odbieranie wiadomosci.");

		fprintf(stderr, "Otrzymano wiadomosc PID: %ld, typ: %d, ile: %d\n", m1.msg_type, m1.res_type, m1.res_quantity);

		/* Tworzenie watku */
	}

	return 0;
}
