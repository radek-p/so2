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

#define MAX_K 99

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

/* Stan serwera */
int K, N;
State state;

/* Identyfikatory kolejek */
int request_msq_id;
int release_msq_id;
int permission_msq_id;

/* Mutex */
pthread_mutex_t mutex;

/* Tablice rozmiaru K */
int   resources_count[MAX_K];
int   waiting_for_resource_count[MAX_K];
int   first_waiting_count[MAX_K];

pid_t waiting_for_partner_pid[MAX_K];
int   waiting_for_partner_quantity[MAX_K];

/* Zmienne warunkowe */
pthread_cond_t waiting_for_resource_cond[MAX_K];
pthread_cond_t first_waiting_cond[MAX_K];

/* Procedury pomocnicze: ========================================== */

/* Usuwa kolejki komunikatow */
void free_resources() {
	msgctl(   request_msq_id, IPC_RMID, 0);
	msgctl(   release_msq_id, IPC_RMID, 0);
	msgctl(permission_msq_id, IPC_RMID, 0);
}

/* Zwalnia zasoby i konczy dzialanie serwera pod wplywam sygnalu */
void exit_server(int sig) {
	state = CLOSING;
	fprintf(stderr, "Serwer konczy prace wskutek sygnalu %d.\n", sig);
	free_resources();
	exit(0);
}

/* Fukcje zwalniajace zasoby i konczace dzialanie *
 * serwera 'wyjatkowo'.                           */
void fatal_error(const char * error_message) {
	free_resources();
	fatal(error_message);
}

