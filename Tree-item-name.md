## Syntax

[[tree item|tree item]] names

1.  may contain (alpha)numeric characters <pre>A..Z, a..z, 0..9, _(underscore, U+005F)</pre>
2.  may not start with a numeric character
3.  for version 7.315 and later: UTF characters U+0080 and higher codes are also allowed, such as: €, °, ß, π, δ, Δ
    -   Mathematical characters are also allowed (with or without sub and superscripts), such as: σₓ, σᵧ, σₓᵧ, α, β, ŷ, ȳ, x̄, ∑
4.  The following characters can not be used in tree item names:
    -   Characters with specific meaning in calculation rules, more specifically: all [[operators|Operator]] , `(` `)` and `[` `]` brackets, comma (U+002C), quotes, spaces (U+0020).
    -   Characters that have specific meaning in the GeoDms language, more specifically: `{`, `}`, `;`, and `"`
    -   Control characters (U+0000 .. U+001F)
5.  The following character is reserved for future use and should not be used in item names: `@` (U+0040)

## Case Sensitivity

[[Tree item]] names are case insensitive with regard to the UTF alphabetical characters below U+0080, thus `A` is identified with `a`, `B` with `b` , ... and `Z` with `z`. Other characters, such as from the Greek alphabet are case sensitive.

We advice to use upper or lowercase characters consistently in tree item names as we are preparing and planning to make tree item names and GeoDMS configurations case sensitive, possibly with a fallback option during a migration period.

## Naming Conventions

For the naming of tree items see [[naming conventions]].

## Implementation in the GeoDMS

The checking of treeItem names is done by the functions `itemNameFirstChar_test` and `itemNameNextChar_test`, defined in
_%geodmsfolder%\rtc\dll\src\utl\Encodes.h_