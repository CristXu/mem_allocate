// mem_allocate.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "assert.h"
#include "string.h"

#define LEAST_MIN (1024)
#define ALIGN (4)
#define MAGIC_BYTES (4)
#define MAGIC     (0x1a2b3c4d)

typedef struct control_block{
	int magic;
	uint32_t size;
	struct control_block* next;
	struct control_block* prev;
}control_block_t;

typedef struct {
	void* base;
	uint32_t total_size;
	uint32_t malloc_num, free_num;
	control_block_t* used_list;
}mem_info_t;

typedef enum {
	UNALIGN4 = -1,
	LIMIT_MIN,
	OK,
}status_t;

#define COPY_REMAIN(remain, src, dst, invert) \
	switch (remain) { \
			case 7: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 6: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 5: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 4: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 3: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 2: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 1: \
				if(invert) \
					*((uint8_t*)dst)-- = *((uint8_t*)src)--; \
				else  \
					*((uint8_t*)dst)++ = *((uint8_t*)src)++; \
				/* no break; */ \
			case 0: \
				break; \
			default: \
				break; \
	}

typedef void (*copy_ptr)(void* src, void* dst, uint32_t size, int invert);
// mabye we can use __typeof__ if we use the armclang
/*
#defie COPY(src, dst, size, type) \
	__typeof__(type) *s = src, *d = dst; \
	for(int i = 0; i<size; i+=(sizeof(type))){ \
		*d++ = *s++    \
	}
*/
void copy_by_4B(void* src, void* dst, uint32_t size, int invert) {
	for (int i = 0;i < (size); i += 4) {
		if(invert)
			*((uint32_t*)dst)-- = *((uint32_t*)src)--;
		else
			*((uint32_t*)dst)++ = *((uint32_t*)src)++;
	}
}
void copy_by_8B(void* src, void* dst, uint32_t size, int invert) {
	for (int i = 0;i < (size); i += 8) {
		if (invert)
			*((uint32_t*)dst)-- = *((uint32_t*)src)--;
		else
			*((uint32_t*)dst)++ = *((uint32_t*)src)++;
	}
}
void new_memcpy(void* src, void* dst, uint32_t size) {
	// make sure from a 4B align addr
	copy_ptr copy;
	uint8_t remain;
	int invert;
	if (((int)src & 0x4) || ((int)dst & 0x4)) {
		copy = copy_by_4B;
		remain = size % 4;
	}
	else {
		copy = copy_by_8B;
		remain = size % 8;
	}
	invert = (int)dst > (int)src ? 1 : 0;
	if (invert) {
		src = (void*)((int)src + size);
		dst = (void*)((int)dst + size);
	}
	copy(src, dst, size - remain, invert);
	COPY_REMAIN(remain, src, dst, invert);
}

