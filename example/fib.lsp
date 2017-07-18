(do
  (= t0 (clock))
  (= fib (fn (n)
    (if (>= n 2)
        (+ (fib (- n 1)) (fib (- n 2)))
        n)))
  (= n (tonumber (or (nth 1 argv) (clamp (floor (frand 36)) 0 36))))
  (print "(fib " n ") -> " (fib n))
  (print "time elaspsed: " (- (clock) t0) " secs"))
