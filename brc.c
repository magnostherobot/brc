// vim: ts=4 sw=4 noet

#include <stdio.h> // printf
#include <fcntl.h> // open
#include <sys/mman.h> // mmap
#include <unistd.h> // lseek
#include <stdint.h> // int32_t
#include <stdlib.h> // strtod
#include <string.h> // strncmp, memset
#include <stdbool.h>

#define min(a, b) (a) < (b) ? (a) : (b)
#define max(a, b) (a) > (b) ? (a) : (b)

#define CHUNK_SIZE 8
typedef char chunk __attribute__ ((vector_size (CHUNK_SIZE)));

typedef struct {
	char *start;
	size_t size;
} string;

#define MAP_SIZE 10000
typedef struct {
	string name;
	double min;
	double max;
	double total;
	unsigned int count;
} map_entry;

unsigned long djb2(string name) {
	unsigned long hash = 5381;

	for (int i = 0; i < name.size; i++) {
		hash = hash * 33 + name.start[i];
	}

	return hash;
}

char *find_semicolon(char *hay, size_t n) {

	// Name is at least 1 character long,
	// so we don't need to check the first character.
	for (int i = 1; i < n; i++) {
		if (hay[i] == ';') {
			return hay + i;
		}
	}

	return NULL;
}

int string_compare(const string *a, const string *b) {
	int result = strncmp(a->start, b->start, min(a->size, b->size));
	return result;
}

void add_entry(map_entry *map, size_t map_size, string name, double value) {
	unsigned long hash = djb2(name);

	for (int i = 0; i < MAP_SIZE; i++) {
		map_entry *entry = map + ((hash + i) % MAP_SIZE);

		if (entry->name.start == NULL) {
			// new entry
			entry->name = name;
			entry->min = value;
			entry->max = value;
			entry->total = value;
			entry->count = 1;
			return;

		} else if (!string_compare(&name, &entry->name)) {
			// existing entry
			entry->min = min(entry->min, value);
			entry->max = max(entry->max, value);
			entry->total += value;
			entry->count++;
			return;

		} else {
			// wrong entry; move along
		}
	}
}

void print_map_entries(map_entry *map, size_t map_size) {
	for (int i = 0; i < map_size; i++) {

		// qsort puts all the empty map entries at the end of the map,
		// so we can early-return when we find the first one.
		if (!map[i].count) return;

		printf("%.*s;%.1lf;%.1lf;%.1lf\n",
				(int)map[i].name.size, map[i].name.start,
				map[i].min,
				map[i].total / (float)map[i].count,
				map[i].max);
	}
}

double read_double(char *start, char **end) {
	double sign = (double)1;
	double mag = (double)0;

	char *p = start;

	while (true) {
		switch (*p) {

			case '\n':
			case '\0':
				*end = p;
				return sign * mag / (double)10;

			case '-':
				sign = (double)-1;
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				mag *= 10;
				mag += (*p - '0');
				break;

			case '.':
				break;

			default:
				printf("unexpected character '%c'\n", *p);
				break;
		}

		p++;
	}
}

int compare_map_entries(const void *ap, const void *bp) {
	const map_entry *a = ap;
	const map_entry *b = bp;

	if (!a->count) return 1;
	if (!b->count) return -1;

	return string_compare(&a->name, &b->name);
}

int main(int argc, char **argv) {
	int fd = open(argv[1], 0, O_RDONLY);

	off_t file_size = lseek(fd, 0, SEEK_END);

	// By allocating an extra byte at the end of the file,
	// we can use a null-check to terminate the for-loop below.
	char *mem = mmap(NULL, file_size + 1, PROT_READ, MAP_SHARED, fd, 0);

	map_entry *map = malloc(MAP_SIZE * sizeof (*map));

	for (char *p = mem; *p;) {
		string name;
		name.start = p;
		p = find_semicolon(p, file_size - (p - mem));
		name.size = p - name.start;
		p++;

		string number;
		number.start = p;
		double value = read_double(p, &p);
		number.size = p - number.start;
		p++;

		add_entry(map, MAP_SIZE, name, value);
	}

	qsort(map, MAP_SIZE, sizeof (*map), compare_map_entries);
	print_map_entries(map, MAP_SIZE);

	return 0;
}
