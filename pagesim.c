#include "pagesim.h"
#include <stdio.h>
#include <stdlib.h>

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

int debug = 1;

int first_time_init = 1;
int first_time_end = 1;

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

// Simulates 2D arrays
int get_frame(int index, position pos)
{
	return frames[index + pos * mem_size];
}

void set_frame(int index, position pos, int value)
{
	frames[index + pos * mem_size] = value;
}

int is_page_available(int page_number)
{
	int ret = -1;
	size_t i;
	for(i = 0; i < mem_size; i++)
		if(get_frame(i, ST) == page_number)
		{
			ret = i;
			break;
		}

	return ret;
}

int page_sim_init(unsigned _page_size, unsigned _mem_size,
				  unsigned _addr_space_size, unsigned _strategy,
				  unsigned _max_concurrent_operations,
				  pagesim_callback _callback)
{
	if(debug)
		printf("page_sim_init(%u, %u, %u, %u, %u, callback) called \n", page_size, mem_size, addr_space_size, strategy, max_concurrent_operations);

	// TODO errno
	if(!first_time_init)
		return -1;

	first_time_init = 0;


	page_size = _page_size;
	mem_size = _mem_size;
	addr_space_size = _addr_space_size;
	strategy = _strategy;
	max_concurrent_operations = _max_concurrent_operations;
	/* callback = &_callback; */

	// TODO errors?
	frames = (int *) malloc(2 * mem_size);
	virtual_memory = (int*) malloc(page_size * mem_size);
	size_t i;
	for(i = 0; i < mem_size; i++)
	{
		set_frame(i, ST, -1);
		set_frame(i, ND, -1);
	}
	empty_frames = mem_size;

	return 0;
}


void page_sim_end()
{
	if(debug)
		printf("page_sim_end() called\n");

	// TODO errno?
	if(!first_time_end)
		return;

	first_time_end = 0;

	free(frames);
	free(virtual_memory);
}


int page_sim_get(unsigned a, uint8_t *v)
{
	return 0;
}

int page_sim_set(unsigned a, uint8_t v)
{
	return 0;
}
