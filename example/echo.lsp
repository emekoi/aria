(do
  (import "net")
  (= S (net-newStream))

  (= onData (fn ()
    (printf "data from %s:%d -> %s" 
      (net-getAddress stream) 
      (net-getPort stream) data)
    (net-write stream data)))

  (= onError (fn ()
    (printf "error from %s:%d -> %s" 
      (net-getAddress stream) 
      (net-getPort stream) msg)))

  (= onAccept (fn ()
    (net-addListener remote "data" onData remote)
    (printf "accepted connection from %s:%d" 
      (net-getAddress remote) 
      (net-getPort remote))))

  (= onListen (fn ()
    (printf "listening on %s:%d"
      (net-getAddress stream)
      (net-getPort stream))))

  (net-addListener S "data" onData nil)
  (net-addListener S "error" onError nil)
  (net-addListener S "accept" onAccept nil)
  (net-addListener S "listen" onListen nil)

  (net-listen S "localhost" 8000)

  (while (> (net-getStreamCount) 0)
    (net-update)))