#define B8TOB32(v)  (v<<24 | v<<16 | v<<8 | v)
/* maybe use a macro when use armclang
#define assign(p, value, size) \
	for (int i = 0;i < (size - remain); i += sizeof(__typeof__(value))) {  \
			*((__typeof__(value)*)ptr)++ = value;  \
	} 
*/
void new_memset(void* ptr, uint8_t value, uint32_t size) {
	uint8_t remain;
	uint64_t tmp = (((uint64_t)B8TOB32(value))<<32) | B8TOB32(value);
	if (((int)ptr & 0x03)) {
		remain = size % 4;
		for (int i = 0;i < (size - remain); i += 4) {
			*((uint32_t*)ptr)++ = (uint32_t)tmp;
		}
	}
	else {
		remain = size % 8;
		for (int i = 0;i < (size - remain); i += 8) {
			*((uint64_t*)ptr)++ = (uint64_t)tmp;
		}
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

#define BLOCK_STRUCT_LEN  (sizeof(control_block_t))
// the smallest block including: block + MAGIC  
#define BLOCK_LEN(size)  (size + BLOCK_STRUCT_LEN + MAGIC_BYTES) 
#define CUR_BLOCK_LEN(ptr) BLOCK_LEN(ptr->size)
#define BLOCK_INTERVAL(cur, next) ((next - cur) * BLOCK_STRUCT_LEN - CUR_BLOCK_LEN(cur))
#define CAST_CONTROL_BLOCK_PTR(val)  ((control_block_t*)(val))
#define CALC_CONTROL_BLOCK_PTR(start, offset)  CAST_CONTROL_BLOCK_PTR((int)(start) + offset)
#define CAST_CONTROL_BLOCKPTR_VOID(ptr, offset)  (void*)((int)ptr + offset) 
#define GET_CONTROL_BLOCK_PTR_FROM_ADDR(mem) CAST_CONTROL_BLOCK_PTR((int)mem - BLOCK_STRUCT_LEN)
status_t mem_init(mem_info_t*control, uint32_t base, uint32_t len) {
	if (base & 0x03) {
		return UNALIGN4;
	}
	if (len < LEAST_MIN) return LIMIT_MIN;
	control->base = (void*)base;
	control->total_size = len;
	control->malloc_num = 0;
	control->free_num = 0;

	control->used_list = NULL;
	return OK;	
}

void* tiny_malloc(mem_info_t *info, uint32_t size) {
	assert(info->base);
	// find available mem block
	control_block_t* start = info->used_list;
	// align the size to 4B
	int aligned_size = (size + (ALIGN - 1)) & (~(ALIGN - 1));
	// without valid list now, update and return directly
	if (!start) {
		if (BLOCK_LEN(size) > info->total_size) return NULL;
		start = (control_block_t*)((int)info->base);
		start->magic = MAGIC;
		start->next = CALC_CONTROL_BLOCK_PTR(info->base, info->total_size);
		start->prev = NULL;
		start->size = aligned_size;
		int* end_magic = (int*)CALC_CONTROL_BLOCK_PTR(start, CUR_BLOCK_LEN(start) - MAGIC_BYTES);
		*end_magic = MAGIC;
		info->used_list = start;
		info->malloc_num += 1;
		return CAST_CONTROL_BLOCKPTR_VOID(start, BLOCK_STRUCT_LEN);
	}
	while (start != (CALC_CONTROL_BLOCK_PTR(info->base, info->total_size))) {
		int interval = BLOCK_INTERVAL(start, start->next);
		// test the first node first
		if (start->size == 0) {
			if (interval >= (aligned_size + MAGIC_BYTES)) {
				start->size = aligned_size;
				int* end_magic = (int*)CALC_CONTROL_BLOCK_PTR(start, CUR_BLOCK_LEN(start) - MAGIC_BYTES);
				*end_magic = MAGIC;
				info->malloc_num += 1;
				return CAST_CONTROL_BLOCKPTR_VOID(start, BLOCK_STRUCT_LEN);
			}
		}
		if (interval >= (int)BLOCK_LEN(aligned_size)) {
			// split the region, and update the next node 
			control_block_t* next = CALC_CONTROL_BLOCK_PTR(start, CUR_BLOCK_LEN(start));
			next->magic = MAGIC;
			next->size = aligned_size;
			if (start->next) {
				control_block_t* tmp = start->next;
				next->next = tmp;
				start->next = next;
				next->prev = start;
			}
			int* end_magic = (int*)CALC_CONTROL_BLOCK_PTR(next, CUR_BLOCK_LEN(next) - MAGIC_BYTES);
			*end_magic = MAGIC;
			info->malloc_num += 1;
			return CAST_CONTROL_BLOCKPTR_VOID(next, BLOCK_STRUCT_LEN);
		}
		start = start->next;
	}
	return NULL;
}

void* tiny_free(mem_info_t* info, void* mem) {
	assert(info->base);
	// find available mem block
	control_block_t* del = GET_CONTROL_BLOCK_PTR_FROM_ADDR(mem);
	info->free_num += 1;
	// check if it is the first used_list node 
	if (del == info->used_list) {
		// only set the size to 0, and don't delete
		del->size = 0;
		return;
	}
	if (del->next) {
		control_block_t* tmp = del->prev;
		tmp->next = del->next;
		del->next->prev = tmp;
		return;
	}	
}

void free_all(mem_info_t* info) {
	info->used_list = NULL;
}


void* tiny_realloc(mem_info_t* info, void* ptr, unsigned newsize) {
	// test if ptr->next is available and can merge 
	control_block_t* tmp = GET_CONTROL_BLOCK_PTR_FROM_ADDR(ptr);
	assert(newsize >= (tmp->size));
	int aligned_newsize = (newsize + ALIGN - 1) & (~(ALIGN - 1));
	// if can expand the size 
	int interval = BLOCK_INTERVAL(tmp, tmp->next);
	if (interval >= (int)BLOCK_LEN(aligned_newsize)) {
		tmp->size = aligned_newsize;
		int* end_magic = (int*)CALC_CONTROL_BLOCK_PTR(tmp, CUR_BLOCK_LEN(tmp) - MAGIC_BYTES);
		*end_magic = MAGIC;
		return CAST_CONTROL_BLOCKPTR_VOID(tmp, BLOCK_STRUCT_LEN);
	}
	// check if we can free the cur node first, then malloc
	int has_free = 0;
	int old_size = GET_CONTROL_BLOCK_PTR_FROM_ADDR(ptr)->size;
	if (tmp->prev) { // not the first node, be sure has the prev node
		int interval = BLOCK_INTERVAL(tmp->prev, tmp->next);
		if (interval >= (MAGIC_BYTES + aligned_newsize)) {
			tiny_free(info, ptr);
			has_free = 1;
		}
	}
	void* new_ptr = tiny_malloc(info, newsize);
	if (new_ptr) {
		new_memcpy(new_ptr, ptr, old_size);
		if (!has_free) tiny_free(info, ptr);
	}
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

	char* t0 = (char*)tiny_malloc(&control, 10);
	char* t = (char*)tiny_malloc(&control, 4000);
	tiny_free(&control, t0);
	t = (char*)tiny_realloc(&control, t, 4056);
	tiny_free(&control, t);

	while (1) {
		char* mem = (char*)tiny_malloc(&control, 5);
		if ((!mem) || ((int)mem & (ALIGN-1))) {
			break;
		}
	}
	free_all(&control);
    printf("Hello World!\n");
}

