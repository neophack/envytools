#ifndef DEMMT_REGION_H
#define DEMMT_REGION_H

#include <stdint.h>

struct region
{
	struct region *prev;
	uint32_t start;
	uint32_t end;
	struct region *next;
};

struct regions
{
	struct region *head;
	struct region *last;
};

void dump_regions(struct regions *regions);
int regions_are_sane(struct regions *regions);
int range_in_regions(struct regions *regions, uint32_t start, uint8_t len);
void free_regions(struct regions *regions);
void regions_add_range(struct regions *regions, uint32_t start, uint8_t len);

#endif
