(do 
	(= f (fiber (x)
		(print "hello" x)
		(yield f)
		(print "goodbye" x)))
	(f "foo")
	(f "bar")
	)