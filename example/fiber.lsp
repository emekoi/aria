(do 
	(= f (fiber x 
		(print "hello")
		(print "world")))
	(resume f)
	(print "done.")
	;(f)
	)