void system_error(const char * error_message) {
	free_resources();
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

	/* Inicjalizacja mutexa, tablic i zmiennych warunkowych */

	if (pthread_mutex_init(&mutex, 0) != 0)
		syserr("Nie udalo sie zainicjalizowac mutexa.");

	for (int i = 0; i < K; ++i) {
		resources_count[i]              = N;
		waiting_for_resource_count[i]   = 0;
		waiting_for_partner_pid[i]      = 0;
		waiting_for_partner_quantity[i] = 0;
		first_waiting_count[i]          = 0;

		/* Inicjalizacja zmiennych warunkowych */
		if (pthread_cond_init(&waiting_for_resource_cond[i], 0) != 0)
			syserr("Blad przy inicjalizacji zmiennej warunkowej");

		if (pthread_cond_init(&first_waiting_cond[i], 0) != 0)
			syserr("Blad przy inicjalizacji zmiennej warunkowej");
	}

	/* Tworzenie kolejek komunikatow */

	if ((request_msq_id = msgget(REQUEST_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1)
		syserr("Nie mozna utworzyc kolejki zadan.");

	if ((release_msq_id = msgget(RELEASE_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
		msgctl(request_msq_id, IPC_RMID, 0);
		syserr("Nie mozna utworzyc kolejki wiad. o zwolnieniach zasobow.");
	}

	if ((permission_msq_id = msgget(PERMISSION_Q_KEY, 0600 | IPC_CREAT | IPC_EXCL)) == -1) {
		msgctl(request_msq_id, IPC_RMID, 0);
		msgctl(release_msq_id, IPC_RMID, 0);
		syserr("Nie mozna utworzyc kolejki zezwolen.");
	}

	if (signal(SIGINT, exit_server) == SIG_ERR)
		system_error("signal");

	/* Po zakonczeniu tej procedury wszystkie zasoby sa zainicjalizowane */
}

/* Watek obslugujacy pare klientow ================================ */

void * thread(void * _args) {
	ThreadArgs * args = (ThreadArgs *) _args;
	pid_t pid1 = args->pid1;
	pid_t pid2 = args->pid2;
	int res_type = args->res_type;
	int res_quantity = args->res_quantity;
	free(args);

	if (pthread_mutex_lock(&mutex) != 0)
		system_error("Nie mozna zablokowac mutexa.");

	fprintf(stderr, "Watek dla procesow %d i %d zostal utworzony (typ x ile: %dx%d)\n", pid1, pid2, res_type, res_quantity);

	/* Czekam na opuszczenie kolejki przez pierwszego czekajacego *
	 * Warunek na czekanie:                                       */
	if (
		(waiting_for_resource_count[res_type] > 0) || /* => Ktos juz czeka na swoja kolej        */
		(       first_waiting_count[res_type] > 0)    /* => Ktos juz czeka na dostepnosc zasobow */
	) {
		/* Byji juz inni czekajacy, wiec bezwarunkowo usypiam i czekam, az ktos mnie obudzi      */
		++waiting_for_resource_count[res_type];
		pthread_cond_wait(&waiting_for_resource_cond[res_type], &mutex);
		--waiting_for_resource_count[res_type];

		/* Ktos mnie obudzil, ale przedtem mogl wejsc inny watek, zatem czekam, az kolejka na    *
		 * ktorej pierwszy watek czeka na dostepnosc zasobow bedzie rzeczywiscie pusta.          */
		while (first_waiting_count[res_type] > 0) {
			++waiting_for_resource_count[res_type];
			pthread_cond_wait(&waiting_for_resource_cond[res_type], &mutex);
			--waiting_for_resource_count[res_type];
		}
	}
	
	/* Jesli nie ma dosc zasobow, czekam */

	while (res_quantity > resources_count[res_type]) {
		++first_waiting_count[res_type];
		pthread_cond_wait(&first_waiting_cond[res_type], &mutex);
		--first_waiting_count[res_type];
	}

	/* Pierwszy watek zwalnia miejsce, zatem budzi kolejnego */
	if (pthread_cond_signal(&waiting_for_resource_cond[res_type]) != 0)
    	system_error("Nie mozna wyslac sygnalu.");

	/* Jest dosc zasobow */

	resources_count[res_type] -= res_quantity;

	printf(
		"Watek %ld przydziela %d zasobow %d klientom %d %d, pozostalo %d zasobow.\n",
		(long) pthread_self(),
		res_quantity,
		res_type + 1,
		pid1,
		pid2,
		resources_count[res_type]
	);

	if (pthread_mutex_unlock(&mutex) != 0)
		system_error("Nie mozna odblokowac mutexa.");

	/* Wysylanie wiadomosci o przydzieleniu zasobow */

	Msg_permission m1 = {
		(long) pid1,
		pid2
	};

	Msg_permission m2 = {
		(long) pid2,
		pid1
	};

	if (msgsnd(permission_msq_id, &m1, MSG_PERMISSION_SIZE, 0) == -1)
		system_error("Blad podczas wysylania pozwolenia na zajecie zasobu.");

	if (msgsnd(permission_msq_id, &m2, MSG_PERMISSION_SIZE, 0) == -1)
		system_error("Blad podczas wysylania pozwolenia na zajecie zasobu.");

	/* Zasoby przydzielone, przechodzimy do fazy oczekiwania na ich zwrot */

	Msg_release mr;

	if (msgrcv(release_msq_id, &mr, MSG_RELEASE_SIZE, (long) pid1, 0) <= 0)
		system_error("Nie powiodlo sie odczytanie wiadomosci o zwolnieniu zasobow.");

	if (msgrcv(release_msq_id, &mr, MSG_RELEASE_SIZE, (long) pid2, 0) <= 0)
		system_error("Nie powiodlo sie odczytanie wiadomosci o zwolnieniu zasobow.");

	/* Zasoby zwolnione */

	if (pthread_mutex_lock(&mutex) != 0)
		system_error("Nie mozna zablokowac mutexa.");

	resources_count[res_type] += res_quantity;

	/* Budze watek, ktory mogl czekac na zasoby */
    if (pthread_cond_signal(&first_waiting_cond[res_type]) != 0)
    	system_error("Nie mozna wyslac sygnalu.");

	if (pthread_mutex_unlock(&mutex) != 0)
		system_error("Nie mozna odblokowac mutexa.");

	return 0;
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

		if (waiting_for_partner_pid[m1.res_type] == 0) {
			waiting_for_partner_pid[m1.res_type] = (pid_t) m1.msg_type;
			waiting_for_partner_quantity[m1.res_type] = m1.res_quantity;
		}
		else {
			fprintf(stderr, "PID1: %d, PID2: %d, ile: %d\n", waiting_for_partner_pid[m1.res_type], (pid_t) m1.msg_type, waiting_for_partner_quantity[m1.res_type] + m1.res_quantity);
			args = malloc(sizeof(ThreadArgs));
			args->pid1 = waiting_for_partner_pid[m1.res_type];
			args->pid2 = (pid_t) m1.msg_type;
			args->res_type = m1.res_type;
			args->res_quantity = m1.res_quantity + waiting_for_partner_quantity[m1.res_type];

			/* Tworzenie watku */

			if (pthread_create(&thread_descr, &thread_attr, thread, args) != 0)
				system_error("Blad podczas tworzenia watku.");

			/* Zwalniam miejsce w tablicy czekajacych na partnera. */
			waiting_for_partner_pid[m1.res_type] = 0;
		}

	}

	return 0;
}
