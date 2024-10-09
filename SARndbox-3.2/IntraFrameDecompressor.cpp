/***********************************************************************
IntraFrameDecompressor - Class to decompress a single bathymetry or
water level grid.
Copyright (c) 2023 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "IntraFrameDecompressor.h"

#include "HuffmanBuilder.h"

namespace {

/******************************************************
Huffman decoding tree for the intra-frame decompressor:
******************************************************/

static const HuffmanBuilder::Node intraFrameDecompressorTree[]=
	{
	{~0U,1U,4U},{~0U,2U,3U},{256U,0U,0U},{255U,0U,0U},{~0U,5U,1026U},{~0U,6U,1025U},{~0U,7U,8U},{258U,0U,0U},
	{~0U,9U,1024U},{~0U,10U,1023U},{~0U,11U,1022U},{~0U,12U,315U},{~0U,13U,14U},{260U,0U,0U},{~0U,15U,314U},{~0U,16U,17U},
	{250U,0U,0U},{~0U,18U,19U},{249U,0U,0U},{~0U,20U,311U},{~0U,21U,134U},{~0U,22U,131U},{~0U,23U,80U},{~0U,24U,79U},
	{~0U,25U,78U},{~0U,26U,55U},{~0U,27U,54U},{~0U,28U,47U},{~0U,29U,46U},{~0U,30U,45U},{~0U,31U,34U},{~0U,32U,33U},
	{76U,0U,0U},{422U,0U,0U},{~0U,35U,44U},{~0U,36U,37U},{77U,0U,0U},{~0U,38U,39U},{405U,0U,0U},{~0U,40U,41U},
	{19U,0U,0U},{~0U,42U,43U},{506U,0U,0U},{10U,0U,0U},{75U,0U,0U},{341U,0U,0U},{311U,0U,0U},{~0U,48U,53U},
	{~0U,49U,52U},{~0U,50U,51U},{135U,0U,0U},{89U,0U,0U},{171U,0U,0U},{184U,0U,0U},{213U,0U,0U},{~0U,56U,57U},
	{214U,0U,0U},{~0U,58U,69U},{~0U,59U,60U},{172U,0U,0U},{~0U,61U,66U},{~0U,62U,63U},{370U,0U,0U},{~0U,64U,65U},
	{57U,0U,0U},{475U,0U,0U},{~0U,67U,68U},{380U,0U,0U},{152U,0U,0U},{~0U,70U,77U},{~0U,71U,72U},{322U,0U,0U},
	{~0U,73U,76U},{~0U,74U,75U},{45U,0U,0U},{78U,0U,0U},{439U,0U,0U},{190U,0U,0U},{278U,0U,0U},{235U,0U,0U},
	{~0U,81U,126U},{~0U,82U,89U},{~0U,83U,88U},{~0U,84U,85U},{294U,0U,0U},{~0U,86U,87U},{310U,0U,0U},{189U,0U,0U},
	{220U,0U,0U},{~0U,90U,91U},{222U,0U,0U},{~0U,92U,125U},{~0U,93U,104U},{~0U,94U,101U},{~0U,95U,96U},{95U,0U,0U},
	{~0U,97U,98U},{348U,0U,0U},{~0U,99U,100U},{55U,0U,0U},{85U,0U,0U},{~0U,102U,103U},{331U,0U,0U},{150U,0U,0U},
	{~0U,105U,110U},{~0U,106U,109U},{~0U,107U,108U},{392U,0U,0U},{117U,0U,0U},{328U,0U,0U},{~0U,111U,114U},{~0U,112U,113U},
	{44U,0U,0U},{118U,0U,0U},{~0U,115U,118U},{~0U,116U,117U},{23U,0U,0U},{110U,0U,0U},{~0U,119U,124U},{~0U,120U,123U},
	{~0U,121U,122U},{423U,0U,0U},{500U,0U,0U},{406U,0U,0U},{2U,0U,0U},{296U,0U,0U},{~0U,127U,130U},{~0U,128U,129U},
	{285U,0U,0U},{286U,0U,0U},{229U,0U,0U},{~0U,132U,133U},{240U,0U,0U},{271U,0U,0U},{~0U,135U,136U},{244U,0U,0U},
	{~0U,137U,246U},{~0U,138U,157U},{~0U,139U,156U},{~0U,140U,141U},{513U,0U,0U},{~0U,142U,155U},{~0U,143U,144U},{200U,0U,0U},
	{~0U,145U,150U},{~0U,146U,149U},{~0U,147U,148U},{394U,0U,0U},{144U,0U,0U},{334U,0U,0U},{~0U,151U,152U},{168U,0U,0U},
	{~0U,153U,154U},{81U,0U,0U},{136U,0U,0U},{295U,0U,0U},{280U,0U,0U},{~0U,158U,191U},{~0U,159U,160U},{289U,0U,0U},
	{~0U,161U,162U},{211U,0U,0U},{~0U,163U,178U},{~0U,164U,165U},{187U,0U,0U},{~0U,166U,169U},{~0U,167U,168U},{400U,0U,0U},
	{139U,0U,0U},{~0U,170U,177U},{~0U,171U,176U},{~0U,172U,175U},{~0U,173U,174U},{447U,0U,0U},{61U,0U,0U},{426U,0U,0U},
	{43U,0U,0U},{373U,0U,0U},{~0U,179U,182U},{~0U,180U,181U},{342U,0U,0U},{101U,0U,0U},{~0U,183U,190U},{~0U,184U,185U},
	{351U,0U,0U},{~0U,186U,189U},{~0U,187U,188U},{33U,0U,0U},{510U,0U,0U},{107U,0U,0U},{149U,0U,0U},{~0U,192U,245U},
	{~0U,193U,210U},{~0U,194U,195U},{196U,0U,0U},{~0U,196U,203U},{~0U,197U,202U},{~0U,198U,199U},{350U,0U,0U},{~0U,200U,201U},
	{11U,0U,0U},{35U,0U,0U},{345U,0U,0U},{~0U,204U,209U},{~0U,205U,208U},{~0U,206U,207U},{407U,0U,0U},{384U,0U,0U},
	{84U,0U,0U},{338U,0U,0U},{~0U,211U,244U},{~0U,212U,235U},{~0U,213U,234U},{~0U,214U,233U},{~0U,215U,228U},{~0U,216U,217U},
	{74U,0U,0U},{~0U,218U,227U},{~0U,219U,226U},{~0U,220U,225U},{~0U,221U,224U},{~0U,222U,223U},{14U,0U,0U},{508U,0U,0U},
	{503U,0U,0U},{9U,0U,0U},{488U,0U,0U},{51U,0U,0U},{~0U,229U,230U},{403U,0U,0U},{~0U,231U,232U},{67U,0U,0U},
	{415U,0U,0U},{71U,0U,0U},{148U,0U,0U},{~0U,236U,237U},{161U,0U,0U},{~0U,238U,243U},{~0U,239U,240U},{60U,0U,0U},
	{~0U,241U,242U},{505U,0U,0U},{53U,0U,0U},{363U,0U,0U},{303U,0U,0U},{287U,0U,0U},{~0U,247U,248U},{275U,0U,0U},
	{~0U,249U,288U},{~0U,250U,287U},{~0U,251U,266U},{~0U,252U,255U},{~0U,253U,254U},{318U,0U,0U},{186U,0U,0U},{~0U,256U,265U},
	{~0U,257U,258U},{165U,0U,0U},{~0U,259U,264U},{~0U,260U,263U},{~0U,261U,262U},{40U,0U,0U},{34U,0U,0U},{29U,0U,0U},
	{371U,0U,0U},{179U,0U,0U},{~0U,267U,268U},{198U,0U,0U},{~0U,269U,274U},{~0U,270U,273U},{~0U,271U,272U},{98U,0U,0U},
	{66U,0U,0U},{169U,0U,0U},{~0U,275U,276U},{357U,0U,0U},{~0U,277U,286U},{~0U,278U,279U},{410U,0U,0U},{~0U,280U,281U},
	{458U,0U,0U},{~0U,282U,285U},{~0U,283U,284U},{461U,0U,0U},{504U,0U,0U},{493U,0U,0U},{409U,0U,0U},{219U,0U,0U},
	{~0U,289U,310U},{~0U,290U,297U},{~0U,291U,294U},{~0U,292U,293U},{176U,0U,0U},{327U,0U,0U},{~0U,295U,296U},{324U,0U,0U},
	{333U,0U,0U},{~0U,298U,299U},{305U,0U,0U},{~0U,300U,309U},{~0U,301U,306U},{~0U,302U,303U},{73U,0U,0U},{~0U,304U,305U},
	{376U,0U,0U},{21U,0U,0U},{~0U,307U,308U},{412U,0U,0U},{129U,0U,0U},{315U,0U,0U},{217U,0U,0U},{~0U,312U,313U},
	{246U,0U,0U},{266U,0U,0U},{261U,0U,0U},{~0U,316U,317U},{251U,0U,0U},{~0U,318U,965U},{~0U,319U,714U},{~0U,320U,451U},
	{~0U,321U,450U},{~0U,322U,387U},{~0U,323U,386U},{~0U,324U,385U},{~0U,325U,326U},{234U,0U,0U},{~0U,327U,364U},{~0U,328U,363U},
	{~0U,329U,346U},{~0U,330U,345U},{~0U,331U,336U},{~0U,332U,333U},{360U,0U,0U},{~0U,334U,335U},{106U,0U,0U},{427U,0U,0U},
	{~0U,337U,344U},{~0U,338U,341U},{~0U,339U,340U},{491U,0U,0U},{435U,0U,0U},{~0U,342U,343U},{430U,0U,0U},{463U,0U,0U},
	{364U,0U,0U},{199U,0U,0U},{~0U,347U,348U},{309U,0U,0U},{~0U,349U,360U},{~0U,350U,359U},{~0U,351U,354U},{~0U,352U,353U},
	{443U,0U,0U},{383U,0U,0U},{~0U,355U,356U},{472U,0U,0U},{~0U,357U,358U},{473U,0U,0U},{474U,0U,0U},{166U,0U,0U},
	{~0U,361U,362U},{146U,0U,0U},{344U,0U,0U},{290U,0U,0U},{~0U,365U,378U},{~0U,366U,377U},{~0U,367U,376U},{~0U,368U,369U},
	{177U,0U,0U},{~0U,370U,375U},{~0U,371U,372U},{368U,0U,0U},{~0U,373U,374U},{385U,0U,0U},{58U,0U,0U},{160U,0U,0U},
	{193U,0U,0U},{209U,0U,0U},{~0U,379U,384U},{~0U,380U,383U},{~0U,381U,382U},{330U,0U,0U},{332U,0U,0U},{197U,0U,0U},
	{208U,0U,0U},{239U,0U,0U},{243U,0U,0U},{~0U,388U,449U},{~0U,389U,390U},{272U,0U,0U},{~0U,391U,418U},{~0U,392U,393U},
	{228U,0U,0U},{~0U,394U,417U},{~0U,395U,396U},{297U,0U,0U},{~0U,397U,416U},{~0U,398U,411U},{~0U,399U,402U},{~0U,400U,401U},
	{90U,0U,0U},{103U,0U,0U},{~0U,403U,408U},{~0U,404U,407U},{~0U,405U,406U},{64U,0U,0U},{25U,0U,0U},{46U,0U,0U},
	{~0U,409U,410U},{28U,0U,0U},{495U,0U,0U},{~0U,412U,415U},{~0U,413U,414U},{94U,0U,0U},{41U,0U,0U},{127U,0U,0U},
	{306U,0U,0U},{218U,0U,0U},{~0U,419U,448U},{~0U,420U,447U},{~0U,421U,422U},{298U,0U,0U},{~0U,423U,446U},{~0U,424U,435U},
	{~0U,425U,426U},{140U,0U,0U},{~0U,427U,434U},{~0U,428U,433U},{~0U,429U,432U},{~0U,430U,431U},{490U,0U,0U},{464U,0U,0U},
	{70U,0U,0U},{54U,0U,0U},{72U,0U,0U},{~0U,436U,445U},{~0U,437U,438U},{87U,0U,0U},{~0U,439U,440U},{0U,0U,0U},
	{~0U,441U,442U},{425U,0U,0U},{~0U,443U,444U},{470U,0U,0U},{484U,0U,0U},{102U,0U,0U},{312U,0U,0U},{291U,0U,0U},
	{281U,0U,0U},{269U,0U,0U},{264U,0U,0U},{~0U,452U,711U},{~0U,453U,630U},{~0U,454U,519U},{~0U,455U,518U},{~0U,456U,517U},
	{~0U,457U,476U},{~0U,458U,475U},{~0U,459U,462U},{~0U,460U,461U},{308U,0U,0U},{191U,0U,0U},{~0U,463U,470U},{~0U,464U,465U},
	{335U,0U,0U},{~0U,466U,469U},{~0U,467U,468U},{452U,0U,0U},{122U,0U,0U},{352U,0U,0U},{~0U,471U,472U},{329U,0U,0U},
	{~0U,473U,474U},{379U,0U,0U},{119U,0U,0U},{288U,0U,0U},{~0U,477U,478U},{216U,0U,0U},{~0U,479U,508U},{~0U,480U,493U},
	{~0U,481U,490U},{~0U,482U,485U},{~0U,483U,484U},{413U,0U,0U},{24U,0U,0U},{~0U,486U,487U},{137U,0U,0U},{~0U,488U,489U},
	{382U,0U,0U},{4U,0U,0U},{~0U,491U,492U},{339U,0U,0U},{159U,0U,0U},{~0U,494U,505U},{~0U,495U,498U},{~0U,496U,497U},
	{97U,0U,0U},{120U,0U,0U},{~0U,499U,504U},{~0U,500U,503U},{~0U,501U,502U},{5U,0U,0U},{30U,0U,0U},{492U,0U,0U},
	{486U,0U,0U},{~0U,506U,507U},{138U,0U,0U},{395U,0U,0U},{~0U,509U,516U},{~0U,510U,511U},{336U,0U,0U},{~0U,512U,515U},
	{~0U,513U,514U},{112U,0U,0U},{429U,0U,0U},{145U,0U,0U},{192U,0U,0U},{276U,0U,0U},{273U,0U,0U},{~0U,520U,627U},
	{~0U,521U,554U},{~0U,522U,553U},{~0U,523U,552U},{~0U,524U,551U},{~0U,525U,544U},{~0U,526U,537U},{~0U,527U,536U},{~0U,528U,529U},
	{375U,0U,0U},{~0U,530U,531U},{68U,0U,0U},{~0U,532U,533U},{446U,0U,0U},{~0U,534U,535U},{476U,0U,0U},{31U,0U,0U},
	{131U,0U,0U},{~0U,538U,543U},{~0U,539U,542U},{~0U,540U,541U},{460U,0U,0U},{36U,0U,0U},{142U,0U,0U},{354U,0U,0U},
	{~0U,545U,546U},{155U,0U,0U},{~0U,547U,550U},{~0U,548U,549U},{393U,0U,0U},{42U,0U,0U},{141U,0U,0U},{206U,0U,0U},
	{215U,0U,0U},{226U,0U,0U},{~0U,555U,556U},{227U,0U,0U},{~0U,557U,598U},{~0U,558U,571U},{~0U,559U,570U},{~0U,560U,569U},
	{~0U,561U,562U},{170U,0U,0U},{~0U,563U,564U},{356U,0U,0U},{~0U,565U,568U},{~0U,566U,567U},{481U,0U,0U},{449U,0U,0U},
	{1U,0U,0U},{316U,0U,0U},{182U,0U,0U},{~0U,572U,581U},{~0U,573U,574U},{157U,0U,0U},{~0U,575U,580U},{~0U,576U,577U},
	{388U,0U,0U},{~0U,578U,579U},{477U,0U,0U},{455U,0U,0U},{125U,0U,0U},{~0U,582U,591U},{~0U,583U,588U},{~0U,584U,587U},
	{~0U,585U,586U},{437U,0U,0U},{414U,0U,0U},{374U,0U,0U},{~0U,589U,590U},{366U,0U,0U},{367U,0U,0U},{~0U,592U,593U},
	{421U,0U,0U},{~0U,594U,595U},{108U,0U,0U},{~0U,596U,597U},{6U,0U,0U},{13U,0U,0U},{~0U,599U,618U},{~0U,600U,601U},
	{195U,0U,0U},{~0U,602U,603U},{185U,0U,0U},{~0U,604U,605U},{124U,0U,0U},{~0U,606U,607U},{362U,0U,0U},{~0U,608U,617U},
	{~0U,609U,616U},{~0U,610U,615U},{~0U,611U,614U},{~0U,612U,613U},{8U,0U,0U},{499U,0U,0U},{512U,0U,0U},{483U,0U,0U},
	{466U,0U,0U},{440U,0U,0U},{~0U,619U,620U},{319U,0U,0U},{~0U,621U,622U},{340U,0U,0U},{~0U,623U,624U},{156U,0U,0U},
	{~0U,625U,626U},{100U,0U,0U},{88U,0U,0U},{~0U,628U,629U},{277U,0U,0U},{233U,0U,0U},{~0U,631U,710U},{~0U,632U,633U},
	{238U,0U,0U},{~0U,634U,709U},{~0U,635U,708U},{~0U,636U,667U},{~0U,637U,640U},{~0U,638U,639U},{320U,0U,0U},{326U,0U,0U},
	{~0U,641U,654U},{~0U,642U,643U},{163U,0U,0U},{~0U,644U,649U},{~0U,645U,648U},{~0U,646U,647U},{511U,0U,0U},{478U,0U,0U},
	{445U,0U,0U},{~0U,650U,653U},{~0U,651U,652U},{417U,0U,0U},{509U,0U,0U},{391U,0U,0U},{~0U,655U,662U},{~0U,656U,657U},
	{133U,0U,0U},{~0U,658U,661U},{~0U,659U,660U},{469U,0U,0U},{468U,0U,0U},{359U,0U,0U},{~0U,663U,664U},{147U,0U,0U},
	{~0U,665U,666U},{126U,0U,0U},{80U,0U,0U},{~0U,668U,687U},{~0U,669U,674U},{~0U,670U,673U},{~0U,671U,672U},{162U,0U,0U},
	{378U,0U,0U},{174U,0U,0U},{~0U,675U,676U},{164U,0U,0U},{~0U,677U,684U},{~0U,678U,681U},{~0U,679U,680U},{444U,0U,0U},
	{416U,0U,0U},{~0U,682U,683U},{38U,0U,0U},{39U,0U,0U},{~0U,685U,686U},{399U,0U,0U},{123U,0U,0U},{~0U,688U,699U},
	{~0U,689U,696U},{~0U,690U,691U},{349U,0U,0U},{~0U,692U,693U},{115U,0U,0U},{~0U,694U,695U},{59U,0U,0U},{457U,0U,0U},
	{~0U,697U,698U},{134U,0U,0U},{347U,0U,0U},{~0U,700U,707U},{~0U,701U,702U},{151U,0U,0U},{~0U,703U,706U},{~0U,704U,705U},
	{433U,0U,0U},{404U,0U,0U},{62U,0U,0U},{337U,0U,0U},{283U,0U,0U},{232U,0U,0U},{242U,0U,0U},{~0U,712U,713U},
	{245U,0U,0U},{267U,0U,0U},{~0U,715U,964U},{~0U,716U,717U},{247U,0U,0U},{~0U,718U,829U},{~0U,719U,828U},{~0U,720U,779U},
	{~0U,721U,776U},{~0U,722U,723U},{225U,0U,0U},{~0U,724U,747U},{~0U,725U,746U},{~0U,726U,727U},{314U,0U,0U},{~0U,728U,737U},
	{~0U,729U,736U},{~0U,730U,735U},{~0U,731U,732U},{37U,0U,0U},{~0U,733U,734U},{487U,0U,0U},{15U,0U,0U},{434U,0U,0U},
	{113U,0U,0U},{~0U,738U,743U},{~0U,739U,740U},{116U,0U,0U},{~0U,741U,742U},{451U,0U,0U},{419U,0U,0U},{~0U,744U,745U},
	{420U,0U,0U},{428U,0U,0U},{300U,0U,0U},{~0U,748U,775U},{~0U,749U,750U},{188U,0U,0U},{~0U,751U,760U},{~0U,752U,753U},
	{389U,0U,0U},{~0U,754U,755U},{396U,0U,0U},{~0U,756U,757U},{456U,0U,0U},{~0U,758U,759U},{507U,0U,0U},{482U,0U,0U},
	{~0U,761U,764U},{~0U,762U,763U},{432U,0U,0U},{454U,0U,0U},{~0U,765U,774U},{~0U,766U,767U},{69U,0U,0U},{~0U,768U,769U},
	{3U,0U,0U},{~0U,770U,773U},{~0U,771U,772U},{7U,0U,0U},{18U,0U,0U},{496U,0U,0U},{96U,0U,0U},{205U,0U,0U},
	{~0U,777U,778U},{282U,0U,0U},{224U,0U,0U},{~0U,780U,781U},{231U,0U,0U},{~0U,782U,801U},{~0U,783U,800U},{~0U,784U,785U},
	{207U,0U,0U},{~0U,786U,789U},{~0U,787U,788U},{173U,0U,0U},{154U,0U,0U},{~0U,790U,799U},{~0U,791U,796U},{~0U,792U,795U},
	{~0U,793U,794U},{424U,0U,0U},{47U,0U,0U},{105U,0U,0U},{~0U,797U,798U},{398U,0U,0U},{377U,0U,0U},{325U,0U,0U},
	{292U,0U,0U},{~0U,802U,825U},{~0U,803U,824U},{~0U,804U,817U},{~0U,805U,816U},{~0U,806U,809U},{~0U,807U,808U},{104U,0U,0U},
	{402U,0U,0U},{~0U,810U,811U},{386U,0U,0U},{~0U,812U,815U},{~0U,813U,814U},{448U,0U,0U},{442U,0U,0U},{27U,0U,0U},
	{167U,0U,0U},{~0U,818U,819U},{175U,0U,0U},{~0U,820U,823U},{~0U,821U,822U},{50U,0U,0U},{361U,0U,0U},{130U,0U,0U},
	{194U,0U,0U},{~0U,826U,827U},{202U,0U,0U},{299U,0U,0U},{270U,0U,0U},{~0U,830U,881U},{~0U,831U,832U},{237U,0U,0U},
	{~0U,833U,880U},{~0U,834U,879U},{~0U,835U,878U},{~0U,836U,857U},{~0U,837U,842U},{~0U,838U,839U},{178U,0U,0U},{~0U,840U,841U},
	{346U,0U,0U},{397U,0U,0U},{~0U,843U,846U},{~0U,844U,845U},{401U,0U,0U},{365U,0U,0U},{~0U,847U,856U},{~0U,848U,855U},
	{~0U,849U,850U},{48U,0U,0U},{~0U,851U,852U},{22U,0U,0U},{~0U,853U,854U},{494U,0U,0U},{501U,0U,0U},{497U,0U,0U},
	{343U,0U,0U},{~0U,858U,865U},{~0U,859U,860U},{353U,0U,0U},{~0U,861U,864U},{~0U,862U,863U},{450U,0U,0U},{49U,0U,0U},
	{355U,0U,0U},{~0U,866U,875U},{~0U,867U,874U},{~0U,868U,869U},{20U,0U,0U},{~0U,870U,873U},{~0U,871U,872U},{56U,0U,0U},
	{462U,0U,0U},{465U,0U,0U},{143U,0U,0U},{~0U,876U,877U},{99U,0U,0U},{86U,0U,0U},{293U,0U,0U},{284U,0U,0U},
	{230U,0U,0U},{~0U,882U,963U},{~0U,883U,918U},{~0U,884U,917U},{~0U,885U,914U},{~0U,886U,887U},{301U,0U,0U},{~0U,888U,905U},
	{~0U,889U,900U},{~0U,890U,899U},{~0U,891U,892U},{132U,0U,0U},{~0U,893U,896U},{~0U,894U,895U},{485U,0U,0U},{479U,0U,0U},
	{~0U,897U,898U},{32U,0U,0U},{498U,0U,0U},{408U,0U,0U},{~0U,901U,902U},{109U,0U,0U},{~0U,903U,904U},{358U,0U,0U},
	{52U,0U,0U},{~0U,906U,913U},{~0U,907U,908U},{114U,0U,0U},{~0U,909U,912U},{~0U,910U,911U},{438U,0U,0U},{489U,0U,0U},
	{369U,0U,0U},{180U,0U,0U},{~0U,915U,916U},{201U,0U,0U},{204U,0U,0U},{223U,0U,0U},{~0U,919U,922U},{~0U,920U,921U},
	{212U,0U,0U},{210U,0U,0U},{~0U,923U,934U},{~0U,924U,931U},{~0U,925U,930U},{~0U,926U,927U},{323U,0U,0U},{~0U,928U,929U},
	{91U,0U,0U},{431U,0U,0U},{181U,0U,0U},{~0U,932U,933U},{317U,0U,0U},{321U,0U,0U},{~0U,935U,936U},{203U,0U,0U},
	{~0U,937U,962U},{~0U,938U,953U},{~0U,939U,948U},{~0U,940U,947U},{~0U,941U,942U},{63U,0U,0U},{~0U,943U,944U},{17U,0U,0U},
	{~0U,945U,946U},{480U,0U,0U},{459U,0U,0U},{92U,0U,0U},{~0U,949U,950U},{26U,0U,0U},{~0U,951U,952U},{12U,0U,0U},
	{467U,0U,0U},{~0U,954U,961U},{~0U,955U,960U},{~0U,956U,959U},{~0U,957U,958U},{418U,0U,0U},{502U,0U,0U},{65U,0U,0U},
	{372U,0U,0U},{83U,0U,0U},{313U,0U,0U},{274U,0U,0U},{263U,0U,0U},{~0U,966U,967U},{262U,0U,0U},{~0U,968U,1021U},
	{~0U,969U,970U},{265U,0U,0U},{~0U,971U,1020U},{~0U,972U,973U},{241U,0U,0U},{~0U,974U,975U},{236U,0U,0U},{~0U,976U,1019U},
	{~0U,977U,978U},{221U,0U,0U},{~0U,979U,982U},{~0U,980U,981U},{304U,0U,0U},{302U,0U,0U},{~0U,983U,998U},{~0U,984U,997U},
	{~0U,985U,986U},{158U,0U,0U},{~0U,987U,996U},{~0U,988U,991U},{~0U,989U,990U},{387U,0U,0U},{390U,0U,0U},{~0U,992U,995U},
	{~0U,993U,994U},{453U,0U,0U},{436U,0U,0U},{411U,0U,0U},{121U,0U,0U},{307U,0U,0U},{~0U,999U,1006U},{~0U,1000U,1001U},
	{183U,0U,0U},{~0U,1002U,1003U},{381U,0U,0U},{~0U,1004U,1005U},{93U,0U,0U},{128U,0U,0U},{~0U,1007U,1016U},{~0U,1008U,1009U},
	{82U,0U,0U},{~0U,1010U,1015U},{~0U,1011U,1014U},{~0U,1012U,1013U},{16U,0U,0U},{441U,0U,0U},{471U,0U,0U},{79U,0U,0U},
	{~0U,1017U,1018U},{153U,0U,0U},{111U,0U,0U},{279U,0U,0U},{268U,0U,0U},{248U,0U,0U},{252U,0U,0U},{259U,0U,0U},
	{253U,0U,0U},{254U,0U,0U},{257U,0U,0U}
	};

}

