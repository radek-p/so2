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

/* Zmienne globalne ================================================ */

int k, n, s;

/* Identyfikatory kolejek */
int request_msq_id;
int release_msq_id;
int permission_msq_id;

/* Funkcje pomocnicze ============================================== */
void get_msg_queues() {
	if ((request_msq_id = msgget(REQUEST_Q_KEY, 0)) == -1)
		syserr("Nie znaleziono kolejki komunikatow zadan.");

	if ((release_msq_id = msgget(RELEASE_Q_KEY, 0)) == -1)
		syserr("Nie znaleziono kolejki komunikatow zwolnienia zasobow.");

	if ((permission_msq_id = msgget(PERMISSION_Q_KEY, 0)) == -1)
		syserr("Nie znaleziono kolejki komunikatow pozwolen.");
}

/* Inicjuje odpowienie zmienne globalne na podstawie argumentow *
 * wywolania programu.                                          */
void process_args(int argc, char const *argv[]) {
	if (argc != 4)
		fatal("Sposob uzycia: %s "
			"<zadany typ zasobu> "
			"<liczba zasobow danego typu> "
			"<czas spania>\n", argv[0]);

	if (sscanf(argv[1], "%d", &k) < 1)
		fatal("Nie udalo sie wczytac typu zasobu (k).");

	if (!(1 <= k && k <= 99))
		fatal("Niepoprawny typ zasobu (k): %d", k);

	k--;

	if (sscanf(argv[2], "%d", &n) < 1)
		fatal("Nie udalo sie wczytac liczby zasobow (n).");

	if (!(2 <= n && n <= 9999))
		fatal("Niepoprawna liczba zasobow danego typu (n): %d", n);

	if (sscanf(argv[3], "%d", &s) < 1)
		fatal("Nie udalo sie wczytac czasu snu (s).");

	if (!(0 <= s))
		fatal("Niepoprawny czas spania (s): %d", s);
}

int main(int argc, char const *argv[]) {

	process_args(argc, argv);
	get_msg_queues();

	/* Przygotowywanie wiadomosci do wyslania */
	Msg_request m1 = {
		((long) getpid()), /* typ wiadomosci: PID klienta */
		k,                 /* typ zasobu                  */
		n                  /* liczba zasobow              */
	};

	if (msgsnd(request_msq_id, &m1, MSG_REQUEST_SIZE, 0) == -1)
		syserr("Blad podczas wysylania wiadomosci.");

	Msg_permission m2;

	fprintf(stderr, "Wyslalem wiadomosc. Czekam na odpowiedz.\n");

	if (msgrcv(permission_msq_id, &m2, MSG_PERMISSION_SIZE, (long) getpid(), 0) <= 0)
		syserr("Nie powiodlo sie odczytanie wiadomosci.");

	printf("%d %d %d %d", k, n, getpid(), m2.partner_pid);

	/* Usypiam na s sekund */
	sleep(s);

	Msg_release m3 = {
		((long) getpid()), /* typ wiadomosci: PID klienta */
	};

	if (msgsnd(release_msq_id, &m3, MSG_RELEASE_SIZE, 0) == -1)
		syserr("Blad podczas wysylania wiadomosci o zwolnieniu zasobu.");

	printf("%d KONIEC\n", getpid());

	return 0;
}