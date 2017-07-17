(do

  (= fib (fn (n)
    (if (>= n 2)
        (+ (fib (- n 1)) (fib (- n 2)))
        n)))

  (print (fib (or (ord (nth 1 argv)) 20)))) ; prints 6765
