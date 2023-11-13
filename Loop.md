*[[MetaScript functions]] loop*

## syntax

- loop(*template*, *number*)

## definition

loop(*template*, *number*) results in **a set of instantiated <I>[[templates|template]]</I>**.

The number of instantiations is specified by the *number* [[argument]]. This *number* is a maximum, a stop condition could result in less instantiations.

A currvalue and nextvalue item need to be configured within the *template*.

## description

Optional a stop condition can be configured. This condition need to be configured as a [[parameter]] with the name stop and an [[expression]] with a boolean condition. If the condition becomes true, the loop will not continue.

The loop function is used for dynamic modelling.

## example
<pre>
template LoopTemplate
{
   parameter&lt;uint32&gt; NrIter;
   container currValue;
   container nextValue;`
   container results
   {
      parameter&lt;uint16&gt; LoopWaarde := NrIter;
   }
   container loop := <B>loop(</B>LoopTemplate, 5<B>)</B>;
}
</pre>
