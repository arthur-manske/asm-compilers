// Diabolical pointer test
// A function returning a pointer to an array of 10 pointers to functions returning a pointer to an int
int *(*(*f(int *(*(*(*a)[5])(void *))[10]))(void))[5];

int main(void) {
    return 0;
}
