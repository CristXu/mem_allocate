// mem_allocate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "assert.h"
#include "string.h"

#define USE_SINGLE_CHAIN (0)
#define LEAST_MIN (1024)
#define ALIGN (4)
#define MAGIC_BYTES (4)
#define MAGIC     (0x1a2b3c4d)
typedef struct {
	void* base;
	uint32_t total_size;
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

void new_memcpy(void* src, void* dst, uint32_t size) {
	// set 4 items together 
	uint8_t remain = size % 8;

	for (int i = 0;i < (size - remain); i += 8) {
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
		*((uint8_t*)dst)++ = *((uint8_t*)src)++;
	}
	switch (remain) {
		case 7:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 6:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 5:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 4:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 3:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 2:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 1:
			*((uint8_t*)dst)++ = *((uint8_t*)src)++;
			/* no break; */
		case 0:
			break;
		default:
			break;
	}
}

void new_memset(void* ptr, uint8_t value, uint32_t size) {
	// set 4 items together 
	uint8_t remain = size % 8;

	for (int i = 0;i < (size - remain); i += 8) {
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
		*((uint8_t*)ptr)++ = value;
	}
	switch (remain) {
		case 7:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 6:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 5:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 4:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 3:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 2:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 1:
			*((uint8_t*)ptr)++ = value;
			/* no break; */
		case 0:
			break;
		default:
			break;
	}
}
status_t mem_init(mem_info_t*control, uint32_t base, uint32_t len) {
	if (base & 0x03) {
		return UNALIGN4;
	}
	if (len < LEAST_MIN) return LIMIT_MIN;
	control->base = (void*)base;
	control->total_size = len;
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

#define CUR_BLOCK_LEN(ptr) (sizeof(control_block_t) + ptr->size + MAGIC_BYTES)
void* tiny_malloc(mem_info_t *info, uint32_t size) {
	assert(info->base);
	// find available mem block
	control_block_t* start = (control_block_t*)info->base;
	// align the size to 4B
	int aligned_size = (size + (ALIGN-1)) & (~(ALIGN-1));
	while (start) {
		// try allocate
		if (start->valid == INVALID) {
			if ((start->size) >= (aligned_size + MAGIC_BYTES)) { // we have the magic both at the head & end of the block
				// split the area 
				int remain_size_for_next_block = start->size - aligned_size -  MAGIC_BYTES; // need substract 4B magic at the end
				start->valid = VALID;
				start->size = aligned_size;
				//fill the magic at the end, but not the end of the aligned end
				int len_to_end = sizeof(control_block_t) + aligned_size;
				int* end_pos = (int*)((int)start + len_to_end);
				*end_pos = MAGIC;
				int remain_size = remain_size_for_next_block - sizeof(control_block_t); // each block at least has MAGIC + control_block
				if ((remain_size - MAGIC_BYTES) >= 0) { // make sure we have enough 
					// make sure the alignment
					control_block_t* new_block = (control_block_t*)((char*)start + CUR_BLOCK_LEN(start));
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

void free_all(mem_info_t* info) {
	control_block_t* block = (control_block_t*)info->base;
	block->magic = MAGIC;
	block->valid = INVALID;
	block->next = NULL;
	block->prev = NULL;
	block->size = info->total_size - sizeof(control_block_t);
}


void* tiny_realloc(mem_info_t* info, void* ptr, unsigned newsize) {
	// test if ptr->next is available and can merge 
	control_block_t* tmp = (control_block_t*)((int)ptr -sizeof(control_block_t));
	assert(newsize >= (tmp->size));
	int aligned_newsize = (newsize + ALIGN) & (~(ALIGN - 1));
	int block_len = CUR_BLOCK_LEN(tmp);
	if ((tmp->next) && (tmp->next->valid == INVALID) && (((int)tmp + block_len) == (int)tmp->next)) {
		int merged_size = CUR_BLOCK_LEN(tmp->next) + tmp->size;
		if (merged_size >= (aligned_newsize)) {
			int addition_size = aligned_newsize - tmp->size; //the addition_size will share by ptr->next
			tmp->size = aligned_newsize; // update the new size
			int next_new_size = tmp->next->size - addition_size;
			if ((next_new_size - MAGIC_BYTES) >= 0) {
				control_block_t* tmp1 = tmp->next->next;
				control_block_t* new_block = (control_block_t*)((uint32_t)tmp->next + addition_size);
				new_block->magic = MAGIC;
				new_block->size = next_new_size;
				new_block->valid = INVALID;
				new_block->next = NULL;
				tmp->next = new_block;
				new_block->next = tmp1;
#if (USE_SINGLE_CHAIN == 0)
				new_block->prev = tmp;
				if(tmp1)
					tmp1->prev = new_block;
#endif

			}
			// fill the magic at the end
			int len_to_end = sizeof(control_block_t) + tmp->size;
			int* end_pos = (int*)((int)tmp + len_to_end);
			*end_pos = MAGIC;
			return ptr;
		}
	}
	// 1. has next node, but can not merged(size is not sufficient) 2. without next node, need re-allocate
	tiny_free(info, ptr);
	void* new_ptr = tiny_malloc(info, newsize);
	memcpy(new_ptr, ptr, ((control_block_t*)ptr)->size);
	return new_ptr;
}

void* tiny_calloc(mem_info_t* info, uint32_t numElements, uint32_t sizeOfElement) {
	int size = numElements * sizeOfElement;
	void* ptr = tiny_malloc(info, size);
	if(ptr) new_memset(ptr, 0, size);
	return ptr;
}

char mem_pool[4 * LEAST_MIN];
int main()
{

	char p_array[120];
	new_memset(p_array, 0x65, sizeof(p_array));
	mem_info_t control = {.base=NULL};
	if (OK != mem_init(&control, mem_pool, sizeof(mem_pool)))
		return -1;
	char* mem0 = (char*)tiny_malloc(&control, 120);
	new_memcpy(p_array, mem0, sizeof(p_array));
	mem0 = (char*)tiny_realloc(&control, mem0, 130);
	char* mem1 = (char*)tiny_malloc(&control, 120);
	char* mem2 = (char*)tiny_malloc(&control, 120);
	char* mem3 = (char*)tiny_malloc(&control, 120);
	new_memcpy(p_array, mem3, sizeof(p_array));
	mem3 = (char*)tiny_realloc(&control, mem3, 145);
	tiny_free(&control, mem0);
	tiny_free(&control, mem2);
	tiny_free(&control, mem1);
	tiny_free(&control, mem3);
	char** p;
	p = (char**)tiny_malloc(&control, sizeof(char*) * 5);
	for (int i = 0;i < 5;i++) {
		p[i] = (char*)tiny_malloc(&control, sizeof(char) * 10);
		for (int j = 0;j < 10;j++) {
			p[i][j] = 'a'+j;
		}
	}
	p[0][9] = '\0';
	printf("%s", p[0]);
	for (int i = 0;i < 5;i++) 
		tiny_free(&control, p[i]);
	tiny_free(&control, p);


	while (1) {
		char* mem = (char*)tiny_malloc(&control, 5);
		if ((!mem) || ((int)mem & (ALIGN-1))) {
			break;
		}
	}
	free_all(&control);
    printf("Hello World!\n");
}

