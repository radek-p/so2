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
#include <pthread.h>

#include "message.h"
#include "err.h"

/* Struktura przekazywana nowo tworzonemu watkowi: ================ */

typedef struct {
	pid_t pid1;
	pid_t pid2;
	int res_type;
	int res_quantity;
} ThreadArgs;

typedef enum {
	DEFAULT,
	CLOSING
} State;

/* Zmienne globalne: ============================================== */

int K, N;
State state;

/* Identyfikatory kolejek */
int request_msq_id;
int release_msq_id;
int permission_msq_id;

/* Tablice rozmiaru K */
int   * resources_count;
/* tablice zapisujace informacje o procesie zadajacym *
 * zasobow danego typu czekajacym na partnera.        */
pid_t * waiting_pid;
int   * waiting_quantity;

/* Mutex */
pthread_mutex_t mutex;

/* Procedury pomocnicze: ========================================== */

/* Zwalnia zaalokowana pamiec */
void free_memory() {
	free(resources_count);
	free(waiting_pid);
	free(waiting_quantity);
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
	state = CLOSING;
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

	state = DEFAULT;

	/* Inicjalizacja mutexow i zmiennych warunkowych */

	if (pthread_mutex_init(&mutex, 0) != 0)
		syserr("Nie udalo sie zainicjalizowac mutexa.");

	/* Alokowanie pamieci */

	if ((resources_count = malloc(sizeof(int) * K)) == NULL)
		syserr("Nie udalo sie zaalokowac tablicy 'resources_count' rozmiaru K");

	if ((waiting_pid = malloc(sizeof(pid_t) * K)) == NULL)
		syserr("Nie udalo sie zaalokowac tablicy 'waiting_pid' rozmiaru K");

	if ((waiting_quantity = malloc(sizeof(int) * K)) == NULL)
		syserr("Nie udalo sie zaalokowac tablicy 'waiting_quantity' rozmiaru K");

	for (int i = 0; i < K; ++i) {
		resources_count[i] = N;
		waiting_pid[i] = 0;
		waiting_quantity[i] = 0;
	}

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

/* Watek obslugujacy pare klientow ================================ */

void * thread(void * _args) {
	ThreadArgs * args = (ThreadArgs *) _args;
	pid_t pid1 = args->pid1;
	pid_t pid2 = args->pid2;
	int res_type = args->res_type;
	int res_quantity = args->res_quantity;
	free(args);

	fprintf(stderr, "Watek dla procesow %d i %d zostal utworzony (typ x ile: %dx%d)\n", pid1, pid2, res_type, res_quantity);

	return (void *) NULL;
}

/* Funkcja main i glowna petla serwera ============================ */

int main(int argc, char const *argv[]) {

	check_args(argc, argv);
	init_server();

	/* Serwer odczytuje kolejne zadania i tworzy *
	 * dla kazdego z nich nowy watek.            */
	Msg_request m1;
	ThreadArgs * args;
	pthread_t thread_descr;
	pthread_attr_t thread_attr;

	if (pthread_attr_init (&thread_attr) != 0)
		system_error("Error while initializing thread_attr");

	if (pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED) != 0)
		system_error("Error while setting detach state to PTHREAD_CREATE_DETACHED.");

	for(;;) {
		fprintf(stderr, "Czekam na wiadomosc.\n");

		if (msgrcv(request_msq_id, &m1, MSG_REQUEST_SIZE, 0L, 0) <= 0)
			system_error("Nie powiodlo sie odbieranie wiadomosci.");

		fprintf(stderr, "Otrzymano wiadomosc PID: %ld, typ: %d, ile: %d\n", m1.msg_type, m1.res_type, m1.res_quantity);
		/* Mutex nie jest potrzebny, tylko watek glowny *
		 * bedzie odwolywal sie do tych zmiennych       */

		/* Jesli nie odebrano jeszcze wiadomosci od klienta *
		 * zadajacego okreslonego typu zasobow              */
		if (!(0 <= m1.res_type && m1.res_type < K))
			fatal_error("Odebrano niepoprawny res_type.");

		if (waiting_pid[m1.res_type] == 0) {
			waiting_pid[m1.res_type] = (pid_t) m1.msg_type;
			waiting_quantity[m1.res_type] = m1.res_quantity;
		}
		else {
			fprintf(stderr, "PID1: %d, PID2: %d, ile: %d\n", waiting_pid[m1.res_type], (pid_t) m1.msg_type, waiting_quantity[m1.res_type] + m1.res_quantity);
			args = malloc(sizeof(ThreadArgs));
			args->pid1 = waiting_pid[m1.res_type];
			args->pid2 = (pid_t) m1.msg_type;
			args->res_type = m1.res_type;
			args->res_quantity = m1.res_quantity + waiting_quantity[m1.res_type];

			/* Tworzenie watku */

			if (pthread_create(&thread_descr, &thread_attr, thread, args) != 0)
				system_error("Blad podczas tworzenia watku.");

			/* Zwalniam miejsce w tablicy czekajacych na partnera. */
			waiting_pid[m1.res_type] = 0;
		}

	}

	return 0;
}
