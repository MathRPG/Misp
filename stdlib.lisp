; Atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

(def {fun} (\ {f b} {
	def (head f) (\ (tail f) b)
}))