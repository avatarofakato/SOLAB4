#include "pagesim.h"
#include <stdio.h>

void callback(int op, int arg1, int arg2)
{
	printf("callback(%d, %d, %d) called\n", op, arg1, arg2);
}

int main()
{
	printf("simple_test is starting\n");

	if(page_sim_init(0, 1, 2, 3, 4, callback) != -1)
		printf("succesfull page_sim_init call\n");
	else
		printf("unsuccesfull page_sim_init call\n");

	if(page_sim_init(0, 1, 2, 3, 4, callback) != -1)
		printf("succesfull page_sim_init call\n");
	else
		printf("unsuccesfull page_sim_init call\n");

	page_sim_end();
	return 0;
}
