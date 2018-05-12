/*
 * csim.c
 * Authors: Megan Bailey and Jake Wahl
 * 
 * Program takes in a .trace file. The file has L, M, S instructions, address,
 * and size. With this information, the program simulates the hits, misses,
 * and evictions of a cache. The program checks for inproper input and memory
 * leaks.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include "cachelab.h"

//Type def's to sooth carpal tunnel
typedef unsigned long int mem_addr;
typedef struct Line Line;
typedef struct Set Set;

//Struct to hold individual line of cache
struct Line {
	unsigned int valid;
	unsigned int tag;
	unsigned int lru;
};

//Struct to hold a set of lines
struct Set {
	Line *Lines;
};

// forward declaration
void simulateCache(char *trace_file, int num_sets, int block_size,
	   	int lines_per_set, int verbose);
void operationL (Set *cache, int lines_per_set, mem_addr address,
	   	int size, int verbose, int set, int tag, int *hit_count,
	   	int *miss_count, int *eviction_count); 
void operationS (Set *cache, int lines_per_set, mem_addr address,
	   	int size, int verbose, int set, int tag, int *hit_count,
	   	int *miss_count, int *eviction_count);
void operationM (Set *cache, int lines_per_set, mem_addr address,
	   	int size, int verbose, int set, int tag, int *hit_count,
	   	int *miss_count, int *eviction_count);
void hit(Set *cache, int lines_per_set, mem_addr address, int i,
	   	char operation, int size, int verbose, int set, int tag,
	   	int *hit_count, int *found);
void miss(Set *cache, int lines_per_set, mem_addr address, int i, 
		char operation, int size, int verbose, int set, int tag,
	   	int *hit_count, int *miss_count, int *found);
void eviction(Set *cache, int lines_per_set, mem_addr address, int i,
	   	char operation, int size, int verbose, int set, int tag, 
		int *hit_count, int *miss_count, int *eviction_count, int *found);
void updateLRU(Set *cache, int set_num, int prev_lru, int lines_per_set);


/**
 * Prints out a reminder of how to run the program.
 *
 * @param executable_name String containing the name of the executable.
 */
void usage(char *executable_name) {
	printf("Usage: %s [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n",
		   	executable_name);
}



/**
 * Main function of program. Opens tracefile and calls simulateCache.
 *
 * @param argc Number of arguments from the command line
 * @param argv Arguments from the command line
 */
int main(int argc, char *argv[]) {
	
	// Setting initial values
	int verbose_mode = 0;
	char *trace_filename = NULL;

	int c = -1;
	
	int num_sets, block_size, lines_per_set;
	int s_flag = 0, b_flag = 0, E_flag = 0, t_flag = 0;

	// Parsing command line arguments
	while ((c = getopt(argc, argv, "vhs:E:b:t:")) != -1) {
		switch (c) {
			case 'v':
				// enable verbose mode
				verbose_mode = 1;
				break;
			case 'h':
				// Print usage
				usage(argv[0]);
				exit(1);
			case 's':
				// Find number of sets
				num_sets = 1 << strtol(optarg, NULL, 10);
				s_flag = 1;
				break;
			case 'E':
				// Take in number of line per set
				lines_per_set = strtol(optarg, NULL, 10);
				E_flag = 1;
				break;
			case 'b':
				// Taking in bytes block size
				block_size = 1 << strtol(optarg, NULL, 10);
				b_flag = 1;
				break;
			case 't':
				// specify the trace filename
				trace_filename = optarg;
				t_flag = 1;
				break;	
			default:
				// default usage
				usage(argv[0]);
				exit(1);
		}
	}

	// Checking if all inputs accounted for
	if (!s_flag || !b_flag || !E_flag || !t_flag) {
		usage(argv[0]);
		exit(1);
	}

	// Verbose boiler plate
	if (verbose_mode) {
		printf("\n");
		printf("Verbose mode enabled.\n");
		printf("Trace filename: %s\n", trace_filename);
		printf("Number of sets: %d\n", num_sets);
		printf("\n");
	}

	// BEGIN SIMULATION!	
	simulateCache(trace_filename, num_sets, block_size,
		   	lines_per_set, verbose_mode);

    return 0;
}

