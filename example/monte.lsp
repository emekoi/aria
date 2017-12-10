(do
  (= monte (fn (n)
  	(= inside 0)
  	(= counter 0)
  	(while (< counter n)
  	  (if (<= (+ (pow (frand) 2) (pow (frand) 2)) 1) (++ inside))
  	  (++ counter))
  	(/ (* 4 inside) n)))

  	(= N (number (or (nth 1 argv) 1e4)))
    (print (monte N)))
