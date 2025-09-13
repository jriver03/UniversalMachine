#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 10000000

int cmpfunc(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

int main() {
    int *arr = malloc(N *sizeof(int));

    srand(42);

    for (int i = 0; i < N; i++) {
        arr[i] = rand();
    }

    clock_t start = clock();

    qsort(arr, N, sizeof(int), cmpfunc);

    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Sorted %d ints in %.2f seconds\n", N, elapsed);

    free(arr);
    return 0;
}