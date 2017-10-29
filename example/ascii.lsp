(do 
	(= plot (fn (series cfg) 

    (= _min (nth 0 series))
    (= _max (nth 0 series))

    (let (i 0) (while (< i (len series)) 
      (= _min (min _min (nth i series)))
      (= _max (max _max (nth i series)))
      (++ i)))

    (= range (abs (- _max _min)))

    (= offset (or (alref 'offset cfg) 3))
    (= padding (or (alref 'padding cfg) "       "))
    (= height  (or (alref 'height cfg) range))
    (= ratio (/ height range))
    ; (= min2    Math.round (min * ratio))
    ; (= max2    Math.round (max * ratio))
    ; (= rows    Math.abs (max2 - min2))
    ; (= width   series.length + offset)
 
    ))

  (plot '(1 6 7 5) nil)
)

