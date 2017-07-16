(do



  (= capitalize (fn (s)
    (string (upper (substr s 0 1)) (substr s 1))))

  (= titleize (fn (s)
    (join (map capitalize (split s " ")) " ")))


  (print (titleize "hello world"))) ; prints "Hello World"
