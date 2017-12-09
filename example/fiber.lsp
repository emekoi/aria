(do
  (= MAX 10)
  ; (let (i 0) (while (< i MAX)
  ;   (print i)
  ;   (++ i)))
  (= titleize (fiber (s)
    (join (map capitalize (split s " ")) " ")))
  (titleize "hello world")
)
