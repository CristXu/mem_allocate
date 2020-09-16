// mem_allocate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "assert.h"

#define USE_SINGLE_CHAIN (0)
#define LEAST_MIN (1024)
#define ALIGN (4)
#define MAGIC_BYTES (4)
#define MAGIC     (0x1a2b3c4d)
typedef struct {
	void* base;
	uint32_t malloc_num, free_num;
}mem_info_t;
typedef enum {
	VALID = 0,
	INVALID,
}block_status;
typedef struct control_block{
	int magic;
	block_status valid;
	uint32_t size;
	struct control_block* next;
	struct control_block* prev;
}control_block_t;

typedef enum {
	UNALIGN4 = -1,
	LIMIT_MIN,
	OK,
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
	frame->magic = MAGIC;
	frame->valid = INVALID;
	frame->size = len - sizeof(control_block_t);
	frame->next = NULL;
	frame->prev = NULL;
	return OK;
	
}
void* tiny_malloc(mem_info_t *info, uint32_t size) {
	assert(info->base);
	// find available mem block
	control_block_t* start = (control_block_t*)info->base;
	// align the size to 4B
	size = (size + ALIGN) & (~(ALIGN-1));
	while (start) {
		// try allocate
		if (start->valid == INVALID) {
			if ((start->size) >= size) {
				// split the area 
				int remain_size = start->size - size - sizeof(control_block_t) - MAGIC_BYTES; // need substract 4B aligned, 4B magic at the end
				start->valid = VALID;
				start->size = size;
				//fill the magic at the end 
				int len_to_end = sizeof(control_block_t) + start->size;
				int* end_pos = (int*)((int)start + len_to_end);
				*end_pos = MAGIC;
				if (remain_size > 0) {
					// make sure the alignment
					control_block_t* new_block = (control_block_t*)((char*)start + len_to_end + MAGIC_BYTES);
					new_block->magic = MAGIC;
					new_block->size = remain_size;
					new_block->valid = INVALID;
					new_block->next = NULL;

					if (start->next) {
						control_block_t* tmp = start->next;
						start->next = new_block;
						new_block->next = tmp;
#if (USE_SINGLE_CHAIN == 0)
						new_block->prev = start;
						tmp->prev = new_block;
#endif
					}
					else {
						start->next = new_block;
#if (USE_SINGLE_CHAIN == 0)
						new_block->prev = start;
#endif
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

#define CUR_BLOCK_LEN(ptr) (sizeof(control_block_t) + ptr->size + MAGIC_BYTES)
void* tiny_free(mem_info_t* info, void* mem) {
	assert(info->base);
	// find available mem block
	control_block_t* del = (control_block_t*)((int)mem - sizeof(control_block_t));
	del->valid = INVALID;
	info->free_num += 1;
#if (USE_SINGLE_CHAIN == 0)
	// merge the next first, in case delete the node
	if ((del->next) && (del->next->valid == INVALID)) {
		// delete the next node, merge it into this
		int block_len = CUR_BLOCK_LEN(del);
		if (((int)del + block_len) == (int)del->next) {
			del->size = del->size + CUR_BLOCK_LEN(del->next);
			control_block_t* tmp = del->next;
			del->next = tmp->next;
			if (tmp->next) {
				tmp->next->prev = del;
			}
		}
	}
	if ((del->prev) && (del->prev->valid == INVALID)) {
		int block_len = CUR_BLOCK_LEN(del->prev);
		if (((int)del->prev + block_len) == (int)del) {
			// delete this node, merge it into the prev
			control_block_t* tmp = del->prev;
			tmp->size = tmp->size + CUR_BLOCK_LEN(del);
			tmp->next = NULL;
			if (del->next) {
				tmp->next = del->next->next;
				del->next->prev = tmp;
			}
		}
	}
#else
	// merge 
	control_block_t* start = (control_block_t *)(info->base);
	while (start) {
		while (start->next){
			if ((start->valid == INVALID) && (start->next->valid == INVALID)) {
				int block_len = CUR_BLOCK_LEN(start);
				if (((int)start + block_len) == (int)start->next) {
					start->size = start->size + CUR_BLOCK_LEN(start->next);
					start->next = start->next->next;
				}
			}
			else {
				break;
			}
		}
		start = start->next;
	}
#endif
}

char mem_pool[4 * LEAST_MIN];
int main()
{

	mem_info_t control = {.base=NULL};
	if (OK != mem_init(&control, mem_pool, sizeof(mem_pool)))
		return -1;
	char* mem0 = (char*)tiny_malloc(&control, 120);
	char* mem1 = (char*)tiny_malloc(&control, 120);
	char* mem2 = (char*)tiny_malloc(&control, 120);
	char* mem3 = (char*)tiny_malloc(&control, 120);
	tiny_free(&control, mem0);
	tiny_free(&control, mem2);
	tiny_free(&control, mem1);
	tiny_free(&control, mem3);
	while (1) {
		char* mem = (char*)tiny_malloc(&control, 5);
		if ((!mem) || ((int)mem & 0x3)) {
			break;
		}
	}
    printf("Hello World!\n");
}

