*[[Miscellaneous functions]] FenceContainer*

## syntax

- FenceContainer(_container_, _string message_)

## description

FenceContainer calculates all [[subitem]]s in the specified _[[container]]_ that are made available to consumers as subitems of the resulting container and posts the configured _string message_ in the EventLog when finished before any of the resulting subitems can be used. This enforces the scheduler to first complete a set of calculations before commencing on a subsequent set.

Starting from version 14.4.0, any calculation task that is scheduled after the calculation of a fenced container will only start after the calculation of all fenced tasks will be completed thus enabling a modeller to synchronize memory intensive sub-tasks and limit simultaneous memory allocation for such separated sub-tasks.

## example

<pre>
container Results := <B>FenceContainer(</B>Results0, 'Results '+Context/ThisIterName+' finished calculating'<B>)</B>;
</pre>

![image](https://github.com/ObjectVision/GeoDMS/assets/48675272/d00d3ddf-4a99-45c2-952c-577653c5ab27)
