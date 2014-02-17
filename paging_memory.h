#ifndef _PAGING_MEMORY_
#define _PAGING_MEMORY_
// TODO get/set should be here not in pagesim.h

enum TYPE_OF_OPERATION
{
	READ,
	WRITE
};

// TODO all three function should finally return errno or 0 to handle possible errors
void init_global(int _NUM_ASYNC_IO, int _PAGE_SIZE, int _PAGE_NUMBER);

void done_global();

void manage_page(int buff_page_number, int page_number, enum TYPE_OF_OPERATION type);

int get_buff(unsigned a, uint8_t *v);
int set_buff(unsigned a, uint8_t v);

#endif
