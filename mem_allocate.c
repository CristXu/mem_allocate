// mem_allocate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "assert.h"

#define LEAST_MIN (1024)
typedef struct {
	void* base;
	uint32_t malloc_num, free_num;
}mem_info_t;
typedef enum {
	VALID = 0,
	INVALID,
}block_status;
typedef struct control_block{
	block_status valid;
	uint32_t size;
	struct control_block* next;
}control_block_t;

typedef enum {
	OK = 0,
	UNALIGN4 = -1,
	LIMIT_MIN,
}status_t;

status_t mem_init(mem_info_t*control, uint32_t base, uint32_t len) {
	if (base & 0x03) {
		return UNALIGN4;
	}
	if (len < LEAST_MIN) return LIMIT_MIN;
	control->base = (void*)base;
	control->malloc_num = 0;
	control->free_num = 0;

	control_block_t* frame = (control_block_t*)base;
	frame->valid = INVALID;
	frame->size = len - sizeof(control_block_t);
	frame->next = NULL;
	return OK;
	
}

void merge_block() {
}
void* tiny_malloc(mem_info_t *info, uint32_t size) {
	assert(info->base);
	// find available mem block
	control_block_t* start = (control_block_t*)info->base;
	while (start) {
		// try allocate
		if (start->valid == INVALID) {
			if ((start->size) >= size) {
				// split the area 
				int remain_size = start->size - size - 4 - sizeof(control_block_t);
				start->valid = VALID;
				start->size = size;
				if (remain_size > 0) {
					// make sure the alignment
					control_block_t* new_block = (control_block_t*)((int)((char*)start + sizeof(control_block_t) + start->size + 4) & (~0x4));
					new_block->size = remain_size;
					new_block->valid = INVALID;
					new_block->next = NULL;

					if (start->next) {
						control_block_t* tmp = start->next;
						start->next = new_block;
						new_block->next = tmp;
					}
					else {
						start->next = new_block;
					}
				}
				info->malloc_num += 1;
				return (void*)((uint32_t)start + sizeof(control_block_t));
			}
		}
		start = start->next;
	}
	return NULL;
}

void* tiny_realloc(mem_info_t* info, void* ptr, unsigned newsize) {

}
void* tiny_calloc(mem_info_t* info, uint32_t numElements, uint32_t sizeOfElement) {

}

void* tiny_free(mem_info_t* info, void* mem) {
	assert(info->base);
	// find available mem block
	control_block_t* del = (control_block_t*)((int)mem - sizeof(control_block_t));
	del->valid = INVALID;
	// merge 
	control_block_t* start = (control_block_t *)(info->base);
	while (start) {
		while (start->next){
			if ((start->valid == INVALID) && (start->next->valid == INVALID)) {
				int block_len = sizeof(control_block_t) + start->size + 4;
				if (((int)start + block_len) == (int)start->next) {
					start->size = start->size + 4 + start->next->size + sizeof(control_block_t);
					start->next = start->next->next;
				}
			}
			else {
				break;
			}
		}
		start = start->next;
	}
}

char mem_pool[4 * LEAST_MIN];
int main()
{

	mem_info_t control = {.base=NULL};
	if (OK != mem_init(&control, mem_pool, sizeof(mem_pool)))
		return -1;
	char* mem0 = (char*)tiny_malloc(&control, 120);
	char* mem1 = (char*)tiny_malloc(&control, 120);
	tiny_free(&control, mem0);
	tiny_free(&control, mem1);
	while (1) {
		char* mem = (char*)tiny_malloc(&control, 12);
		if ((!mem) || ((int)mem & 0x3)) {
			break;
		}
	}
    printf("Hello World!\n");
}

