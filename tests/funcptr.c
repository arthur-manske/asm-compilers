int (*func)(void) = (void *)0;

int func_with_callback(int (*fn)(void))
{
	return fn();
}

int dummy(void)
{
	return 1;
}

int main(void)
{
	int (*func_array[10])(void) = {0};

	func = main;

	func_with_callback(dummy);
	return 0;
}
