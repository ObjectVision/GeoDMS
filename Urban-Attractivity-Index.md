The Urban Attractivity Index describes the presence of four types of urban amenities: historic buildings and monuments; cultural facilities; shops; hotels, restaurants and other catering establishments. The numbers of units per amenity type are counted per 500 m grid cell, rescaled to a maximum value of 0.25 per type, added up and averaged out over a 2.5 km radius [(Broitman and Koomen, 2015)](https://doi.org/10.1016/j.compenvurbsys.2015.05.006).


```
container UrbanAttractivityIndex
{	
	parameter<string> Year := '2021'; //2018 2021
	unit<uint32>      vbo  := ='brondata/BAG/Snapshots/VBOs/Y'+Year+'01/vbo';
	unit<uint32>      pand := ='brondata/BAG/Snapshots/Panden/Y'+Year+'01/Pand';
	
	attribute<float32>   Utiliteit_opp              (NL_grid/domain)   := value(vbo/GebruiksdoelSets/UtiliteitsFucties/oppervlakte_ha, float32);
	attribute<float32>   Detailhandel_opp           (NL_grid/domain)   := vbo/GebruiksdoelSets/winkel/GebruiksdoelSet/Voorraad/oppervlakte_ha[float32];
	attribute<float32>   HorecaCultuur_count        (NL_grid/domain)   := vbo/GebruiksdoelSets/bijeenkomst/GebruiksdoelSet/Voorraad/count_ha[float32] + vbo/GebruiksdoelSets/logies/GebruiksdoelSet/Voorraad/count_ha[float32];
	attribute<float32>   Fractie_monumentaal        (NL_grid/domain)   := MakeDefined(																			pand/Voorraad/pand_met_vbo/monumentale_panden[float32] >= 3f																			? pand/Voorraad/pand_met_vbo/monumentale_panden[float32] / pand/Voorraad/pand_met_vbo/count_ha[float32]																			: 0f																		, 0f);

	attribute<float32>   Utiliteit                  (NL_grid/domain)   := potential(Utiliteit_opp, geometries/potentialen/pot5000m/potrange/AbsWeight_w_center);
	parameter<float32>   Max_Utiliteit                                 := max(Utiliteit);
	attribute<float32>   Detailhandel               (NL_grid/domain)   := potential(Detailhandel_opp, geometries/potentialen/pot5000m/potrange/AbsWeight_w_center);
	parameter<float32>   Max_Detailhandel                              := max(Detailhandel);
	attribute<float32>   Horeca                     (NL_grid/domain)   := potential(HorecaCultuur_count, geometries/potentialen/pot5000m/potrange/AbsWeight_w_center);
	parameter<float32>   Max_Horeca                                    := max(Horeca);
	attribute<float32>   Monumentaal                (NL_grid/domain)   := potential(Fractie_monumentaal, geometries/potentialen/pot5000m/potrange/AbsWeight_w_center);
	parameter<float32>   Max_Monumentaal                               := max(Monumentaal);
	
	attribute<float32>   Utiliteit_norm             (NL_grid/domain)   := Utiliteit / Max_Utiliteit;
	attribute<float32>   Detailhandel_norm          (NL_grid/domain)   := Detailhandel / Max_Detailhandel;
	attribute<float32>   Horeca_norm                (NL_grid/domain)   := Horeca / Max_Horeca;
	attribute<float32>   Monumentaal_norm           (NL_grid/domain)   := Monumentaal / Max_Monumentaal;

	attribute<float32>   UAI                       (NL_grid/domain)   := Utiliteit_norm + Detailhandel_norm + Horeca_norm + Monumentaal_norm;
	attribute<float32>   UAI_smoothed              (NL_grid/domain)   := potential(UAI, geometries/potentialen/pot2000m/potrange/AbsWeight_w_center);
	parameter<float32>   Max_UAI_smoothed                             := max(UAI_smoothed);
	attribute<float32>   UAI_smoothed_norm        (NL_grid/domain)   := UAI_smoothed / Max_UAI_smoothed;
	attribute<float32>   Make_Tiff                (NL_grid/domain)   := UAI_smoothed_norm, StorageName = "='%SourceDataDir%/SpatialData/UAI_'+Year+'.tif'";
}
```