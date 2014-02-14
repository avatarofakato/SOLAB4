#include "pagesim.h"
#include "err.h"
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

void callback(int op, int arg1, int arg2)
{
	printf("callback(%d, %d, %d) called\n", op, arg1, arg2);
}

#define ILEWATKOW 6
#define MAXSLEEP  3

int ile = 0;

pthread_mutex_t mutex;
void *watek (void *data) 
{
  pid_t pid = getpid();
  pid_t id = pthread_self();

  pthread_mutex_lock(&mutex);
  ile++;
  pthread_mutex_unlock(&mutex);
  int r = rand() % MAXSLEEP;
  printf ("Jestem %d watek. Moj PID=%d moje ID=%d pojde spac na %ds\n", ile, pid, id, r);

  sleep(r);

  printf ("Jestem %d watek. Moj PID=%d moje ID=%d wlasnie sie obudzilem\n", ile, pid, id);

  page_sim_get(id, &ile);

  return (void *) ile;
}

int main()
{
	srand(time(NULL));
	printf("simple_test is starting\n");

	if(page_sim_init(0, 1, 2, 3, 4, callback) != -1)
		printf("succesfull page_sim_init call\n");
	else
		printf("unsuccesfull page_sim_init call\n");

	page_sim_end();

  pthread_t th[ILEWATKOW];
  pthread_attr_t attr;
  int i, wynik;
  int blad;
  pid_t pid = getpid();

  printf("Proces %d tworzy watki\n", pid);
  
  if ((blad = pthread_attr_init(&attr)) != 0 )
    syserr(blad, "attrinit");
  
  if ((blad = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
    syserr(blad, "setdetach");
  
  for (i = 0; i < ILEWATKOW; i++)
    if ((blad = pthread_create(&th[i], &attr, watek, 0)) != 0)
      syserr(blad, "create");
  
  /* {  */
    /* printf ("Proces %d czekam na zakonczenie watkow\n", pid); */
    /* for (i = 0; i < ILEWATKOW; i++) { */
      /* if ((blad = pthread_join(th[i], (void **) &wynik)) != 0) */
	/* syserr(blad, "join"); */
      /* printf("Kod powrotu %d\n", wynik); */
    /* } */
  /* } */

  sleep(MAXSLEEP + 1);
  printf("KONIEC\n");

  if ((blad = pthread_attr_destroy (&attr)) != 0)
    syserr(blad, "attrdestroy");
  return 0;

	return 0;
}
