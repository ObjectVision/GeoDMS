*[[Miscellaneous functions]] GeoDmsPatchNumber*

## syntax

- GeoDmsPatchNumber()

## definition

GeoDmsPatchNumber() results in a uint32[[parameter]] with the **GeoDMS patch number**.

Up until version 8.045 its value was defined as <I>MajorVersion + 0.001 * MinorVersion</I>.

Since November 2022 we use [semantic versioning](https://semver.org/), following the format <I>MajorVersion.MinorVersion.PatchNumber</I>. The GeoDmsVersion() is now defined as <I>MajorVersion</I> + 0.01 * <I>MinorVersion</I> + 0.0001 * <I>PatchNumber</I>;

The first semantic version number is 8.5.0 in order to order well with earlier version numbers. In GeoDms v8.5.1, the result of the GeoDmsVersion() is (the Float64 representation of) 8.0501.

## example

```
GeoDmsMajorVersionNumber(): UInt32
GeoDmsMinorVersionNumber(): UInt32
GeoDmsPatchNumber(): UInt32
```

```
parameter<string> GeoDMSVersion := string(GeoDmsMajorVersionNumber()) 
                                     +'.'+ 
                                   string(GeoDmsMinorVersionNumber()) 
                                     +'.'+ 
                                   string(GeoDmsPatchNumber());`
```



## since version

9.0.4

