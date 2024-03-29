; Atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

; Define functions
(def {fun} (\ {f b} {
	def (head f) (\ (tail f) b)
}))


(fun {unpack f l} {
	eval (join (list f) l)
})

(fun {pack f & xs} {f xs})

(def {curry} unpack)
(def {uncurry} pack)

; Sequence
(fun {do & l} {
	if (== l nil)
		{nil}
		{last l}
})

(fun {let b} {
	((\ {_} b) ())
})

(fun {not x} {- 1 x})
(fun {or x y} {+ x y})
(fun {and x y} {* x y})

(fun {flip f a b} {f b a})
(fun {ghost & xs} {eval xs})
(fun {comp f g x} {f (g x)})

(fun {fst l} { eval (head l ) })
(fun {snd l} { eval (head (tail l)) })
(fun {trd l} { eval (head (tail (tail l))) })

(fun {len l} {
	if (== l nil)
		{0}
		{+ 1 (len (tail l))}
})

(fun {nth n l} {
	if (== n 0)
		{fst l}
		{nth (- n 1) (tail l)}
})

(fun {last l} {nth (- (len l) 1) l})

(fun {take n l} {
	if (== n 0)
		{nil}
		{join (head l) (take (- n 1) (tail l))}
})

(fun {drop n l} {
	if (== n 0)
		{l}
		{drop (- n 1) (tail l)}
})

(fun {split n l} {list (take n l) (drop n l)})

(fun {elem x l} {
	if (== l nil)
		{false}
		{if (== x (fst l)) {true} {elem x (tail l)}}
})

(fun {map f l} {
	if (== l nil)
		{nil}
		{join (list (f (fst l))) (map f (tail l))}
})

(fun {filter f l} {
	if (== l nil)
		{nil}
		{join
			(if (f (fst l)) {head l} {nil})
			(filter f (tail l))
		}
})

(fun {foldl f z l} {
	if (== l nil)
		{z}
		{foldl f (f z (fst l)) (tail l)}
})

(fun {sum l} {foldl + 0 l})
(fun {product l} {fold * 1 l})

(fun {select & cs} {
	if (== cs nil)
		{error "No Selection Found"}
		{if (fst (fst cs)) {snd (fst cs)} {unpack select (tail cs)}}
	})

(def {otherwise} true)

(fun {month-day-suffix i} {
	select
		{(== i 1) "st"}
		{(== i 2) "nd"}
		{(== i 3) "rd"}
		{otherwise "th"}
})

(fun {case x & cs} {
	if (== cs nil)
		{error "No Case Found"}
		{if (== x (fst (fst cs))) {snd (fst cs)} {
			unpack case (join (list x) (tail cs))
		}}
})

(fun {day-name x} {
	case x
		{0 "Monday"}
		{1 "Tuesday"}
		{2 "Wednesday"}
		; etc
})

(fun {fib n} {
	select
		{ (< n 0) (error "fib of negative value!") }
		{ (== n 0) 0 }
		{ (== n 1) 1 }
		{ otherwise (+ (fib (- n 1)) (fib (- n 2))) }
})