#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "hamt.h"

void* put_value_heap(Value v) {
  void* ptr = malloc(sizeof(Value));
  memcpy(ptr, (void*)&v, sizeof(Value));
  return ptr;
}
/* I want to add the functionality of having HAMT keys of any value,
	 not only char* */
void value_test() {
  struct hamt_t* hamt = create_hamt();
  Value value;
  value.actual_value.u8 = 13;
  value.actual_value.string = "HEllo";

  Value key_value;
  key_value.type = U8;
  key_value.actual_value.u8 = 69;
  hamt = hamt_set(hamt, put_value_heap(key_value), put_value_heap(value));

  Value* gotten_value = hamt_get(hamt, put_value_heap(key_value));
  printf("%s\n\n", gotten_value->actual_value.string);
  printf("%i\n\n", gotten_value->actual_value.u8);
}

Value* mkkey(char* cool_string, int cool_int) {
  Value* v;
  v = malloc(sizeof(Value));
  v->actual_value.u8 = cool_int;
  v->actual_value.string = cool_string;
  return v;
}
void martins_test() {
  struct hamt_t* hamt1 = create_hamt();
  struct hamt_t* hamt2 = create_hamt();
  hamt1 = hamt_set(hamt1,
									 mkkey("hello",
												 1337),
									 "world");
  char *value11 = (char *)hamt_get(hamt1,
																	 mkkey("hello",
																				 1337));
  char *value12 = (char *)hamt_get(hamt1,
																	 mkkey("good night",
																				 12));
  hamt2 = hamt_set(hamt1,
									 mkkey("good night",
												 6969),
									 "friend");
  char *value21 = (char *)hamt_get(hamt2,
																	 mkkey("hello",
																				 11));
  char *value22 =  (char *)hamt_get(hamt2,
																		mkkey("good night",
																					9));
  printf("value11: %s,\n value12: %s,\n value21: %s,\n value 22: %s\n",
				 value11,
				 value12,
				 value21,
				 value22);
}

void test_case_1() {
  struct hamt_t *hamt = create_hamt();

  hamt = hamt_set(hamt, mkkey("hello", 4), "world");
  hamt = hamt_set(hamt, mkkey("hey", 11), "over there");
  hamt = hamt_set(hamt, mkkey("hey2", 11), "over there again");
  char *value1 = (char *)hamt_get(hamt, mkkey("hello", 11));
  char *value2 = (char *)hamt_get(hamt, mkkey("hey", 11));
  char *value3 = (char *)hamt_get(hamt, mkkey("hey2", 11));
  printf("value1: %s\n", value1);
  printf("value2: %s\n", value2);
  printf("value3: %s\n", value3);

  hamt = hamt_set(hamt, mkkey("Aa", 11), "collision 1");
  hamt = hamt_set(hamt, mkkey("BB", 11), "collision 2");

  char *collision_1 = (char *)hamt_get(hamt, mkkey("Aa", 11));
  char *collision_2 = (char *)hamt_get(hamt, mkkey("BB", 11));
  printf("collision value1: %s\n", collision_1);
  printf("collision value2: %s\n", collision_2);
}

void insert_dictionary(struct hamt_t **hamt, char *dictionary) {
  char *ptr = dictionary;

  while (*dictionary != '\0') {
    if (*dictionary == '\n') {
      *dictionary = '\0';
      char *str = strdup(ptr);
      *hamt = hamt_set(*hamt, mkkey(str, 11), str);
      ptr = dictionary + 1;
      *dictionary = '\n';
    }
    dictionary++;
  }
}

void dictionary_check(struct hamt_t *hamt, char *dictionary) {
  char *ptr = dictionary;
  char *value;
  int missing_count = 0;
  int accounted_for = 0;

  printf("Checking HAMT entries..\n");
  while (*dictionary != '\0') {
    if (*dictionary == '\n') {
      *dictionary = '\0';

      value = (char *)hamt_get(hamt, mkkey(ptr, 11));
      if (value == NULL) {
        missing_count++;
      } else if(strcmp(value, ptr) != 0) {
        printf("Mismatch\n");
      } else {
        accounted_for++;
      }
      ptr = dictionary + 1;
    }
    dictionary++;
  }

  printf("Missing: %d\n", missing_count);
  printf("Present: %d\n", accounted_for);
}

void remove_all(struct hamt_t *hamt, char *dictionary) {
  char *ptr = dictionary;
  int missing_count = 0;
  int removal_count = 0;

  while (*dictionary != '\0') {
    if (*dictionary == '\n') {
      *dictionary = '\0';

      hamt = hamt_remove(hamt, mkkey(ptr, 11));
      if (hamt == NULL) {
        missing_count++;
        goto failed;
      } else {
        removal_count++;
      }
      ptr = dictionary + 1;
    }
    dictionary++;
  }

  printf("Missing: %d\n", missing_count);
  printf("Removed: %d\n", removal_count);
  return;
 failed:
  printf("Failed Missing: %d\n", missing_count);
  printf("Failed Removed: %d\n", removal_count);
}


void test_case_2(char *contents) {
  struct hamt_t *hamt = create_hamt();

  insert_dictionary(&hamt, strdup(contents));
  dictionary_check(hamt, strdup(contents));
  printf("finished insert\n");
  remove_all(hamt, strdup(contents));
  printf("Finished removing\n");
  dictionary_check(hamt, strdup(contents));
}

int main(void) {
  /* int fd; */
  /* struct stat sb; */

  /* if ((fd = open("./testing/dictionary.txt", O_RDONLY)) == -1) { */
  /*   fprintf(stderr, "Failed to load file: %s\n", strerror(errno)); */
  /*   exit(EXIT_FAILURE); */
  /* } */

  /* if ((fstat(fd, &sb) == -1)) { */
  /*   fprintf(stderr, "Failed to fstat file: %s\n", strerror(errno)); */
  /*   goto failed; */
  /* } */

  /* char *contents = mmap(NULL, sb.st_size, PROT_WRITE, MAP_PRIVATE, fd, 0); */
  /* if (contents == MAP_FAILED) { */
  /*   fprintf(stderr, "Failed to mmap file: %s\n", strerror(errno)); */
  /*   goto failed; */
  /* } */

  value_test();
  martins_test();
               /* test_case_1(); */
  /*  test_case_2(contents); */


  /*  munmap(contents, sb.st_size); */
  /*  close(fd); */

  /* failed: */
  /*  (void)close(fd); */
  /*  exit(EXIT_FAILURE); */
}
