#include "pagesim.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_mutex_t mutex;

// TODO do we need enum name?
enum {
	ALG_FIFO,
	ALG_CLOCK,
	ALG_WSCLOCK
};

typedef enum {
	ST,
	ND
} position;

// TODO probably we'd have nastepna_wolna = -1 initialized
typedef struct {
	int bylo_odwolanie;
	int czas_odwolania;
	int modyfikowana;
	int trwa_transmisja;
	int proces;
	int nastepna_wolna;
} frame;

int debug = 1;

int first_time_init = 1;
int end = 0;

unsigned number_of_pagesim_call = 0;
const unsigned T = 3;

int *virtual_memory;

int *frames;
int empty_frames;
int index_frames = 0;

unsigned page_size;
unsigned mem_size;
unsigned addr_space_size;
unsigned strategy;
unsigned max_concurrent_operations;
/* pagesim_callback *callback; */

int strzalka;
int wolna;
int usuwane;
int czekajacy;
int biezacy_proces;

frame *PaO;

// TODO
/* (* inicjuje przesłanie strony z danej ramki z pamięci operacyjnej do pomocniczej *) */
void UsunStrone(int nrramki)
{
}

// TODO
/* (* wstrzymuje działanie procesu aż do chwili najbliższego zakończenia usuwania */
int CzekajNaRamke()
{
	return 0;
}

// TODO
/* (* przekazuje numer procesu wyznaczonego do usunięcia *) */
int UsunProces()
{
	return 0;
}

void ZwolnijRamke(int nrramki)
{
	PaO[nrramki].nastepna_wolna = wolna;
	wolna = nrramki;

	PaO[nrramki].proces = 0;
	PaO[nrramki].trwa_transmisja = 0;
	PaO[nrramki].bylo_odwolanie = 0;
}

void WSClock(int *nrramki)
{
	int *znaleziono;
	int ostatni;
	int nrproc;

	if(wolna != -1)
	{
		*nrramki = wolna;
		wolna = PaO[wolna].nastepna_wolna;
	}
	else
	{
		*znaleziono = 0;
		ostatni = strzalka;
		while(*znaleziono || (strzalka == ostatni))
		{
			strzalka = (strzalka + 1) % mem_size;
			// trwa_transmisja == strona byla modyfikowana lub jest zapisywana na dysk
			if(!PaO[strzalka].trwa_transmisja)
			{
				if(PaO[strzalka].bylo_odwolanie)
				{
					PaO[strzalka].bylo_odwolanie = 0;
					PaO[strzalka].czas_odwolania = number_of_pagesim_call;
				}
				else
				{
					if(abs(number_of_pagesim_call - PaO[strzalka].czas_odwolania) > T)
					{
						if(PaO[strzalka].modyfikowana)
						{
							usuwane++;
							PaO[strzalka].trwa_transmisja = 1;
							UsunStrone(strzalka);
						}
						else
						{
							*znaleziono = 1;
							PaO[strzalka].proces = biezacy_proces;
							*nrramki = strzalka;
						}
					}
				}
			}
		}

		if(!(*znaleziono))
		{
			if(czekajacy >= usuwane)
				nrproc = UsunProces();

			size_t ostatni;
			for(ostatni = 0; ostatni < mem_size; ostatni++)
			{
				if(PaO[ostatni].proces == nrproc)
				{
					PaO[ostatni].trwa_transmisja = 1;
					usuwane++;
					UsunStrone(ostatni);
				}
				else
					ZwolnijRamke(ostatni);
			}
			
			if(wolna != -1)
			{
				*nrramki = wolna;
				wolna = PaO[wolna].nastepna_wolna;
			}
			else
			{
				czekajacy++;
				*nrramki = CzekajNaRamke();
			}
		}
	}
}

// Simulates 2D arrays TODO is mutex needed?
int get_frame(int index, position pos)
{
	pthread_mutex_lock(&mutex);
	return frames[index + pos * mem_size];
	pthread_mutex_unlock(&mutex);
}

void set_frame(int index, position pos, int value)
{
	pthread_mutex_lock(&mutex);
	frames[index + pos * mem_size] = value;
	pthread_mutex_unlock(&mutex);
}

// If page is already loaded into some frame, returns its index otherwise -1
int is_page_available(int page_number)
{
	size_t i;
	for(i = 0; i < mem_size; i++)
	{
		pthread_mutex_lock(&mutex);
		if(get_frame(i, ST) == page_number)
			return i;
		pthread_mutex_unlock(&mutex);
	}

	return -1;
}

int page_sim_init(unsigned _page_size, unsigned _mem_size,
				  unsigned _addr_space_size, unsigned _strategy,
				  unsigned _max_concurrent_operations,
				  pagesim_callback _callback)
{
	if(debug)
		printf("page_sim_init(%u, %u, %u, %u, %u, callback) called \n",
	_page_size, _mem_size, _addr_space_size, _strategy,
	_max_concurrent_operations);

	// TODO errno
	pthread_mutex_lock(&mutex);
	if(!first_time_init || end)
	{
		pthread_mutex_unlock(&mutex);
		return -1;
	}

	page_size = _page_size;
	mem_size = _mem_size;
	addr_space_size = _addr_space_size;
	strategy = _strategy;
	max_concurrent_operations = _max_concurrent_operations;
	/* callback = &_callback; */

	// TODO errors?
	frames = (int *) malloc(2 * mem_size);
	PaO = (frame*) malloc(mem_size);
	virtual_memory = (int*) malloc(page_size * mem_size);
	size_t i;
	pthread_mutex_unlock(&mutex);

	for(i = 0; i < mem_size; i++)
	{
		set_frame(i, ST, -1);
		set_frame(i, ND, -1);
	}

	empty_frames = mem_size;
	first_time_init = 0;
	pthread_mutex_unlock(&mutex);

	return 0;
}


void page_sim_end()
{
	if(debug)
		printf("page_sim_end() called\n");

	// TODO errno?
	pthread_mutex_lock(&mutex);
	if(end)
	{
		pthread_mutex_unlock(&mutex);
		return;
	}

	end = 1;

	free(frames);
	free(PaO);
	free(virtual_memory);
	pthread_mutex_unlock(&mutex);
}


int page_sim_get(unsigned a, uint8_t *v)
{
	if(debug)
		printf("page_sim_get(%u, %u) called pthread_self()=%d\n", a, *v, pthread_self());
	pthread_mutex_lock(&mutex);
	if(first_time_init || end)
	{
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	number_of_pagesim_call++;
	pthread_mutex_unlock(&mutex);
	return 0;
}

int page_sim_set(unsigned a, uint8_t v)
{
	if(debug)
		printf("page_sim_get(%u, %u) called\n", a, v);
	pthread_mutex_lock(&mutex);
	if(first_time_init || end)
	{
		pthread_mutex_unlock(&mutex);
		return -1;
	}
	number_of_pagesim_call++;
	int page = a / page_size;
	int place = a % page_size;
	pthread_mutex_unlock(&mutex);
	return 0;
}
