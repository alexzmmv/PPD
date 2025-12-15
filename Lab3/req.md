
Write several programs to compute the product of two matrices.

Have a function that computes a single element of the resulting matrix.

Have a second function whose each call will constitute the thread function (and will be called on several threads in parallel). This function will call the above one several times consecutively to compute several elements of the resulting matrix. Consider the following ways of splitting the work between threads (for the examples, consider the final matrix being 9x9 and the work split between 4 threads):

Each thread computes consecutive elements, going row after row. So, thread 0 computes rows 0 and 1, plus elements 0-1 of row 2 (20 elements in total); thread 1 computes the remainder of row 2, row 3, and elements 0-3 of row 4 (20 elements); thread 2 computes the remainder of row 4, row 5, and elements 0-5 of row 6 (20 elements); finally, thread 3 computes the remaining elements (21 elements).
Each thread computes consecutive elements, going column after column. This is like the previous example, but interchanging the rows with the columns: thread 0 takes columns 0 and 1, plus elements 0 and 1 from column 2, and so on.
Each thread takes every k-th element (where k is the number of threads), going row by row. So, thread 0 takes elements (0,0), (0,4), (0,8), (1,3), (1,7), (2,2), (2,6), (3,1), (3,5), (4,0), etc.
For debugging, each time you call the function computing an element of the resulting matrix, print the row and column of the element being computed and the thread number (0 to nr_threads-1) of the current thread.


The sizes of the matrix;
The number of threads;

Usage: ./exe n m p threads strategy
  A is n x m, B is m x p, C is n x p
  strategy: row | col | cyclic

example: ./exe 4 4 4 3 row
./exe 1000 1000 1000 8 row