Begrippen:

-   bouwdeel := { RO, RB, DR, PL, MG, MS, DS, DP }
-   bouwdeelkwaliteit := { L, M, H }
-   bouwdeelverbeterdoel := { X, M, H }
-   bouwdeelverbetering := { LM, MH, LH }
-   bouwdeelverbeterpakket := relevant bouwdeel (ten minste 1)-> bouwdeelverbeterdoel
-   bouwdeelverbeteroptie := combinatie van bouwdeelverbeterpakketten waarin geen bouwdelen dubbel voor komen
-   activeringscriterium := (woning x zichtjaar) -> ( bouwdeel -> {actief, inactief } ) ; er mogen alleen verbeteringen plaats vinden
    aan actieve bouwdelen.
    -   criteria voorbeelden:
        -   technische levensduur van bouwdelen (eerdere verbetering of proxy obv bouwjaar) of installatie of natuurlijk investeringsmoment.
        -   beleid: geografie (bijvoorbeeld enkele CBS buurten), woningcategorie en/of eigendomssituatie.
        -   mee-activatie: bouwdeelsubset actief -> andere selecties ook actief
        -   percentage overige woningen op bouwdeel(sub)set
    -   of: (bouwdeel x zichtjaar) -> woning -> ( {actief, inactief } )
    -   of: bouwdeel -> (woning x zichtjaar) -> ( {actief, inactief } )
-   WoningType := { Vrijstaand, 2 onder 1 kap, rijwoning hoek, rijwoning tussen, Meergezins max 4 verdiepingen, Meergezins >4 verdiepingen }
-   Bouwjaarklasse := { 1930...., 1946..., 1965..., 1975..., 1992...,
    1996..., 2000..., 2006..., 2011..., 2015 }
-   ModelObject := WoningType x Bouwjaarklasse
-   verbeterkostentabel := ModelObject -> bouwdeel -> bouwdeelverbetering -> (min, max: €/m^2)
-   bouwdeelverbeterpakket-toepasbaarheids-tabel := bouwdeelverbetepakket -> (woning x zichtjaar) -> Boolean

Bovenstaande bepaalt per (woning x zichtjaar) een set van toegestane bouwdeelverbeteropties.

-   issue: wat is de (qua performance) gewenste volgorde van toepassing bepaling set van bouwdeelverbeteropties. Eerst bouwdeelverbeteropties vaststellen en dan activeringscriteria checken of (beter) per bouwdeelverbeterpakket checken met het relevante deel van de activeringscriteria
