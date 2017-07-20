(while t (pcall
          (fn () (print (string "-> " (eval (parse (readline)) global))))
          (fn (err tr)
            (print "error:" err)
            (print "traceback:")
            (while tr
              (print (string "  [" (dbgloc (car tr)) "] "
                              (substr (string (car tr)) 0 50)))
              (= tr (cdr tr))))))