/**
 * Simulates cache with the specified organization (S, E, B) on the given
 * trace file.
 *
 *filetype plugin on
 *au FileType php setl ofu=phpcomplete#CompletePHP
 *au FileType ruby,eruby setl ofu=rubycomplete#Complete
 *au FileType html,xhtml setl ofu=htmlcomplete#CompleteTags
 *au FileType c setl ofu=ccomplete#CompleteCpp
 *au FileType css setl ofu=csscomplete#CompleteCSS*
 * @param trace_file Name of the file with the memory addresses.
 * @param num_sets Number of sets in the simulator.
 * @param block_size Number of bytes in each cache block.
 * @param lines_per_set Number of lines in each cache set.
 * @param verbose Whether to print out extra information about what the
 *   simulator is doing (1 = yes, 0 = no).
 */
void simulateCache(char *trace_file, int num_sets, int block_size,
						int lines_per_set, int verbose) {
	// Variables to track how many hits, misses, and evictions we've had so
	// far during simulation.
	int hit_count = 0;
	int miss_count = 0;
	int eviction_count = 0;
	int set, tag, size = 0; 
	char operation[1];
	mem_addr address = 0;

	// Initializes Cache
	Set *cache = malloc(num_sets *  sizeof(Set));

	// Iniitializes Cache inards
	int i, j;
	for (i = 0; i < num_sets; i++){
		cache[i].Lines = malloc(lines_per_set *  sizeof(Line));
		for (j = 0; j < lines_per_set; j++){
			cache[i].Lines[j].valid = 0;
			cache[i].Lines[j].lru = j;
			cache[i].Lines[j].tag = 0;
		}
	}

	// Seeing if valid file and opening it
	FILE *fp = fopen(trace_file, "r");
	if (fp == NULL) {
		printf("Error opening file");
		exit(1);	
	}

	// Kickstart loop & parsing file
	int ret = fscanf(fp, "%s %lx,%d", operation, &address, &size);
	
	while (ret == 3 && ret != EOF) {

		// Isolating tag and set numbers
		tag = address >> ((int)log2(block_size) + (int)log2(num_sets));
		set = (address << (64 - ((int)log2(block_size) + (int)log2(num_sets))))
				>> (64 - (int)log2(num_sets));
		
		// Cases for each instruction
		if (!strncmp(operation, "L", 1)){
			operationL (cache, lines_per_set, address, size, verbose,
				   	set, tag, &hit_count, &miss_count, &eviction_count);	
		} 
		else if (!strncmp(operation, "S", 1)) {
			operationS (cache, lines_per_set, address, size, verbose,
				   	set, tag, &hit_count, &miss_count, &eviction_count);	
		}
		else if(!strncmp(operation, "M", 1)) {
			operationM (cache, lines_per_set, address, size, verbose,
				   	set, tag, &hit_count, &miss_count, &eviction_count);	
		} 
		else if(!strncmp(operation, "I", 1)) {
			;	
		} 
		else {
			printf("Error \n");
		}

		// Grabbing next line of input
		ret = fscanf(fp, "%s %lx,%d", operation, &address, &size); 
	}

	// Freeing cache
	for (i = 0; i < num_sets; i++){
		free(cache[i].Lines);
	}
	free(cache);

	// Printing stats
	printf("\n");
	printSummary(hit_count, miss_count, eviction_count);

	fclose(fp);
}



/**
 * Simulates the process of the L instruction in a cache. 
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address The memory location of the memory access
 * @param size Number of bytes in each cache block
 * @param verbose A flag which is set for verbose mode
 * @param set The set number of the memory access
 * @param tag The tag of the data memory access
 * @param hit_count A counter of cache hits
 * @param miss_count A counter of cache misses
 * @param eviction_count A counter of cache evictions
 */
void operationL (Set *cache, int lines_per_set, mem_addr address, int size,
	   	int verbose, int set, int tag, int *hit_count, int *miss_count,
	   	int *eviction_count) {
	int found = 0;
	int i;
	
	// Checking if hit
	for (i = 0; i < lines_per_set; i++) {
		if (cache[set].Lines[i].valid == 1) {
			if (cache[set].Lines[i].tag == tag){
				hit(cache, lines_per_set, address, i, 'L', size, verbose, set,
					   	tag, hit_count, &found);
				break;	
			}
		}
	}

	// Checking valid bits for miss
	if (!found) {
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].valid == 0) {
				miss(cache, lines_per_set, address, i, 'L', size, verbose, set,
					   	tag, hit_count, miss_count, &found);
				break;
			}
		}	
	}

	// Checking for full cache for eviction
	if (!found) {
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].lru == lines_per_set - 1) {
				eviction(cache, lines_per_set, address, i, 'L', size, verbose,
					   	set, tag, hit_count, miss_count, eviction_count,
					   	&found);
				break;
			}
		}
	}
}



