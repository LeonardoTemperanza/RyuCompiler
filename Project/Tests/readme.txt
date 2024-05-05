
Simple testing system. Every test should be put in this directory,
and can have a comment at the top specifying the intended behavior of the program.
The comment has to be at the top, and must have a "@test" tag at the beginning,
otherwise the directives will just be ignored.
The syntax is the following:

/* @test
keyword: value   
keyword: value   keyword: value  keyword: value
...
*/

* OR *

// @test keyword: value   keyword: value ...

If not specified, the default value of a keyword will be used.

These are the keywords with their respective default value:

should_compile: true
program_exit:   0
ignore_test:    false  // (useful for files that don't have a main, and are imported by other files)