/***************************************
Methods of class IntraFrameDecompressor:
***************************************/

IntraFrameDecompressor::IntraFrameDecompressor(IO::File& sFile)
	:file(&sFile),
	 decoder(sFile,intraFrameDecompressorTree)
	{
	}

namespace {

inline Pixel predictPaeth(Pixel a,Pixel b,Pixel c) // Predicts a pixel value based on three neighbors using Alan W. Paeth's PNG filter
	{
	/* Calculate the predictor coefficient: */
	int p=int(a)+int(b)-int(c);
	
	/* Return the neighbor value that is closest to the predictor coefficient: */
	Pixel result=a;
	int d=abs(p-int(a));
	
	int db=abs(p-int(b));
	if(d>db)
		{
		result=b;
		d=db;
		}
	
	int dc=abs(p-int(c));
	if(d>dc)
		{
		result=c;
		d=dc;
		}
	
	return result;
	}

}

void IntraFrameDecompressor::decompressFrame(unsigned int width,unsigned int height,Pixel* pixels)
	{
	Pixel* pPtr=pixels;
	ptrdiff_t stride(width);
	
	Pixel pred;
	
	/* Decompress the first grid row: */
	Pixel* rowEnd=pPtr+stride;
	
	/* Read the first pixel as-is: */
	*pPtr=Pixel(decoder.readBits(numPixelBits));
	
	/* Decode the rest of the first row's pixels: */
	for(++pPtr;pPtr!=rowEnd;++pPtr)
		{
		/* Predict the current grid value: */
		pred=pPtr[-1];
		
		/* Decode the prediction error: */
		*pPtr=pred+decode();
		}
	
	/* Decompress the remaining rows: */
	for(unsigned int y=1;y<height;y+=2)
		{
		/* Process the odd row right-to-left: */
		pPtr+=stride-1;
		rowEnd=pPtr-stride;
		
		/* Predict the current grid value: */
		pred=pPtr[-stride];
		
		/* Decode the prediction error: */
		*pPtr=pred+decode();
		
		/* Process the row's remaining pixels: */
		for(--pPtr;pPtr!=rowEnd;--pPtr)
			{
			/* Predict the current grid value: */
			pred=predictPaeth(pPtr[1],pPtr[-stride],pPtr[-stride+1]);
			
			/* Decode the prediction error: */
			*pPtr=pred+decode();
			}
		
		/* Bail out early if the grid's height is even: */
		if(y+1>=height)
			break;
		
		/* Process the even row left-to-right: */
		pPtr+=stride+1;
		rowEnd=pPtr+stride;
		
		/* Predict the current grid value: */
		pred=pPtr[-stride];
		
		/* Decode the prediction error: */
		*pPtr=pred+decode();
		
		/* Process the row's remaining pixels: */
		for(++pPtr;pPtr!=rowEnd;++pPtr)
			{
			/* Predict the current grid value: */
			pred=predictPaeth(pPtr[-1],pPtr[-stride],pPtr[-stride-1]);
			
			/* Decode the prediction error: */
			*pPtr=pred+decode();
			}
		}
	
	/* Flush the decoder: */
	decoder.flush();
	}
