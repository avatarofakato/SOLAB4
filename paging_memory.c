#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>
#include "err.h"
#include <aio.h>
#include "paging_memory.h"

/* TODO ask about uint8_t = 1B */
/* TODO ask about parallel threads */

//number of concurrent IO/s per file copied
int NUM_ASYNC_IO;
int PAGE_SIZE;
int PAGE_NUMBER;

struct paging_file_info
{
	char *file_name;
	//file descriptors write/read
	int fdw;
	int fdr;
	struct aiocb *aiocb_blocks;
};

// Contains info whether aiocb_block is active
// and if so which page it is representing.
struct buff_page_info
{
	int is_active;
	int page_number;
};

pthread_mutex_t mutex;
pthread_mutex_t aio_requests;
pthread_mutex_t *aio_mutexes;

//buffer for NUM_ASYNC_IO read blocks of BUFFER_SIZE size
char *buffer;

//total number of outstanding reads
int total_aio_active;

//contains information whether page_file.aiocb_blocks[i] is active
struct buff_page_info *aiocb_info;

//aiocb blocks
/* struct aiocb read_aiocb_blocks[NUM_ASYNC_IO]; */

struct paging_file_info page_file;

// TODO sizeof(uint8_t)
char *get_page_address_in_buff(int buff_page_number)
{
	return buffer + buff_page_number * PAGE_SIZE;
}

// TODO sizeof(uint8_t)
int get_page_address_in_file(int page_number)
{
	return page_number * PAGE_SIZE;
}

