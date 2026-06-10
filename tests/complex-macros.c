extern int printf(const char *, ...);

#define COMPLEX_MACRO(x)                                                                                               \
	for (int i = 0; i < 10; i++) {                                                                                 \
		printf("%d ", (x) + i);                                                                                \
	}

#define ANOTHER_MACRO(x)                                                                                               \
	COMPLEX_MACRO(x)                                                                                               \
	printf("\n");

#define YET_ANOTHER_MACRO(x)                                                                                           \
	ANOTHER_MACRO(x)                                                                                               \
	printf("Done!\n");

#define FINAL_MACRO(x)                                                                                                 \
	YET_ANOTHER_MACRO(x)                                                                                           \
	printf("All macros executed.\n");

#define CALL_FINAL_MACRO(x) FINAL_MACRO(x)

#define DO_WHILE_MACRO(x)                                                                                              \
	do {                                                                                                           \
		printf("Do while macro %d\n", (x));                                                                    \
	} while (0);

int main(void)
{
	CALL_FINAL_MACRO(5);
	DO_WHILE_MACRO(10);
	return 0;
}
