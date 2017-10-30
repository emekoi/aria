(do 
  (= make-grid (fn (grid)
    (let (lst (join (car grid)))
      (each (fn (row) (= lst (string lst "\n"
        (join row)))) (cdr grid)) lst))
  )

	(= plot (fn (series cfg) 

    (= _min (nth 0 series))
    (= _max (nth 0 series))

    (let (i 1) (while (< i (len series)) 
      (= _min (min _min (nth i series)))
      (= _max (max _max (nth i series)))
      (++ i)))

    (= range (abs (- _max _min)))

    (= offset  (or (alref 'offset cfg) 3))
    (= padding (or (alref 'padding cfg) "       "))
    (= height  (or (alref 'height cfg) range))
    (= ratio   (/ height range))
    (= min2    (round (* _min ratio)))
    (= max2    (round (* _max ratio)))
    (= rows    (abs (- max2 min2)))
    (= width   (+ (len series) offset))
    (= _format (or (alref 'format cfg)
      (fn (x) (substr (string padding
        (round x .2)) (* (strlen padding) -1)))))

    (let (i 0 res nil) (while (<= i rows) 
      (let (j 0) (while (< j width)
        (push " " res)
        (++ j)))
      (push res result)
      (= res nil)
      (++ i)))

    (let (y min2 label nil) (while (<= y max2) ; axis + labels
      (= label (_format (- _max (/ (* (- y min2) range) rows)) (- y min2)))
      (set (max (- offset (strlen label)) 0)
        label (nth (- y min2) result))
      (set (- offset 1) (or (and (is y 0) "┼") "┤")
        (nth (- y min2) result))
      (++ y)))

    (= y0 (- (round (* (nth 0 series) ratio)) min2))
    (set (- offset 1) "┼" (nth (- rows y0) result)) ; first value

    (let (x 0) (while (< x (- (len series) 1))
      (= y0 (- (round (* (nth (+ x 0) series) ratio)) min2))
      (= y1 (- (round (* (nth (+ x 1) series) ratio)) min2))
      (if (is y0 y1) (set (+ x offset) "─" (nth (- rows y0) result))
        (do
          (set (+ x offset) (or (and (> y0 y1) "╰") "╭") (nth (- rows y1) result))
          (set (+ x offset) (or (and (> y0 y1) "╮") "╯") (nth (- rows y0) result))
          (= from (min y0 y1) to (max y0 y1))
          (let (y (+ from 1)) (while (< y to)
            (set (+ x offset) "│" (nth (- rows y) result))
            (++ y)))
        ))
      (++ x)))
    (make-grid result)))

  (= SIZE 120)
  (let (i 0) (while (< i SIZE)
    (push (* (cos (* i (/ (* math-pi 4) SIZE))) 15) s0)
    (++ i)))
  (print (plot s0 nil))
)

