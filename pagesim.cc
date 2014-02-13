#include "pagesim.h"
#include <stdio.h>
#include <stdlib.h>

// TODO do we need enum name?
enum {
	ALG_FIFO,
	ALG_CLOCK,
	ALG_WSCLOCK
};

int debug = 1;

int first_time_init = 1;
int first_time_end = 1;

int *virtual_memory;

int page_sim_init(unsigned page_size, unsigned mem_size,
				  unsigned addr_space_size, unsigned strategy,
				  unsigned max_concurrent_operations,
				  pagesim_callback callback)
{
	// TODO errno?
	if(!first_time_init)
		return -1;

	first_time_init = 0;

	virtual_memory = (int*) malloc(page_size * addr_space_size);

	if(debug)
		printf("page_sim_init(%u, %u, %u, %u, %u, callback) called \n", page_size, mem_size, addr_space_size, strategy, max_concurrent_operations);

	callback(-42, 0, 42);
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

	free(virtual_memory);
}