void init_global(int _NUM_ASYNC_IO, int _PAGE_SIZE, int _PAGE_NUMBER)
{
	int NUM_ASYNC_IO = _NUM_ASYNC_IO;
	int PAGE_SIZE = _PAGE_SIZE;
	int PAGE_NUMBER = _PAGE_NUMBER;

	int i;
	page_file.file_name = malloc(16);
	page_file.file_name = "file.tmp";
	page_file.fdw = creat(page_file.file_name, 0644);
	page_file.fdr = open(page_file.file_name, O_RDONLY);
	page_file.aiocb_blocks = malloc(NUM_ASYNC_IO * sizeof(struct aiocb));
	if(page_file.aiocb_blocks == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	aiocb_info = malloc(NUM_ASYNC_IO * sizeof(struct buff_page_info));
	if(aiocb_info == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	// TODO shouldn't mutexes be also initialized?
	aio_mutexes =  malloc(NUM_ASYNC_IO * sizeof(pthread_mutex_t));
	if(aio_mutexes == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	for(i = 0; i < NUM_ASYNC_IO; i++)
	{
		aiocb_info[i].is_active = 0;
		aiocb_info[i].page_number = -1;
	}
	// TODO initialize as locked
	pthread_mutex_lock(&aio_requests);

	buffer = malloc(NUM_ASYNC_IO * PAGE_SIZE);
	// TODO is such memset ok for char array?
	/* memset(buffer, 0, NUM_ASYNC_IO * PAGE_SIZE); */
	for(i = 0; i < NUM_ASYNC_IO * PAGE_SIZE; i++)
		buffer[i] = '0';
	if(buffer == NULL)
	{
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}
}

void done_global()
{
	/* TODO why it doesn't work */
	/* free(page_file.file_name); */
	free(page_file.aiocb_blocks);
	free(buffer);
	free(aiocb_info);
	// TODO shouldn't mutexes be rather destroyed
	free(aio_mutexes);
	close(page_file.fdw);
	close(page_file.fdr);
}

// TODO DELETE
int find_debug = 0;
int find_and_book_empty_aio(int page_number)
{
	if(find_debug)
		printf("find_and_book_empty_aio(%d) called\n", page_number);
	int i;
	int ret;
	/* TODO consider aio_suspend of page_file.aiocb_blocks instead of mutexes */
	pthread_mutex_lock(&mutex);
	if(total_aio_active != NUM_ASYNC_IO)
	{
		// Firstly we are looking whether there is already some
		// working aio process on requested page_number.
		for(i = 0; i < NUM_ASYNC_IO; i++)
		{
			if(find_debug)
				printf("checking aiocb_info[%d] for working process on page %d\n", i, page_number);
			if(aiocb_info[i].is_active && aiocb_info[i].page_number == page_number)
			{
				if(find_debug)
					printf("there is working process on page %d (aiocb_info[%d])\n", page_number, i);
				ret = i;
				break;
			}
		}
		// If not then it remains to find not active one.
		if(i == NUM_ASYNC_IO)
		{
			if(find_debug)
				printf("there is no working process on page %d\n", page_number);
			for(i = 0; i < NUM_ASYNC_IO; i++)
			{
				if(find_debug)
					printf("checking aiocb_info[%d] for free aiocb\n", i);
				if(!aiocb_info[i].is_active)
				{
					if(find_debug)
						printf("aiocb_info[%d] is free\n", i);
					aiocb_info[i].is_active = 1;
					aiocb_info[i].page_number = page_number;
					ret = i;
					total_aio_active++;
					break;
				}
				else
					if(i == NUM_ASYNC_IO - 1)
						ret = -1;
			}
		}
	}
	if(find_debug)
	{
		if(ret == -1)
			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
		printf("find_and_book_empty_aio(%d)=%d\n", page_number, ret);
		if(ret == -1)
			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}

// TODO delete
int manage_debug = 0;
void print_info(int buff_page_number, int page_number, int buff_no, enum TYPE_OF_OPERATION type, char *s)
{
	if(manage_debug)
		printf("manage_page(%d,%d,%s) is alocated on %d (%s)\n", buff_page_number, page_number, type == READ ? "READ" : "WRITE", buff_no, s);
}

// buff_page_number reffers to absolute page's number at buffer
// page_number reffers to absolute page's number memory
void manage_page(int buff_page_number, int page_number, enum TYPE_OF_OPERATION type)
{
	int i = find_and_book_empty_aio(page_number);
	/* printf("i = %d\n", i); */
	/* printf(" */
	while(i == -1)
	{
		print_info(buff_page_number, page_number, i, type, "hanging on aio_requests");
		// hang due there is no empty aiocb structure
		pthread_mutex_lock(&aio_requests);
		i = find_and_book_empty_aio(page_number);
		/* print_info(buff_page_number, page_number, type,  */
		/* printf("i = %d\n", i); */
	}

	pthread_mutex_lock(&aio_mutexes[i]);
	// TODO not sure whether dup is neccessary
	// note: it completely doesn't work while duplicating file descriptor
	/* page_file.aiocb_blocks[i].aio_fildes = dup(page_file.fdw); */

	page_file.aiocb_blocks[i].aio_offset = get_page_address_in_file(page_number);
	page_file.aiocb_blocks[i].aio_nbytes = PAGE_SIZE;
	page_file.aiocb_blocks[i].aio_buf = get_page_address_in_buff(buff_page_number);
	page_file.aiocb_blocks[i].aio_sigevent.sigev_notify = SIGEV_NONE;

	print_info(buff_page_number, page_number, i, type, "aiocb structure is ready");

	// TODO error handling?
	int err;
	if(type == READ)
	{
		page_file.aiocb_blocks[i].aio_fildes = page_file.fdr;
		err = aio_read(&page_file.aiocb_blocks[i]);
	}
	else
	{
		page_file.aiocb_blocks[i].aio_fildes = page_file.fdw;
		err = aio_write(&page_file.aiocb_blocks[i]);
	}

	print_info(buff_page_number, page_number, i, type, "done");

	if(err == -1)
	{
		fprintf(stderr, "aio_write failed\n");
		exit(1);
	}

	pthread_mutex_lock(&mutex);
	total_aio_active--;
	pthread_mutex_unlock(&mutex);
	aiocb_info[i].is_active = 0;
	pthread_mutex_unlock(&aio_mutexes[i]);
	// TODO Do we need to check whether aio_requsests is really locked?
	pthread_mutex_unlock(&aio_requests);
}

int get_buff(unsigned a, uint8_t *v);
int set_buff(unsigned a, uint8_t v);

