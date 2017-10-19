# aria
A *tiny*, easily embeddable lisp-shaped language implemented in C99.

```lisp
(do

  (= fib (fn (n)
    (if (>= n 2)
        (+ (fib (- n 1)) (fib (- n 2)))
        n)))

  (print (fib 20))) ; prints 6765
```


## Overview
* Supports numbers (floats), symbols, strings, pairs, lambdas, macros
* Stack tracebacks with filename and line numbers on error
* Exception-like error handling
* Simple implementation and garbage collector
* Easy-to-use C API


## License
This library is free software; you can redistribute it and/or modify it under
the terms of the MIT license. See [LICENSE](LICENSE) for details.

## Other
* https://github.com/ceramic/ceramic
* http://libmill.org/
* https://github.com/sustrik/libmill
* https://millcomputing.com/topic/coroutines-greenlets-in-mill/
* https://millcomputing.com/topic/threadscoroutines/
* https://docs.rs/crate/rustyline/1.0.0/source/src/tty/unix.rs
* https://github.com/kroitor/asciichart/blob/master/asciichart.js

## TODO
* add loading from any shared lib; search for a aria symbol then dynamic symbol