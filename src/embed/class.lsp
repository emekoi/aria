(do

  (= class (macro fields
    (list 'fn '_init_args
      (list 'let (concat '(self nil
                           set-self (fn (x) (= self x))
                           init (fn args
                             (if super (apply super (cons 'init args)))))
                         fields)
        '(= self (fn (method . args)
          (let (m (eval method))
            (if (and m (isnt m (eval method global)))
                (apply m args)
                (apply super (cons method args))))))
        '(if super (super 'set-self self))
        '(apply init _init_args)
        'self))))


  (= getter (macro (sym)
    (list 'fn '() sym)))


  (= setter (macro (sym)
    (let (x (gensym))
      (list 'fn (list x) (list '= sym x)))))


  (= invoke (fn (args lst)
    (each (fn (x) (apply x args)) lst)))
