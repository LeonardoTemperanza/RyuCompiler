
Simple testing system. Every test should be put in this directory,
and can have a comment at the top specifying the intended behavior of the program.
The syntax is the following:

/* keyword: value   keyword: value */
// keyword: value   keyword: value

If not specified, the default value of a keyword will be used.

These are the keywords with their respective default value:

should_compile: true
program_exit:   0
