(do
  (= t0 (clock))
  (= fib (fn (n)
    (if (>= n 2)
        (+ (fib (- n 1)) (fib (- n 2)))
        n)))
  (= n (number (or (nth 1 argv) (clamp (floor (frand 36)) 0 36))))
  (print (string "(fib " n ") -> " (fib n)))
  (print (string "time elaspsed: " (- (clock) t0) " secs")))
