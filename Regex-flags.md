*[[argument]] of [[regex_match]], [[regex_search]] and [[regex_replace]] [[Operators and functions]]*

In the regex_match, regex_search and regex_replace functions an optional flag parameter can be configured to control how a source string is matched against the configured expression syntax. More information on this flag argument can be found here.

In the GeoDMS this argument is passed as uint32 parameter, see the next enumeration for how these parameter values relate to the described flags.

<pre>
match_default          = 0,
match_not_bol          = 1, /* 0x00000001: is not start of line */
match_not_eol          = match_not_bol << 1, /* 0x00000002: last is not end of line */
match_not_bob          = match_not_eol << 1, /* 0x00000004: first is not start of buffer */
match_not_eob          = match_not_bob << 1, /* 0x00000008: last is not end of buffer */
match_not_bow          = match_not_eob << 1, /* 0x00000010: first is not start of word */
match_not_eow          = match_not_bow << 1, /* 0x00000020: last not end of word */
match_not_dot_newline  = match_not_eow << 1, /* 0x00000040: \n is not matched by '.' */
match_not_dot_null     = match_not_dot_newline << 1, /* 0x00000080: '\0' is not matched by '.' */
match_prev_avail       = match_not_dot_null << 1, /* 0x00000100: *--first is a valid expression */
match_init             = match_prev_avail << 1, /* 0x00000200: internal use */
match_any              = match_init << 1, /* 0x00000400: don't care what we match */
match_not_null         = match_any << 1, /* 0x00000800: string can't be null */
match_continuous       = match_not_null << 1, /* 0x00001000: each grep match must continue from */
                                                    /* uninterupted from the previous one */`
match_partial          = match_continuous << 1, /* 0x00002000: find partial matches */

match_stop             = match_partial << 1, /* 0x00004000: stop after first match (grep) V3 only */
match_not_initial_null = match_stop, /* 0x00008000: don't match initial null, V4 only */
match_all              = match_stop << 1, /* 0x00010000 must find the whole of input even if match_any is set */
match_perl             = match_all << 1, /* 0x00020000 Use perl matching rules */
match_posix            = match_perl << 1, /* 0x00040000 Use POSIX matching rules */
match_nosubs           = match_posix << 1, /* 0x00080000 don't trap marked subs */
match_extra            = match_nosubs << 1, /* 0x00100000 include full capture information for repeated captures */
match_single_line      = match_extra << 1, /* 0x00200000 
                                            treat text as single line and ignor any \n's when  matching ^ and $. */
match_unused1          = match_single_line << 1, /* unused */
match_unused2          = match_unused1 << 1, /* unused */
match_unused3          = match_unused2 << 1, /* unused */
match_max              = match_unused3, /* 0x01000000
format_perl            = 0, /* perl style replacement */
format_default         = 0, /* ditto. */
format_sed             = match_max << 1, /* 0x02000000 sed style replacement. */
format_all             = format_sed << 1, /* 0x04000000 enable all extentions to sytax. */
format_no_copy         = format_all << 1, /* 0x08000000 don't copy non-matching segments. */
format_first_only      = format_no_copy << 1, /* 0x10000000 Only replace first occurance. */
format_is_if           = format_first_only << 1, /* 0200000000 internal use only. */
format_literal         = format_is_if << 1 /* 0x40000000 treat string as a literal */