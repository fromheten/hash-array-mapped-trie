# `hamt-poly` - polymorphic hash array mapped trie

`hamt-poly` implements polymorphic and immutable Hash Array Mapped Tries in C.
The use case it was built for was to handle storing api routes as the key with a function pointer/struct as the value, but it is extremely versatile.

`hamt-poly` is a fork of [`hamt`](https://github.com/Jamesbarford/hash-array-mapped-trie).

`hamt-poly` is a header-only library - just drop `hamt.h` into your project, and you're good.

Most code is moved into a preprocessor macro, in order to achieve polymorphism, i.e. to be able to have keys of arbitrary types, rather than a hard-coded type such as `char*`. Thank you James for publishing your code, it was a real delight to find!

## Testing

Tests are defined in `hamt-testing.c`. To build and run the tests, run:

```sh
$ mkdir build
$ make
$ ./hamt-testing.out
```

## Usage

### Initialisation
In this key-value data store, keys can be of arbitrary types. Thus we must also provide a way to create a hash of the key type (the H in HAMT), as well as how to check if two keys are equal. From `hamt-testing.c` comes this example:

```c
#include "hamt.h"

typedef struct MyKeyType {
  char *str;
} MyKeyType;
/**
 * It must be able to check equality, for example when inserting
 * with a hash collission
 */
bool mykeytype_equals(MyKeyType *s0, MyKeyType *s1) {
  return strcmp(s0->str, s1->str) == 0;
}
unsigned int get_hash_of_mykeytype(MyKeyType *s) { return get_hash(s->str); }
/**
 * HAMT_DEFINE will define types & functions prefixed with name_hamt_,
 * where name in this example is MyKeyType.
 */
HAMT_DEFINE(MyKeyType, get_hash_of_mykeytype, mykeytype_equals)
```

### Insert, Retrieve, and Remove


Any arbitrary data `void *` can be stored against a `MyKeyType *` key.
```c
int main(void) {
  MyKeyType *keyptr = malloc(sizeof(MyKeyType));
  keyptr->str = "the key";
  MyKeyType_hamt *hamt = MyKeyType_hamt_new();
  /**
   * Here we insert a char* value in the 2nd argument. Note that it could have
   * been any other type, as it is cast to void* internally  */
  hamt = MyKeyType_hamt_set(hamt, keyptr, "polymorphic...");
  char *value = (char *)MyKeyType_hamt_get(hamt, keyptr);
  printf("Polymorphism: MyKeyType value = %s\n", value);
  assert(strcmp(value, "polymorphic...") == 0);
  hamt = MyKeyType_hamt_remove(hamt, keyptr);
  value = MyKeyType_hamt_get(hamt, keyptr);
  assert(value == NULL);
}
```
