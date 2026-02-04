#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STPCPY
char *stpcpy(char *restrict dst, const char *restrict src);
#else
char *stpcpy(char *restrict dst, const char *restrict src) {
  while (*src != '\0') {
    *dst++ = *src++;
  }
  *dst = *src;
  return dst;
}
#endif

#ifdef HAVE_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
#else
int strcasecmp(const char *s1, const char *s2) {
  while(*s1 != '\0' && *s2 != '\0' && (tolower(*s1) == tolower(*s2)))
  {
    ++s1;
    ++s2;
  }
  return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// More efficient
uint8_t crc_iter_sub48 (uint8_t *message, size_t message_size, uint64_t poly, uint8_t poly_bits) {
  assert(poly_bits <= 48);
  uint64_t rem = 0;
  poly <<= 8;
  for (size_t i = 0; i < message_size; ++i) {
    rem |= message[i];
    for (uint8_t j = 0; j < 8; ++j) {
      uint64_t masked = -(rem >> (poly_bits - 1 + 8)) & poly;
      rem <<= 1;
      rem ^= masked;
    }
  }
  for (uint8_t j = 0; j < 8; ++j) {
    uint64_t masked = -(rem >> (poly_bits - 1 + 8)) & poly;
    rem <<= 1;
    rem ^= masked;
  }
  return rem >> 8;
}

// Poly is either truncated 64 bit polynomial or full 2-63 bit.
uint64_t crc_iter(uint8_t *message, size_t message_size, uint64_t poly, uint8_t poly_bits) {
  if (poly_bits <= 48)
    return crc_iter_sub48(message, message_size, poly, poly_bits);
  assert(poly_bits > 1);
  uint64_t rem = 0;
  size_t i = 0;
  for (; i < poly_bits / 8; ++i) {
    rem |= message[i] << ((poly_bits & 0xF8) - (i + 1) * 8);
  }
  for (; i < message_size; ++i) {
    for (unsigned j = 0; j < 8; ++j) {
      uint64_t masked = poly & -(rem >> (poly_bits - 1));
      rem = (rem << 1) | (message[i] >> (7 - j) & 1);
      rem ^= masked;
    }
  }
  for (unsigned j = 0, end = poly_bits; j < end; ++j) {
    uint64_t masked = poly & -(rem >> (poly_bits - 1));
    rem = (rem << 1);
    rem ^= masked;
  }
  return rem;
}

uint8_t count_bits(uint64_t number) {
  uint8_t bits;
  for (bits = 0; number > 0; number >>=1, ++bits);
  return bits;
}

const char *const usage_msg = "Usage: %s [FLAGS] POLYNOMIAL [MESSAGE]\n"
                              "When message is passed, calculates it's crc. When not,\n"
                              "generates a table for crc algorithm with POLYNOMIAL\n"
                              "in FULL form (ex.: 0x104c11db7 for standard CRC32 polynomial)\n"
                              "Up to 64-bit polynomials are supported\n\n"
                              "Supported flags:\n"
                              "  -c, --columns NUM               number of columns in table (default 8)\n"
                              "  -h, --help                      display this error message\n"
                              "  -t, --table-type BITS[ENDIAN]   specify table bits (default 8),\n"
                              "                                  when >8 also append target endianess\n"
                              "                                  (b or l for big or little respectively,\n"
                              "                                  by default little is assumed). ex.: 16l, 12b\n"
                              "  -v, --verbose                   be verbose\n";

//uint64_t table[256];

#define byteswap64(num) \
  (((num) >> 56) | (((num) >> 40) & 0xFF00) | (((num) >> 24) & 0xFF0000) | (((num) >> 8) & 0xFF0000) | \
   (((num) & 0xFF) << 56) | (((num) & 0xFF0000) << 40) | (((num) & 0xFF0000) << 24) | (((num) & 0xFF0000) << 8))

void crc_gentable_sameendian(uint64_t poly, uint8_t poly_bits, uint8_t table_bits, size_t columns) {
  uint64_t end = (1 << (table_bits & 63)) - 1 - (table_bits > 63);
  columns = MIN(columns, end);
  uint64_t num = -1;
  do {
    uint64_t max = MIN(end - columns + 1, (num + 1)) + columns - 1; // Working around all possible overflows
    do {
      ++num;
      printf("0x%"PRIX64", ", crc_iter((uint8_t*)&num, sizeof(uint64_t), poly, poly_bits));
    } while (num < max);
    putchar('\n');
  } while (num < end);
}

void crc_gentable_diffendian(uint64_t poly, uint8_t poly_bits, uint8_t table_bits, size_t columns) {
  uint64_t end = (1 << (table_bits & 63)) - 1 - (table_bits > 63);
  columns = MIN(columns, end);
  uint64_t num = -1;
  do {
    uint64_t max = MIN(end - columns + 1, (num + 1)) + columns - 1; // Working around all possible overflows
    do {
      ++num;
      uint64_t num_be = byteswap64(num);
      printf("0x%"PRIX64", ", crc_iter((uint8_t*)&num_be, sizeof(uint64_t), poly, poly_bits));
    } while (num < max);
    putchar('\n');
  } while (num < end);
}

void crc_gentable_8(uint64_t poly, uint8_t poly_bits, uint8_t table_bits, size_t columns) {
  uint8_t end = (1 << (table_bits & 7)) - 1 - (table_bits > 7);
  columns = MIN(columns, end);
  uint8_t num = -1;
  do {
    uint8_t max = MIN((uint8_t)(end - columns + 1), (uint8_t)(num + 1)) + columns - 1; // Working around all possible overflows
    do {
      ++num;
      printf("0x%"PRIX64", ", crc_iter(&num, 1, poly, poly_bits));
    } while (num < max);
    putchar('\n');
  } while (num < end);
}

#include "example.c"

int main(int argc, char *argv[]) {
  uint64_t polynomial = 0;
  size_t columns = 8;
  uint8_t table_bits = 8;
  uint8_t poly_bits = 0;
  _Bool target_big_endian = 0;
  _Bool verbose = 0;
  const char *const progname = argv[0];
  for (++argv, --argc; argc > 0; ++argv, --argc) {
    if (**argv != '-') {
      if (polynomial > 0)
        break; // message
      polynomial = strtoull(*argv, NULL, 0);
      if (errno == 0 && polynomial > 0) {
        continue;
      }
      if (errno != ERANGE) {
        if (polynomial == 0)
          puts("POLYNOMIAL 0 is incorrect");
        printf(usage_msg, progname);
        return -1;
      }
      errno = 0;
      char num[67] = {0};
      uint8_t string_size = strlen(*argv);
      memcpy(num, *argv, MIN(string_size-1, sizeof(num)-1));
      polynomial = strtoull(num, NULL, 0);
      if (string_size > sizeof(num) || errno == ERANGE) {
        printf(usage_msg, progname);
        return -1;
      }
      uint8_t num_offset = (num[0] == '0');
      num_offset += num_offset && !isdigit(num[1]);
      memcpy(num + num_offset, "10", 3);
      unsigned mul = strtoull(num, NULL, 0);
      memcpy(num + num_offset, *argv + string_size - 1, 2);
      polynomial = polynomial * mul + strtoull(num, NULL, 0);
      poly_bits = 64;
    }
    _Bool longarg = (*argv)[1] == '-';
    switch ((*argv)[1 + longarg]) {
      case 'c':
        if ((longarg && strcmp((*argv) + 2, "columns") != 0) || argc <= 1) {
          printf(usage_msg, progname);
          return -1;
        }
        ++argv;
        --argc;
        break;
      case 'h':
        printf(usage_msg, progname);
        return -(longarg && strcmp((*argv) + 2, "help") != 0);
      case 't':
        if ((longarg && strcmp((*argv) + 2, "table-type") != 0) || argc <= 1) {
          printf(usage_msg, progname);
          return -1;
        }
        ++argv;
        --argc;
        char *end;
        table_bits = strtoul(*argv, &end, 0);
        if (table_bits == 0) {
          printf(usage_msg, progname);
          return -1;
        }
        while (*end == '-' || *end == '_' || isblank(*end))
          ++end;
        if (*end != '\0') {
          target_big_endian = (strcasecmp(end, "b") == 0) || (strcasecmp(end, "big") == 0);
          if (!target_big_endian && !(strcasecmp(end, "l") == 0) || (strcasecmp(end, "little") == 0)) {
            printf(usage_msg, progname);
            return -1;
          }
        } else if (argc > 1 && (argv[1][0] == 'b' || argv[1][0] == 'B' || argv[1][0] == 'l' || argv[1][0] == 'L')) {
          target_big_endian = (strcasecmp(argv[1], "b") == 0 || strcasecmp(argv[1], "big") == 0);
          uint8_t addsub = target_big_endian || strcasecmp(argv[1], "l") == 0 || strcasecmp(argv[1], "little") == 0;
          argv += addsub;
          argc -= addsub;
        }
        break;
      case 'v':
        if (!longarg || strcmp((*argv) + 2, "verbose") == 0) {
          verbose = 1;
          break;
        }
        printf(usage_msg, progname);
        return -1;
    }
  }
  if (polynomial == 0 || columns == 0) {
    printf(usage_msg, progname);
    return -1;
  }
  size_t len = 0;
  if (poly_bits == 0)
    poly_bits = count_bits(polynomial) - 1;
  if (poly_bits < 1) {
    puts("0-bit polynomials are unsupported");
    return -1;
  }
  assert(poly_bits <= 64);
  if (verbose)
    printf("POLY: 0x%s%"PRIX64"\nBITS: %u\nTABLE_BITS: %u\nENDIAN: %s (native) -> %s (target)\n",
           (poly_bits == 64) ? "1" : "", polynomial, poly_bits, table_bits,
           ((union {uint32_t a; uint8_t b[4];}){.a=1}.b[3]) ? "BIG" : "LITTLE", target_big_endian ? "BIG" : "LITTLE");
  if (argc == 0) {
    // Native endianess check, 0 if little and 1 if big
    if (((union {uint32_t a; uint8_t b[4];}){.a=1}.b[3]) != target_big_endian)
      crc_gentable_sameendian(polynomial, poly_bits, table_bits, columns);
    else
      crc_gentable_diffendian(polynomial, poly_bits, table_bits, columns);
    return 0;
  }
  char *str;
  if (argc == 1) {
    str = *argv;
    len = strlen(str);
  } else {
    for (int i = 0; i < argc; ++i) {
      len += strlen(argv[i]);
    }
    str = malloc((len + 1) * sizeof(char));
    char *end = str;
    for (int i = 0; i < argc; ++i) {
      end = stpcpy(end, argv[i]);
    }
  }
  if (verbose)
    printf("STRING: \"%s\"\n", str);
  uint64_t result = crc_iter((uint8_t*)str, len, polynomial, poly_bits);
  //if (argc != 1)
    //free(str);
  printf("0x%"PRIX64"\n", result);
  printf("0x%X\n", crc4_4bit((uint8_t*)str, len));
  if (argc != 1)
    free(str);
  return 0;
}