/**
 * Simulates the process of the S instruction in a cache. 
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address Memory location of the memory access
 * @param size Number of bytes in each cache block
 * @param verbose A flag which is set if the user wants verbose mode
 * @param set The set number of the memory access
 * @param tag The tag of the memory access
 * @param hit_count A counter of cache hits
 * @param miss_count A counter of cache misses
 * @param eviction_count A counter of cache evictions
 */
void operationS (Set *cache, int lines_per_set, mem_addr address, int size,
	   	int verbose, int set, int tag, int *hit_count, int *miss_count,
	   	int *eviction_count) {
	int found = 0;
	int i;

	//Checking for hit
	for (i = 0; i < lines_per_set; i++) {
		if (cache[set].Lines[i].valid == 1) {
			if (cache[set].Lines[i].tag == tag) {
				hit(cache, lines_per_set, address, i, 'S', size, verbose, set,
					   	tag, hit_count, &found);	
				break;	
			}
		}
	}

	// Checking valid bits for miss
	if (!found) {
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].valid == 0) {
				miss(cache, lines_per_set, address, i, 'S', size, verbose, set,
					   	tag, hit_count, miss_count, &found);
				break;
			}
		}
				
	}
	
	// Checking for full cache for eviction 
	if (!found){
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].lru == lines_per_set - 1) {
				eviction(cache, lines_per_set, address, i, 'S', size, verbose,
					   	set, tag, hit_count, miss_count, eviction_count,
					   	&found);
				break;
			}
		}
	}
}



/**
 * Simulates the process of the M instruction in a cache. 
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address Memory location of the memory access
 * @param size Number of bytes in each cache block
 * @param verbose A flag which is set for verbose mode
 * @param set The set number of the memory address
 * @param tag The tag of the memory address
 * @param hit_count A counter of cache hits
 * @param miss_count A counter of cache misses
 * @param eviction_count A counter of cache evictions
 */
void operationM (Set *cache, int lines_per_set, mem_addr address, int size,
	   	int verbose, int set, int tag, int *hit_count, int *miss_count,
	   	int *eviction_count) {
	int found = 0;
	int i;
	
	// Checking for hit
	for (i = 0; i < lines_per_set; i++) {
		if (cache[set].Lines[i].valid == 1) {
			if (cache[set].Lines[i].tag == tag) {
				hit(cache, lines_per_set, address, i, 'M', size, verbose, set,
					   	tag, hit_count, &found);
				break;	
			}
		}
	}

	// Checking valid bits for miss
	if (!found) {
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].valid == 0) {
				miss(cache, lines_per_set, address, i, 'M', size, verbose, set,
					   	tag, hit_count, miss_count, &found);
				break;
			}
		}		
	}

	// Checking for full cache for eviction 
	if (!found) {
		for (i = 0; i < lines_per_set; i++) {
			if (cache[set].Lines[i].lru == lines_per_set - 1) {
				eviction(cache, lines_per_set, address, i, 'M', size, verbose,
					   	set, tag, hit_count, miss_count, eviction_count,
					   	&found);
				break;
					}
				}
			}	
}



/**
 * Simulation of a cache hit.
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address Memory location of the memory access
 * @param i Line number in the set
 * @param operation The performed operation
 * @param size Number of block bytes in each cache block 
 * @param verbose A flag which is set for verbose mode
 * @param set The set number of the memory address
 * @param tag The tag of the memory address
 * @param hit_count A counter of cache hits
 * @param found The flag which tracks if the address was found
 */
void hit(Set *cache, int lines_per_set, mem_addr address, int i,
	   	char operation, int size, int verbose, int set, int tag,
	   	int *hit_count, int *found){
	
	(*found) = 1;

	// Incrementing appropriate counters 
	if (operation == 'M') { 
		(*hit_count) += 2;
	} else {
		(*hit_count)++;
	}

	// Updating set LRU 
	updateLRU(cache, set, cache[set].Lines[i].lru, lines_per_set);

	// Printing for verbose mode
	if (verbose) {
		if (operation == 'L'){
			printf("L %lx,%d hit\n", address, size);
		} else if (operation == 'S') {
			printf("S %lx,%d hit\n", address, size);	
		} else {
			printf("M %lx,%d hit hit\n", address, size);
		}
	}
}



