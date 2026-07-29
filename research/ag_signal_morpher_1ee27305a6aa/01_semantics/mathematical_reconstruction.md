# Implementierungsgetreue Rekonstruktion

## Originalausdruck

```text
K(x) =
  logabs(sin(cos(sdiv(x,x))))
  - tanh(
      sdiv(
        -0.357064 * sin(sdiv(x,x)),
        logabs(sin(cos(sdiv(x,x))))
        - logabs(sin(cos(sdiv(x,x))))
      )
    )
  - sin(
      logabs(sin(cos(sdiv(x,x))))
      * (sin(cos(sdiv(x,x))) + cos(sin(sdiv(x,x))))
    )
```

## Geschützte Algebra

Mit den tatsächlich implementierten Operatoren gilt

```text
q(x) = x / (|x| + 10^-6)
A(q) = log(|sin(cos(q))| + 10^-9)
B(q) = sin(cos(q)) + cos(sin(q))
N(q) = -0.357064 * sin(q)
```

Die beiden Auswertungen von `A(q)` sind deterministisch identisch. Ihr
Differenzterm ist deshalb in den vorhandenen Backends exakt `0`. Damit ist die
zulässige implementierungsgetreue Darstellung

```text
K(x) = A(q(x))
       - tanh(N(q(x)) / 10^-6)
       - sin(A(q(x)) * B(q(x)))
```

Dies ist keine schulalgebraische Vereinfachung von `x/x` zu `1`.
`q(0)=0`, und für große positive bzw. negative Beträge nähert sich `q` den
Werten `+1` bzw. `-1`.

## AST

```text
Sub
├── Sub
│   ├── LogAbs(Sin(Cos(SDiv(x,x))))
│   └── Tanh
│       └── SDiv
│           ├── Mul(Sin(SDiv(x,x)), -0.357064)
│           └── Sub(A(x), A(x))
└── Sin
    └── Mul
        ├── A(x)
        └── Add(Sin(Cos(SDiv(x,x))), Cos(Sin(SDiv(x,x))))
```

## Formel-, Schutz- und Wrapperwirkung

- **Fakt – Formel plus `sdiv`:** Der aktive Eingang wird durch `q(x)` auf ein
  weiches Vorzeichenintervall komprimiert.
- **Fakt – Schutzwirkung:** Der Schutznenner `10^-6` verstärkt `N(q)` stark.
  Der `tanh`-Term wird deshalb außerhalb einer extrem kleinen Nullumgebung fast
  zu `-1` für positive und `+1` für negative Eingaben.
- **Fakt – `sanitize`:** Nichtendliche Eingaben werden auf `0` abgebildet;
  endliche Werte werden auf `[-10^6,10^6]` begrenzt.
- **Ableitung – Kernelklasse:** begrenzter, hochsensitiver
  Polaritätsseparator mit zwei äußeren Plateaus.
- **Wrapperwirkung:** Erst
  `g(z)=clip((1+tanh(K(z)))/2,0.05,0.95)` erzeugt einen nichtnegativen
  synaptischen Wirksamkeitsfaktor. Diese Projektion gehört nicht zum Kernel.

Der unveränderte Export in `exports/` wurde weder überschrieben noch
vereinfacht.

