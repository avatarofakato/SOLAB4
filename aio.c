#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdint.h>
#include <pthread.h>

#include <aio.h>
/* TODO ask about uint8_t = 1B */

enum
{
	//number of concurrent IO/s per file copied
	NUM_ASYNC_IO = 20,
	PAGE_SIZE = 8,
	PAGE_NUMBER = 32
};

enum TYPE_OF_OPERATION
{
	READ,
	WRITE
};

struct paging_file_info
{
	char *file_name;
	//file descriptors
	int fdw;
	int fdr;
	struct aiocb *aiocb_blocks;
};

// Contains info about whether aiocb_block is active
// and if so which page is representing.
struct buff_page_info
{
	int is_active;
	int page_number;
	/* int just_reserved; */
};

pthread_mutex_t mutex;
/* pthread_mutex_t aio_mutex; */
pthread_mutex_t aio_requests;
pthread_mutex_t *aio_mutexes;

//number of pointers to aiocb structures in aiocb_ptrs array
int num_aiocb_ptrs;

//buffer for NUM_ASYNC_IO read blocks of BUFFER_SIZE size
char *buffer;

//total number of outstanding reads
int total_aio_active;

//contains information whether page_file.aiocb_blocks[i] is active
struct buff_page_info *aiocb_info;

//aiocb blocks
/* struct aiocb read_aiocb_blocks[NUM_ASYNC_IO]; */

struct paging_file_info page_file;

void init_global()
{
	int i;
	page_file.file_name = malloc(16);
	page_file.file_name = "paging_file.tmp";
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
	// TODO is such memset ok for cahr array?
	memset(buffer, 0, NUM_ASYNC_IO * PAGE_SIZE);
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

// TODO sizeof(uint8_t)
char *get_page_address_in_buff(int buff_page_number)
{
	return buffer + buff_page_number * PAGE_SIZE;
}

int find_and_book_empty_aio(int page_number)
{
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
			if(aiocb_info[i].is_active && aiocb_info[i].page_number == page_number)
			{
				ret = i;
				/* aiocb_info[i].just_reserved = 0; */
				break;
			}
		}
		// If not then it remains to find not active one.
		if(i == NUM_ASYNC_IO)
		{
			for(i = 0; i < NUM_ASYNC_IO; i++)
			{
				if(!aiocb_info[i].is_active)
				{
					aiocb_info[i].is_active = 1;
					aiocb_info[i].page_number = page_number;
					/* aiocb_info[i].just_reserved = 1; */
					ret = i;
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&mutex);
	return ret;
}

void print_info(int buff_page_number, int page_number, enum TYPE_OF_OPERATION type)
{
	printf("manage_page(%d,%d,%s)\n", buff_page_number, page_number, type == READ ? "READ" : "WRITE");
}

void manage_page(int buff_page_number, int page_number, enum TYPE_OF_OPERATION type)
{
	int i = find_and_book_empty_aio(page_number);
	printf("i = %d\n", i);
	/* printf(" */
	while(i == -1)
	{
		print_info(buff_page_number, page_number, type);
		printf("hanging on aio_requests\n");
		// hang due there is no empty aiocb structure
		pthread_mutex_lock(&aio_requests);
		i = find_and_book_empty_aio(page_number);
		print_info(buff_page_number, page_number, type);
		printf("i = %d\n", i);
	}

	pthread_mutex_lock(&aio_mutexes[i]);
	// TODO not sure whether dup is neccessary
	page_file.aiocb_blocks[i].aio_fildes = dup(page_file.fdw);
	page_file.aiocb_blocks[i].aio_offset = page_number * PAGE_SIZE;
	page_file.aiocb_blocks[i].aio_nbytes = PAGE_SIZE;
	page_file.aiocb_blocks[i].aio_buf = get_page_address_in_buff(buff_page_number);
	page_file.aiocb_blocks[i].aio_sigevent.sigev_notify = SIGEV_NONE;

	print_info(buff_page_number, page_number, type);
	printf("aiocb structure is ready\n");

	// TODO error handling?
	int err;
	if(type == READ)
		err = aio_read(&page_file.aiocb_blocks[i]);
	else
		err = aio_write(&page_file.aiocb_blocks[i]);

	print_info(buff_page_number, page_number, type);
	printf("%s done\n", type == READ ? "READ" : "WRITE");

	if(err == -1)
	{
		fprintf(stderr, "aio_write failed\n");
		exit(1);
	}

	err = close(page_file.aiocb_blocks[i].aio_fildes);
	if(err == -1)
	{
		fprintf(stderr, "closing of file descriptor failed\n");
		exit(1);
	}

	aiocb_info[i].is_active = 0;
	pthread_mutex_unlock(&aio_mutexes[i]);
	// TODO Do we need to check whether aio_requsests is really locked?
	pthread_mutex_unlock(&aio_requests);
}


int main(int argc, char ** argv) 
{
	/* NUM_ASYNC_IO = 20, */
	/* PAGE_SIZE = 8, */
	/* PAGE_NUMBER = 32 */
	init_global();
	int i;
	for(i = 0; i < NUM_ASYNC_IO * PAGE_SIZE; i++)
		buffer[i] = '0' + (i % 10);
	
	for(i = 0; i < PAGE_NUMBER; i++)
		manage_page(i % NUM_ASYNC_IO, i, WRITE);
	/* start_write(0, 0); */
	/* copy_files(); */
	
	/* done_copy_files(); */
	/* done_source_file(); */
	done_global();

	return 0;
}