/**
 * Simulation of a cache miss.
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address Memory location of the memory access
 * @param i Line number in the set
 * @param operation The performed operation
 * @param size Number of block bytes in each cache block 
 * @param verbose A flag which is set for verbose mode
 * @param set The set number of the memory address
 * @param tag The tag of the memory address
 * @param hit_count A counter of cache hits
 * @param miss_count A counter of cache misses
 * @param found The flag which tracks if the address was found
 */
void miss(Set *cache, int lines_per_set, mem_addr address, 
		int i, char operation, int size, int verbose, int set, 
		int tag, int *hit_count, int *miss_count, int *found) {

	(*found) = 1;

	// Incrementing appropriate counters
	if (operation == 'M') { 
		(*miss_count)++;
		(*hit_count)++;
	} else {
		(*miss_count)++;
	}

	// Update line attributes
	cache[set].Lines[i].valid = 1;
	cache[set].Lines[i].tag = tag;

	// Updating set LRU 
	updateLRU(cache, set, cache[set].Lines[i].lru, lines_per_set);

	// Printing for verbose mode
	if (verbose) {
		if (operation == 'L'){
			printf("L %lx,%d miss\n", address, size);
		} else if (operation == 'S') {
			printf("S %lx,%d miss\n", address, size);	
		} else {
			printf("M %lx,%d miss hit\n", address, size);
		}
	}
}


/**
 * Simulation of a cache eviction.
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param lines_per_set Number of lines per cache set
 * @param address Memory location of the memory access
 * @param i Line number in the set
 * @param operation The performed operation
 * @param size Number of block bytes in each cache block 
 * @param verbose A flag which is set for verbose mode
 * @param set The set number of the memory address
 * @param tag The tag of the memory address
 * @param hit_count A counter of cache hits
 * @param miss_count A counter of cache misses
 * @param eviction_count A counter of cache evictions
 * @param found The flag which tracks if the address was found
 */
void eviction(Set *cache, int lines_per_set, mem_addr address, 
		int i, char operation, int size, int verbose, int set, 
		int tag, int *hit_count, int *miss_count, int *eviction_count, 
		int *found) {

	(*found) = 1;

	// Incrementing appropriate counters
	if (operation == 'M') { 
		(*miss_count)++;
		(*hit_count)++;
		(*eviction_count)++;
	} else {
		(*miss_count)++;
		(*eviction_count)++;
	}
	
	// Updating line attributes
	if (operation == 'L'){
		cache[set].Lines[i].valid = 1;
		cache[set].Lines[i].tag = tag;	
	} else {
		cache[set].Lines[i].tag = tag;
	}

	// Updating set LRU
	updateLRU(cache, set, cache[set].Lines[i].lru, lines_per_set);

	// Printing for verbose mode
	if (verbose) {
		if (operation == 'L'){
			printf("L %lx,%d miss eviction\n", address, size);
		} else if (operation == 'S') {
			printf("S %lx,%d miss eviction\n", address, size);	
		} else {
			printf("M %lx,%d miss eviction hit\n", address, size);
		}
	}
}



/**
 * Updates Least Recently Used bit in a set after a memory access.
 *
 *
 * @param cache An array of type Set that simulates a cache
 * @param set_num The set number in the cache that needs to be updated
 * @param prev_lru The previous LRU at line that was recently accessed
 * @param lines_per_set Number of lines per cache set
 */
void updateLRU(Set *cache, int set_num,int prev_lru, int lines_per_set) { 
	int i;

	// Update valid lines LRU
	for (i = 0; i < lines_per_set; i++) {
		if (cache[set_num].Lines[i].valid == 1) {

			// Only updating LRU lower than modified lines LRU
			if (cache[set_num].Lines[i].lru <= prev_lru) {
				if(cache[set_num].Lines[i].lru == prev_lru) {

					// Setting modified lines LRU to 0 
					cache[set_num].Lines[i].lru = 0;
				} else {

					// Incrementing nonmodified lines
					cache[set_num].Lines[i].lru++;
				}
			}
		}
	}
}
