(do
  (import "net")

  (= C (net-newStream))

  (= onLine (fn ()
    (printf "data from %s:%d -> %s"
      (net-getAddress stream)
      (net-getPort stream) data)))

  (= onError (fn ()
    (printf "error from %s:%d -> %s"
      (net-getAddress stream)
      (net-getPort stream) msg)))

  (= onConnect (fn ()
    (printf "connected to %s:%d"
      (net-getAddress stream)
      (net-getPort stream))))

  (net-addListener C "line" onLine nil)
  (net-addListener C "error" onError nil)
  (net-addListener C "connect" onConnect nil)

  (net-connect C "time.nist.gov" 13)
  ;
  ; (while (> (net-getStreamCount) 0)))
    (net-update)
