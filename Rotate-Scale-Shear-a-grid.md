*[[Configuration examples]] Rotate Scale Shear a grid*

The script presents an example how you can rotate, stretch and shear a raster domain.

## example

<pre>
container Geography
{
    unit&lt;fpoint&gt; rdc_meter: range = "[{300000, 0}, {625000, 280000})"; 
}
container parameters
{
    unit&lt;float32&gt; meter := BaseUnit('meter', float32);
    parameter&lt;meter&gt; gridsize := 1000[meter];
                                               
    parameter&lt;meter&gt; NL_grid_X_min := 270000[meter];
    parameter&lt;meter&gt; NL_grid_Y_min := 325000[meter];
   
    parameter&lt;meter&gt; X_org := (     0f + (NL_grid_Y_min / 2f))[meter];
    parameter&lt;meter&gt; Y_org := (375000f - (NL_grid_X_min / 2f))[meter];
   
    parameter&lt;uint32&gt; aantalkolommen := uint32(((NL_grid_Y_min + NL_grid_X_min)*(sqrt(2f) / 2f)) / gridsize);
    parameter&lt;uint32&gt; aantalrijen    := uint32(((NL_grid_Y_min + NL_grid_X_min)*(sqrt(2f) / 2f)) / gridsize);
   
    parameter&lt;float32&gt; shear_factor     := 0.5f;
    parameter&lt;float32&gt; rotation_angle   := (pi() / 4d)[float32]; <I>// in radial=45 degrees</I>;
    parameter&lt;float32&gt; X_stretch_factor := gridsize;
    parameter&lt;float32&gt; Y_stretch_factor := gridsize;
}

unit&lt;fpoint&gt; domain; 
unit&lt;spoint&gt; rotated_grid_domain := 
   range(domain, point(0s, 0s), point(parameters/aantalkolommen[int16], parameters/aantalrijen[int16]));

unit&lt;spoint&gt; conversion_matrix := range(spoint, point(0s,0s), point(2s,2s))
,  Using = "parameters"
{
    atrribute&lt;float32&gt; A_stretch  := union_data(., Y_stretch_factor, 0f, 0f, X_stretch_factor);
    atrribute&lt;float32&gt; B_shear    := union_data(., 1f, shear_factor, 0f, 1f);
    atrribute&lt;float32&gt; C_rotation := 
       union_data(., cos(rotation_angle), sin(rotation_angle), -sin(rotation_angle), cos(rotation_angle));
    atrribute&lt;float32&gt; AxB        := matr_mul(A_stretch, B_shear,.);
    atrribute&lt;float32&gt; AxBxC      := matr_mul(AxB, C_rotation,.);
    atrribute&lt;float32&gt; Result     := AxBxC;
}

unit&lt;upoint&gt; grid_coords := range(upoint, point(0,0), point(2, #rotated_grid_domain))
{
   atrribute&lt;float32&gt; rotated_grid := 
      union_data(., PointCol(id(rotated_grid_domain))[float32], PointRow(id(rotated_grid_domain))[float32]);
   atrribute&lt;float32&gt; rd           := matr_mul(conversion_matrix/Result, rotated_grid, .);
}

unit&lt;uint32&gt; grid_domain := range(uint32, 0, #rotated_grid_domain)
,  Using      = "parameters"
,  DialogData = "rotated_grid_rd"
,  DialogType = "map"
{ 
   atrribute&lt;Geography/rdc_meter&gt; rotated_grid_rd  := 
     point( first(grid_coords/rd, pointcol(id(grid_coords))[.]) + Y_org
	   , last(grid_coords/rd, pointcol(id(grid_coords))[.]) + X_org
     , geography/rdc_meter);
}
</pre>