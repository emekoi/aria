(do
  (= monte (fn (n) 
  	(= inside 0)
  	(= counter 0)
  	(while (< counter n)
  	  (if (<= (+ (sqr (frand)) (sqr (frand))) 1) (++ inside))
  	  (++ counter))
  	(/ (* 4 inside) n)))

  	(= N (tonumber (or (nth 1 argv) 1e4)))
    (print (monte N)))
