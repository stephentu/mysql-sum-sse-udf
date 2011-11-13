MySQL sum() aggregate using Intel SSE instructions
==================================================
The idea here is to explore the performance benefits of using Intel SSE (SIMD)
instructions for implementing the sum() aggregate operator in MySQL. This
library provides two primitives, a 32-bit `sum_sse_i32()` aggregate operator,
and a 64-bit `sum_sse_i64()` aggregate operator. The 32-bit version will
overflow if the result exceeds the maximum value of a 32-bit signed integer,
whereas the 64-bit version will overflow if the result exceeds the maximum
value of a 64-bit signed integer.

Installation
============
Compile and install the shared library. You might need to edit the `Makefile`
to fix any hardcoded paths. This assumes a version of `gcc` which supports
compiler intrinsics for SSE instructions:

    make
    sudo make install

Create the functions in MySQL:

    CREATE AGGREGATE FUNCTION sum_sse_i32 RETURNS INTEGER SONAME 'libsumsse.so';
    CREATE AGGREGATE FUNCTION sum_sse_i64 RETURNS INTEGER SONAME 'libsumsse.so';

Usage
=====
You use it pretty much as a drop in replacement for `sum()` on integers:

    SELECT sum_sse_i32(x) FROM my_table;

Except you have to pick the appropriate version to use to prevent
overflow.

Performance
===========
The performance numbers are not fantastic, since the implementation is a bit
simplistic. Here is the example of a microbenchmark run on 20M row table,
comparing `sum()` versus `sum_sse_i64()`:

    mysql> select SQL_NO_CACHE sum(i) from foo;
    +-----------------+
    | sum(i)          |
    +-----------------+
    | 199999990000000 |
    +-----------------+
    1 row in set (9.36 sec)

    mysql> select SQL_NO_CACHE sum_sse_i64(i) from foo;
    +-----------------+
    | sum_sse_i64(i)  |
    +-----------------+
    | 199999990000000 |
    +-----------------+
    1 row in set (8.26 sec)

For this example, the table size was around 76M (with a much larger buffer pool). 
The queries had been run a few times before to load the table into the buffer pool.
As expected, `sum_sse_i32()` performs a little bit better, but since it overflows
for this example, the results are not listed.
