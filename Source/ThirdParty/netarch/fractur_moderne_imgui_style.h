#include "imgui.h"

// Load with io.Fonts->AddFontFromMemoryCompressedBase85TTF(moderne_fraktur_compressed_data_base85, 18.0f);
// File: 'moderne-fraktur.ttf' (47784 bytes)
// Exported using binary_to_compressed_c.exe -base85 "moderne-fraktur.ttf" moderne_fraktur
static const char moderne_fraktur_compressed_data_base85[40545+1] =
    "7])#######1vF6Y'/###W),##2(V$#Q6>##2IJf=JMY(+h6)=-'OE/1v]n42L<^mqp,>>#aA^01NNV=B/X=te36YY#NLE/1$0XGHX6P6#nI:;$o&-p/$Jk7DvY`.fF(m<-4sJw5Z/0%J"
    "dl(jg?RUV$GdCfUW?K0FRaq985<wCW.=n6'E-d<B9PmFi/eHo[P6Z&#TT$=(7nV8$)*m<-b=Ps/<_[FHUf/B0beuH2I?uu#Q2JuBZNT*RP*El]x^e>1eJ[^I#,?V#e=&M^^2jq/+>00F"
    "2>dAL?)m<-B#Jb/1$S+Hd8BM5$[62_<W+41FeFVCI6;Z[Mc=f_b?:@-^YlS.(LY(+EYjfL1<(>c>Yp^oWh);Q`G(C&4m.SeFrw],7@GF%`d8##%/5##cRO,M'.9/LSxefLFBPD3eox+#"
    "l,TS%su1G`I-DP8/:Cig=X:^#Z,>F%_om92*(:B#>,>>#CnuHM>0SY-Y^W1#fRm5A9@n8%I3TQ&Y&;k'jow-)$c^F*4UD`+DH+#-T;h;.e.NT/uw4n0/kq02?^WI3OP>c4`C%&6p6b>7"
    "**HW8:s.q9Jfk3;ZXQL<kK8f=%?u(?52[A@E%BZAu0upL/,K9Cjs3RDc,1DE'DxT%sC`LF<(JfG+EP/#g6'##:NiAJmVSZK'J:tL[gJcMCt84OSguLP'E2JQp:tcR*.Z&T:w@?UJj'XV"
    "Z]dqWmJvnXw0c1Z1$IJ[0<)##I;#E^Y.`^_jwEw`$k,:boU=7c@>+PdP1hie:v19#M=G##-UL).K'niLuB64(8[/2'f73MK2MGi^%[7`jg;f=uxp]]=0h`rQvKI`jDT9v-e_bJ2lxCv-"
    "_uP:v@Ykq2g4AcV6f(J_*'4]ktrUq2Oa1r2;L)d*2(WA5e*6K1imH(sHP$Vm)OmFi%76fhuh9igJH*##J9)ig[iP1TdD&##`;Gr5VS9u=mc(W]Kn5eP?VgCO>E%IMJPbJMRL$##]G_/#"
    "Tgb.#<M.U.Xst.#L9<Z5HB+.#UTF.#le0'#[nl+#;5`$#YDG##J#KKMmDf9&^^%/10Lto%)d7Q#pj68_ctv<_)@/M_/Yuu#r%O3_qPp?-6*rw8cX_pL)w8/1VBrP0aMjv5d7dP9B1:Q9"
    "J&0Z?#gKDO&`vI-Sf&=BbI<p.I#Pn*@f/f?#Ug5#f?_qLPW&D?94;^?I7;9.vV%d5g5a>$M%l63*WQDOaV3B--/ImL7IMX-LwIb.)6T/1Bqc2$CZ8/=FNF$9tvTs8/h'Z-O8&sa=s&sa"
    "RbqY?DX4?-3RQxL34kpD@hAN-.w=K.+]i'#'GQ5fZi)`#THUn#*h1$#1Y^T7D_Rw#jn%j0n2=1)uiWI)*)TF4e:=<%?DXI)?]d8/:;b=IV55',V5#b+p%J1(cZ+]#)A&A,W9ED+82&P'"
    "t7oL(Il###JaB#vi$V?#;ne%#qmaV$_8eh2'b@F4=_Jw#:/5J*i&l]#[+]]4+W6C#`3a_=+:Q(+vh5c*[40f)GgLQ&,'taEebg+*M?`k'b8v7&;4^V,B4LJ)0GT#,NhI%#YYd$vsGV0$"
    "Y[O.#Z)1/#(sU2'4kpfL>mGQ']im$$l%8&4ULD8.kmqc)OpmT`=wmeMFmL1.)^]iLjHo8%21,,M.MJw#r.'T/^uMq/+N%d)W)aF3ENjq-&(;b7$p]E[f9OA#):Q8nBQ.J39Zti0Lra0("
    "%Pb.)fsBU%4)fT0#>Qb+WWa)+7K0;.8V<N1tF.@-9MKB+N^o&,kR]`=oP,=7G>%@-_7te)VK0:%?hoi'n>v3'V6MEQ)m4e)V[Rs$m8Zj'&3,Z*'ON4B;Yg:%(>oh(R<Z-3<,fdF<YXf2"
    "CQXZ6]IuI):7m_+I7ui(1t(41tg(s/CT-S9U;,m'=68a+PoE^4o')02[4.<$]rnh(ZX.W$]A_1NvBmpB`pWI3#F?v?s[d22WZViKRir0#XK9D3f()p%s<,)*R088%e&###r2=1)(3U/1"
    "c`[D*fait6oYhM'i?oT%H,]]4/CB%4mUnS%o=Xe)d-S<$wpRx,PcA'+m6^3'eDvn&dM%1(,5wn&X-][#8&iW/e[Y_+>52p/9ZG]uwbV5*`NeK)PCf0(a,HR&hx<.)'j?R&1$..MPJ6I#"
    "kv_Z#e$M$#S$(,)#@uD#Hi<<%Q9OA#fBFCP]v6Z(6@Ep%Y-+t$]oHtHg(]fL.GI7&?3uJ(&5ao#IxE<%eOUs-=tP;d7w%T&DO5-MNrH(#g[8a#u7>##oC@Z73xPC+o%Sl'h8Q3'wI(w,"
    "QQXA#[v4A#A/###*A^Y,a?x+2_:<^M=*wYP[)eh28Ej;7sHr8.^(.m/2$s8.>GVD3HN:&4=Q/J3P4LO+o<>W4O[T34g&6:(,Ik-$Y<=B6UWjfL,>1fDkntRGL8=*MRx*EGV*&X#<YjfL"
    "VWO&#PmT#v=cxt#kIY##S)1/##;w0#hT)979c0m8+w#?$FbYp34wm)4lYWI)$5WF3A^WI3*lOF3)T=o$VTrHM0fXI)9a2#7P=%w-?]d8/w<7f3ZI(*45,7C#vBo8%I5aO'.N;_,&pLh1"
    "a;<E4^weP/vA,T.oo/f)<fW^-atk05j0Vt6c6.1MCV@C#.(<e3?A3;?$GY3LG[ET%V7^&=vm_S0g54_5&Zm7&$k06&J*06&=bsP&BN_,)SWSx,Enj5&Kd+p81o,-34/`B#Up:K(fWFX$"
    "Mac3'h,mN'@),J*)/]P'MTYj'Q*'U%%s_$'I8^A,dUtJ2q*n@-kHl`#PA<Z%)[T>.-_,70N3>3'D^c40-JI_#K5N&+,&-W.:mk^,?cn60YIVS%N7Vn'kLg.21XsZu^JsL)imFx#Y;MO'"
    "nITb*gSRh(=^T;.@Xx1KBf^AtJW4;H)>PYuCQ^q%JP-%-/R3$#ZQ9D3l9G)*1gWV-JE[i9Bn#&+@wX&#$B>##uim_#oI>b#Oc<n0TZqK2e.i?#b*SWAFCXF3vu.&4(#u/M:J[]4Op1,)"
    "]CI8%2=YcM:cR_#=$+mLsVl]#+%G,Md)``3v'D.3qYVO'jK+U%]Dmp0KU)a4ovfx,8#VI;B+0>.xGd&4eSL(,uNiu3v&U.287a?#`bWx-f<v2(@o.Cu<?%],#lKJ)EQIg)[=2?#L,#3'"
    "-ta6/2rqw,0x'j(7<RM/=Idc*e#GJ4[7w;7aZ+Z6ti.1(gkfa#4hU7[d/2f+I6Y]-crj@.Ju-f*F%N13s:mt7C>^@/P]hI_l2[L2wS?ZA928p0a_f(#XSZ$vP^[[#7wJ*#7B+.#82M6&"
    "tRZD<=@%W$FA1j0T.pK2e5ABHq.])*as$d)$@?6s-xI`,sq-x6`1d59@G1a4mZd)GmS:u$+kRP/nPg*%fB7f3<91#$CH:a#YM#<-RUfZ$j.ef+d6E`+VQ+:.AqJ@#a=nn9.;.n9-IZE="
    "<$qB-sp`N(c@p>,?.qB+@Vbe)116G*@x:Z#^1XI)kP@h(mw7[#hXb[$nE8N0s%O.)))-k&<2,2Co.=I)(<_#.Yg>A#p.';.r_'g2G&Ml0;mGp8m&]b4#Av:A$01v-j?)E4/%$-3509;."
    "<$Nd*bxSb*gjTq%;c(Z#6cvB,QsFf_YCto&MqUN(pIH>#bm=]uT-j.3Fs$p,DRQG*Fn#M;6uk+D/bI;$$'@>,F;(<-2mf(%8.KD*PCI8%f*H87gN[>#f7o1(Od$H)j5`,)SCi?#gYe)*"
    "K*tp%`K]S%s)FSRj>m3'aF,c*e]a**F_9O1tgcN'kVrk'NOlN'>`O_.BY&*#e=8F-ape/&&mWI)]]WI)71T*#0)%&4hD.&4q`l-$[dEpfl^1o0BgF^,K$lf(h$i@,:t`D+p&4M&VZ0qB"
    "kcXKE_U9:/T3Bq%for4'rAmnMQx1UDTGgB+o]Q;7r$aooXrLK<?4uC.aTQl1OlQtCdCpV60Adp%Ba1e*j9FT%bRMM0%7FeH14w=lprR-)Q$9U;V@`R;o=k.)I'b9%eBuJO2?:L=3?Oh)"
    "Flb_.).nK#'IJF-IMB0/7F>L2<iM^6mGPA#v10[%L`JD*Yf=Z,0$.L,?M>c43OqhLMT;E4xE6T/A.ikLC*(V%_lRP/+gb6%L00q%SQ53'N^1k']io#,g:O.)c5`0(c)?R&@S=w59P9f)"
    "P%2v#Bmd)*d?bp%MOns$L/Ih(Xw.@#lo%_4v>;k'q&LU%K<gQ&+sh6&<LMh)(5oL(Y&D0(a`EI))gMB4)E%1(Rk<T%C$g2'o''Q&V#m3'ZDRh(]<ow#:r:Z#W8+h)?JLE52u6s$,63P("
    "d;cA#;t+j'a2;O'K?Tm&#iqjt.@WV$Xx:/1hl_$'cHF:.__OF39:k=.22(>o2krom<3O8?*^pPMZU'JLv,QfLA_no%rr:$#])@L2G9Kw#5%lo7W4N%$eb,,MdP@h(%DNW%s_gb*c#Z3'"
    "l&-lLe>V0(6Q=B,'mMO'l-g60cA.L(8sEr0qW_R#Npx=#Krgo.E%>B#<t%XuFdpG.,1c1g2>F#$25+^#DId<%A@[6<@Rb3=/Hj-6Mu?xkoV#>lbD+<%h8>###;Qv%YwH>#$#7B3nRv=l"
    "$J(Z#_,2>5vbM5&g[/,)V*Kn3`[`298VT/)Ec;Y$JUrd4w3TE#nG(g4;59]?29uf4</t@?%V:&5t%VC,1`&,3'x3A-Qw#F:1E2V8S>kT9;N-^6YdNaueX$^A2+OtKf6duB,DnsK5PF'#"
    "Ig#V#F@&=#__=W%_@Iw#:Jq_,#b:hLIRR@%^7o]4=Sx_45vvC#2mD9/[2-s.SH(K(1VK'+2#W4'UDR-)hr<I)&*%Grfj^6&xaIX-sHV7[.Z<.3#;K'++,sk'lD24'OIxQ0f=%LG,jE+r"
    "LZRfh(3&##lt9a#,1CT&/JQs-/rSfL+@LP8TvY6UbZ/[#C7.[#mx5x6MF-q9]JR#%hnEC#&/Us-L:N2'96mn0v8fK)QPsw.f2F-*Ls?d)R'OT%wH/[#TDwd)HD%m00wSR/0L2&5$JV;$"
    "1w*B,o8Ne3$74&-K?tx#-nV)+jVMk'T*nC+r-At7ce_+*nGoB5Bvnp0Df[214'=#-e&;4'F:2v#rgqk1p;***-HGo/o$vD4S@uI)J>:D?b)5',4FkO:(j<f2uK'6&bm4A#A),##5'sW#"
    "L1b9#pGBT7Vg#j'D*_P'2`^F*A,4o4e5]+4>W8f3*^B.*_w.[#-`h[,uuPA#c15gL)KH*&R)h5/MFw(<n4PjLYO^h7x;eP1EPlY#uf2B45lrM0SIoC#=[:3(1r^972xl)4[VL_,[)VK("
    "g/v3'Ng(k'1-s`+hrds.c?TD,W91E#OYW:.A`;:86.iw,Spu01b0b9%KW#V%hY)P0p.hw6A%=A-P@dY#R$->/%:7,*42j0(f<9Q&`ArK(k5dn&g-p60WJP98_]2:8m%$##XYd$v'NI[#"
    "JS?(#Y4QU7kbb:&^p0^#mn^3(*=49#`:SC#>r_v#Hq_F*_)UHmgv)E*kTd)GY;Ag6%C9a#<U/[#a+Fb3g48C#qps6/p<_V&BC[s$(.g#I_3b>-XoTi)U&`K(j.t**dL1@,$Nm3'%=;%,"
    "_LuPpABHe#6$XT%p#$A40o>v,X3o],1RMJ)W3vM(f8Mk'T6Q-3^;;B#VVRL(K%MZ#)[<D<.5t**m`mC#M.QS)5),##F08t#Z_T:#(%(,)]4i?#dl(F8WIp;.b[uo7etp;.D7Dj9lW,^X"
    "TjZuYT+%s$a3lr/f;bH*^CR%->hhJ)c+Oe)[Bb9%QH^6&aCJm0ja3v6?6Gu-M:s?-F*CQ&5Tpo8=a7H)cF=R:FEjYu(r)s%iI'G3mwt0;NFD[,2Ycv,`B,n&l:Tb*i;)O'e*Qq&On-b>"
    "hVV]7:6;[cLJ&B+[eL/)9Y###,Q.)vgu=u#2xv(#aMKT7+Yac)-.pK2P><j1o19f3Jc7C#.`Hm8=FhKc`t9a#[`JD*?DXI)6[GL%f;>x6CtOX(LjXv-EUY.qH.iV$=N+l9eMHD66q5i;"
    "Zh[e43./V8t60B-MPsa*3eZ0)tjBm&h)ZN'n$lRe;axa+P`g+4:O:/)bOp'+#`1@,Ltl@5kM52<V=K%--@W`589uU.g-8`?_AF.)sn4M(2il$,U39q%daS69**,*?;9<G3m),##:wiW#"
    ">XT0/`,>>#V-AnL]Qo8%c/%E35UlQW6T^A#+2pb4B4r?#MZ-H)<OT&$bdGg1EBuD#MPsD#(fPn$il7l'sgh02kC2,+gF''+%gPS/Gs=F+RbkJ2Xv#@-6p.F#T@>I)nxw-)l^KU%0:'S0"
    "Q1]?$cKdB+0?TV.x;a*4us+V/fZBq%2F&r8<KBnLZ58A.tl7L(hJ24'&Ao1(/8S70%5F*#nXYi9mltY#GK)<.OgWf:/9n-O=DPd$l0'30x?7f3I*8M)dSjHZC`bq0.s_`3qd&],&m>9%"
    "vt#,D7x4J2LAdT83v^@6j]T*+VRqR1W_Nc+T2=5(#n.a+RKRg)83NH*=:>2(42Lu7hC*]-HeuD4t$BY.MxH*/N<jA5WP.21RdwC>_%k/2Ww#.<:mpW7?@mD4V<ow)DRqJ)N.8Q0Dd'$-"
    "Fx*N1p5SUA?6tU.VQ8A,qsxx,=1%##eL&&v,EG)M&<W$#@&V&M<:Uv-Qw*T.eJoY,fUpw%AY958M(d,*1XwG;X4P)4v^t;f`HwK#Pk(504c(d4[Zb^,)D+d3`>$)64epL;YbRe4/iDu7"
    "w<t&-LV8'+93<1)qc)o&GgUg(g8;O'H3TQ&_/-7&T%mh0r;TlB_qU.,IXIY#On8MD-3DRLiQfY6?Zs+=jfM9/6^&j<uu.l'x`Dk'c<BQ&[D@-)is5R&kie-)*$KI3*+c1g,O%v#34no%"
    "lS(Z#x*a8.&%k%$qSd/1a_H1<EF/f=CY4i<LQL@-xKL@-RLL@-gML@-,0bV$[Y6X1W,*eFU&7tLi;<7:$C:B#tC/f=/<Pp)XX-##OUcb#I'x%#r@9T7*>G##p:SC#x3kX$2=fS%$5'Jh"
    "$h1PfBgBac9Dk+M&jX%X'#axXI]t(#L_G)4:0fX-&OXAu'j9B#%MU%[](;YPwRcGMm834'>wl)4Mr@PJ)l`0>GQ.Pfon=bc?8rKlq7huPvvZY,S_$##%RU&$,<An$9_Jw#loH`,Um'u$"
    "XZTM'L^0#$6HUV%NNNfq0i_;$Pe*A-3-P89T6%+4=ATP+]MYx6$hsq.m>mn&fo*.)XgcN'Wg%[Y=7Aq/-'(11wXXF35>SZ$.Q]&,-NR&>LQpL*/%C7/h,Zn&l1XI)TT>3',LrJW'Q+-$"
    "SK^2#uH`,#A9x8Bmo598;IPRChd/[#BS7C#h$<8.J'%m8t;Ev6F5Xe-E>Xd=uIJJ/g#1-2T0,@7X]H8LM74rEFi(`RgiW:<l5,?:&uag4Lgc7<Q&l13Yfub+kHoW$pENk)#H^r.AwWb5"
    "ahgg2DhZMV_7.`,SMZ40b/vO0)[C_,i=S%7[0v0;(Z1F>#dL+4).cR0lOm8;K%sm23Ul?8TQ(#.J=st.t='=-oREk:jEaa4&Pqo.76@u8q=ap/b_T60YN9D3/3f%kqU72'U?.DE.H@W&"
    "7wsI=)8K29>(H##Z`'nf`:SC#a3FA#?]d8/MAf#$bfcLMXtsO'1fWF3kWj[,)OT&$C;XQ'@HMk+Heh0,^=E_&2>.Ks)C_F*Q.i?#[9Mk+s5AS'%kRP/P7n8%i^&c$8,H`a%:Pj1Bonr%"
    "vEF^0,ww^5pbj9/8)Ib<<Zj*4Px=$,V'pm&<Y:o(H'bT%UaF=$<@gp&VjJ&,fW;U:S?GA-`qA8%u-UJ)csqG))JFi(8l_V$Bud<.ibbN1e>HPJ;ipcG#/fv5.E&bumbW;9/+&`,M>NN;"
    "/@Y6&098f3&uwo%']pl8IIET%k;q-2tS<'.nWwg))Yc_+1o.V8t)Bm2NIW88KR`J)xfIL(4:<p%vH;D+eKpM'W#gS88?uu#5C%]kT1<SnEFPfCQ:ro%lNrc#&;pK2Pdp_,eL0+*iapi'"
    "w-@x6rM5<.]CI8%&*k;-6t(M%ZntD#^un$$^qMp7#ig-6BMt1<+.(f))Kd]4R31V%bBC8.Gq[P/lfhV$btrZ#1Rjv#DjVE*+'(9/H?e,*mT[=-JegK2g.cq0&;O</-3Ja+6q8W$k?Jw#"
    "J@&u.Ya&a+4k*N3&GZb+slcB,0w3Y-<iaj15,1<7Dc$=-2&6T:n$HH30%*^6xxIM1m`l+`9wdg);mP=$j&T;.QjqY.4N?h(l2Dg($qsU.[AmO0vw[f*/vT.*OG/#,/g^V.Wk4r/>sYS/"
    "lw[%-7.'Q0?PujL)kD1)4']A,`?`E4vL&Q00BpC/Z8$;fPQO9B&%Gr/Yh'k1r%c(5e,>>#6L@xkho&DsZEb$':W'<-N*Xt>)A`#7ll8f3xE[F4Yk=V/@GUv-q1)t-o6$,M@`'`$c-g;Q"
    "#/'2Bbg%F4gaN)+fh<5&o]/#,?*Na4(Pr:8JRe=-SrCG+:+[u6snpT&s#1u$lYev.t^tj=+Lwg3Ank#HXvon;v;9w6qHb]#f`:</1)7@7WATI*R:KP'</sS0blP++7xfU9<0xA,$[RE+"
    "lWc01TCes$CUNTL)MxgH`-Z.<D3V/<;2NI?KWxO2X[R'Mv%x7$P:H;#vGBT7.]hV$_,eh2M8&n$vtd;-dm1P-hk$2&;;gF4X`A,&;;Rv$cx.^=N<x/NF_#`4Xvp<1OI=JLgr.l'-5Wo&"
    "r+XE*cU@s$$]$9.x4f.2P>;p/dNaN9m66a,_ZqU/&m8X/SIIb+MT*)+GT9Q2VL:=/IeOVEh=3g4B3eK)s<vc*S[^cMam)I3wSAJ34aRH)$*pR/LhNQ/*@%g)aGCV.;xn@-OPv=$&r29;"
    "ub5q0Mc@i1X0bu-k^%#6Y6R7:.0NB-Wb&`,,*::%kP<e).['99wC*T%4rco[&MTcD+9%<&9d9F$W#Ma#en^3(O0pK2c4n8%_Grc)*Od]O'`?<.4T,T%6U^:/l+]]4<Px_4a=@8%S;Ua4"
    "RtC.3a(m<-qUtv%q%?)483CT%5Hji0_tOJ(KR5d3eP2B#;-&*+(b.F>X.$42U*=]#M)Te*c`IQ:l%.%-F$YJ(oi/B+sZBx7_O4W%eQ'U%XZ:W/+^S0)xOXI)?JNn:A+q(4Oa#S/vNr&5"
    "xkti(=0*,D;=Cc3./rV.uf6W.H*BN0><J&,=ZZ?.Yg*-*+W1#$rNl31ea#r%:RW5&8mVY5Z0c=$Zo$w-m1oP'DxRQ0`WTq%P'pq/bbE]-[hP2(w&930?OD)+P.?>#P5G`<fWXrmSF75&"
    "etc+#DW>d$7crx%rv8S%Li_s-GR[LMo2K8%'ZRD*/CTB=w$Ka>_CNC=R56TK2Nx?&h5u.UdYrO'(o'+*fvsH*LDP++6FUO;,sxJ3&8mt7w7vE5+1:j(tvHh2q`wh(WkJU/LrQ[-Q$Dj("
    "4GjB4Q0Ik(q]7P'hfn-)-0Hi;Z*FF5Fj,s.cB,v.N*/@#KI&SBl2Vc3[`>J*DZh,)6c'f)awfq/(6v;/ux#F+0%Is$7%%W$O,N61#&N4'u%NS&'2pF4*gKd>N:_c3F-bU.d@pC4Dt$d3"
    "`@pp&u4Jl'4n/v79ocK3S]AK<1lM>7O#JH)g+0J).+c:0T/5##:1dr#ISA=#UZ%-#UVm$$xH:a#ae_F*bdGg1p:Rv$^.]-F<w9a#p-ZQ'_%/cPBGUv-@(?Z,ZKOg$*^tv6.U1f)a6,T%"
    "6=gF4ECI>#,_041;*OY-GD:C,%LB;.]gG40hSvH?&@9ULfk/Z7gQV41g$aC+r^27';-+:.&<QV%AmOB,^ZIK)_hQ9.816G*414A-@e;71b@s(=Eb?v6w?VJ)5@7t-^-?.)mZgQ&cSEIP"
    "wm&?Ctgjv6<k*N)NJ$h*h8b[?Fh)N(;3T307nJ7/8?pR/I.m?,K0gQ&(Q9>*dVM11Z6_-5r#Y(5WH9D3T3Icir:5DEuKG^$Qt+p%,N#,2hY[]42*rw.0RYQ'fL0+*@6+<-dhF<%r+]]4"
    "%UnF4PdGg1&[sU8'=Qv$sXEU2(T.d)0AEj$9Vqs7lS1g_L`FU*_a,8/'I3:.JRD%,2T]&,Y0?*=>FIt-KtM@,sPe-)cl@P'$6TR/O9g@#+'CL0>-9R/9uBV0-ve12`dsH*M0-h2;V9b*"
    "&c(@,QfslA=H3Z6;3Ms/u'(2Bu8xH))a0i)mb@@-4TO#-cn,H3Ulsk'9R7X-f;;4'P8*E*F>wI2v%(t@^.lB0IXhk1BtPL2Y4W50NVHf*]@?,4h*VJ)->'b*[b>2(BOHK38_x),9BAM9"
    "f6'?dhsco@13av63lX8/1INm8retA#UR0:)V<#e;ACxI*[YWI)x@^Y,]im$$vBo8%t]DD3mU5hL/(Kk$(*p:ml79F*=LeX-=EJe*/NAG*;7ue)rgiI3':He<7'S^5Wc,?,l6ec*fp&&,"
    "aHng)[?@g)%=9m&1anL2(UEo(Cm'C,[-Q%5YG=E4C<hn/4=dD4Vd&A,]23d,YG@i1xTx/1?,4i(mbY(+OM],=E;XQ1cjT#$cOn@-pu#F+noVo&?ZMO1<[8:1)#^^+.*_t8<p)4'uvO]#"
    "5&MR/I5YY#RG(A=8%Wrm]$UiB.N[s&b)=E$p%,B$f]SA$XXNp.x,Tv-GCl#%7_tD#)JIb%^'D.3Xwr1)Nn%68?QY8/VO%:.q`[D*`CST%5=gF4S,m]#(`/E##s3e)s.Qt7f5Mx7:IkO:"
    "T8G>#]1+P0x0Zd3YM)p/Q#pE+N?'>$E2lB,Nq:j(JkqJ)F6x9.m]%m0sPK*+p82O'gehF4P#)Z-NnHg)qJdn&9x*O4<Eo'5&nR#6`]2q8/Xk5&@KtH;>kqo0kvi*4fxn.22R;rZTvjd*"
    "^%M(,7@lR:L)(_,['uj1?]l;-#$Ou-+*$;/]6Lq/nFIj)4Fm$,SP###WYd$viE7[#&%(/#qQI##;(NY$v+]]4FC6u6`gLK2*%O[$S65S952.@%ueXI)YS?m/0I6u6_T^:/%:OG%P+uo7"
    "N@Z/=ZoN5$tB:a#_Z%[,3C(N(.^Tv-$GOI)b)?r%d'os$t/Rh25v2ZHg4x?.QekT/nOYi(:0Ja+m.+M(I+_F*1u=M(:<WH*Zh>m'CJ_$-c[M;$tLBF*Ohws$RT#R&V^,n&o$Qc*%_PM;"
    "Q]ko76wV@,H(LO;gx0+*eija*4n/$7n=:R1&ZboA>UqH@dT=3=1VVU8euhc+-oB+*):6,*nWYv.E:#i(:kD)+64PM(*&>xL0nIO2)`GC+WN5n&g(0F*W$+t$`(Dn<.Lo<$GY_0#WYd$v"
    "6NNh#`*Dc>c39v-JC6mAK#d'?2sLD3vu.&4>t],5YEo8%LJfcP;QWB%9]]I)R'k:mF:MZ#x$)@,YW?708WlY-#$sE4+pa.);(X,<WueS/n6N%,Vu5b+_3GE,V<M/)qVhV.UTbU.co<f2"
    "2,o-)ILAC#/Ew8%q#XY-lE7#6eCBG3Tl73:Jr*T/gb`H49gf],i9vD4DHJ+44uk.)d2fb4V7Fg;11vI))Ed3'*Gfh(K[Lj(0h5?.6QR7$mA&=#`NKT7RF(29bEo8.r:Rv$XHX<-4^8c$"
    "+Hrc)@=@8%HVZ#otr1#$dngV6h+lv6[QZP.[*'u$pfTg2;2QA#k*JY-U%1O+jDGq;N3+l9Y32H*9O]s$,R*W-^s9RLNX.@#>CtY-hd9uA96A&,#d?7&c$<@,0)/1(I8;L1r:m>#r<6G*"
    "L;W0(sb5C+LqDd*m?^5:[OWP&Oc%m01X%E+>Rb[$34?(+<Pu(5^MoH)Gn2%,u9W6/79v;/jOl$,C'Tk0Q)v3'e_*A-f9]<$Lp/I*tX@[-,UrP24=h+*OH3x,SmtE?]msD+-op'+%GBJ)"
    "02al0R/qV.1UM)+W?G3NR_ErA;2Oi()x5(+34PU&LCrv#+Kq`IQKAu6_,x_>v*4G>Kgb&+2aYs.<evg)q_QJGDm^f3-OW%,o9ix5UKPk0jKT),1ZbL)it4A#^MJJ3T&BO(`]I)*pfc#-"
    "T@qb*#qI:7M(i[,`$@q$5lWon<p&##[F:P%ENt;-cdXw&0aVO'ZcD<%4IFG<-$vQ:9*0k0^`WI)jfpw65IF<%bNf8.k@S_#u(&aj#F/[#$%Bx>Qjw517Btu-6=-K)CrfH3Bwj:.Zoub+"
    ":FjP&nocm<;O6(+H4#%5KRs?#/cnM1wso@#VemT.0W*Q1DEs)+B]JL(u<Bq%90E;7K#@;%<C0K2g*>r/<^QB7[.s:8ie/u.0it.)%V#$,_UH_5YXlM(U.^$50Z)iD<@ex6`=8<()L5n&"
    "9wF/Er,rV8eM[v.q$?d3D>N4'uom`6]c90).Y9'+3j$m0/PNK;R`K#,KnKK2hqSu.NUTg2$),##;7mr#Y.###xY^T7%qC($p4O$%d=@8%_IR8%[YWI)OQe,M$.m<-Tk0k$,M`2:CJ3j1"
    ".F1[%-4$gLvbZx6i?rkLDrke#kOFM(q^I/=hhM`+2t4R<lVul2Q$hF6t4Qb+nIap/2l0/)L-4A#PRA:/O^<H*r%V?-xc-d5XF_a>2O-t9Kk;D+aD&X/XdGn&=S>##AY1v#*]cg3Nn6D>"
    ",Ae(5UHN4;,$^0)TbZk%Fh1O18Fhj(J3s[,VrxL(jI&m0pW<H*DV^x?0&P*Gsse$?Q4e;J%))c+6]L;$>%v##XYd$v&R]t#RXs)#GkQU&tYj#/BH`2%3tFA#R&*E*u$D.3AS<F3/awC>"
    "q34Q'(hA79do>mSfY8f3JF%;QD'PX.3/wo/TIU+*F6rN(2c4M($r'j(vHE@,suNE*UB0q%Cwj9%CF8xG.qQ?.U96n0uD/d30l([,e[X]'LX^fL,D].2IE/b4PI:J)7U.],.;jP0BQBV."
    "3OD)+N;64Wde1@,bjcN'7.nS%NK#2Km'UdF*Td`6%v`[$KrXU0:x-]6T^(Z1.mv&#f]0)2:r4u#EI_/#6(pD*=KJs7WD5s.0j=p7%flS/9;h<-]cW/%+['H2#7Yx[#/1h0?]d8/MweP/"
    "UE=1:$*Wh)sTFg1#V]C4.<K?-JK^>eQ?n68ZN%+427A=-V3k6)G[`?#Oi%60q)vg1QjCg(Hil>#^mHH)w`b78EkiH*>k7m2q[)>7+L_DlXjW78[2$%-A<8e*SCD@#ar=;-1M(t:1dH01"
    "X.eS%1i+m'R0R(+qFGT'6]Kx?(GuV-YXr$#XYd$v#RI[#8ut.#GagT7;/+i<.K0j0fujh2*8m/:ipB#$e*B30Y5MG)#0Tv-(7xH+f]ID*/[Es-q`>lLOt#`4-jm$$?cS&$^o1T.bXm5/"
    "'ZUW$TL`JqJ>bLCW>gj2uDZ7&@OdC+-qw=-kOYC+/j6r%@-[K)Fh_J)htHX-N*]x,PP<I)iSi0(K6pm&peP(+rk,G*t$BL;;1R_%S4c?,@FeU.Q:)?#S.P?,ObUf)&p%l'$>?A6b;G>#"
    "`pv:&t$ZC+v6xV8.9vu>kZic37=v%5-I@],1.D@,.$4:.?=?C+MX`%,'i^F*=+Z$,J>'/3/=M:Am:GM(m&::%Y#v3'pi%1(bi3.)kHf<$rGjD7^,Vw*uVge*xM4M(@LhJ)l(@-)HGrl0"
    "4p9s7UEH-3@?7E5ZX2@,k,K*+ki`O'hNNdX?;bs#po@-#47.[#LhjT.S:n=7kX'`$)ZVO'T2vAQ%V7C#T6$NskD6*&<;Rv$r_(p$*cYQU+*8x,TECh5k?T`#ql.&,<T(L1_R^H<lgKR/"
    ">3fY6a$M&5JpXB,j`-7C>'6C-cnP2(rjN>-[&_U%HPxY,.5d^,@;K#IU%Pf2L)?p8UVw.28$j)+^&/F4d*^>.ctbn06kq4fP_e8%_1TF*1J@-5TgU>.,PB'+H>M508@`-4`T(O'*M:%6"
    "1a6V%'xL%,fA(m9dDe3'h%34Dg3'(6;Jso&?;MP0&K,t72QmR&KH1,)ZO?M9=-A9]m&HS@r<,B$3GW4:4$xP'q<u&;)k2w$:WPA#',m]#D^Kp.9U(02tTC9r#v<.+QavH&+=Fv6Ptuh<"
    "HjUR#IpG,*wBuQ'(C@x,u1/p&g[m5/2U+X$WAr0(ix&F*/tj6/L6g@#9e)h)D_7[#^xm40kL6(+oGCM:/t_U/3X.K)8K#A#Cgm`*x8;0(b_'G3LijM*`&H@7g=BB-64PM()KG40V'BQ&"
    "KMdA#3x$W$[.(W-7xjP0><Uk3A-:U%AhY-3#4r<-_ue.2/D]M1bC)?#C7VZ#w^i%,1TX^,`R?>#9h<uldkY%t1/qo%4X'^#::pK2rr<*e_en,E5aEv6-^wKF6KSq$JioD3-jm$$a=]:/"
    "l(=V/:/5J*Zw.[#2>>vcMV,u-foiWB&u.%-i59*+0t3Y-8Pw215@@x,i&DA$9E+>-`*7K)39D41Je]2'<rj9)xd5R&543p%PYX]B-,DB#cJsi2Tet/267?x6brYKP.Wis/7k570+rKJ)"
    "9<=#-_'md3[n:j(NN*bNR:#[7dk?K)r:r_,S6HH3L5v8/7T?O*+F3Y-hR:GD*#Jh(_7jf+6&YCj>-e1<fbfq/J,>>#0lG(jYKD`s5ref(HjQ_#xQAR/oWK>$Zkq)1DsMG)#:&0aLYUs$"
    "^B7f3RiA,&knn8%s`[D*$#?W-w^?b#:w#,MT9:o0V*$-3lJ'I*NK6r/wtLJ)Qg,na(2xI)v>UU%5$s`+#FR=-ZOEx-3P%$$q'Bq/5,T7;HhMD+j%A1(JIV?#6iLi<%vwi1'T#P9+2E7B"
    "Art+3)#sl0E'J],'.?,*;?X>-MGiP0EWBV.5e.a+JIH^?xdSG.Rlem02^PP9jjTE5WIjq'7B'7//HY1:K^oQsxZO]kMMr.L,B@W&f?B##u]Y5&-H0j0TZqK2uU=j1NkFc)%N#68+:+Pr"
    "Ar@i$X$D.34))T.OM>c4&%Jf$8d'<-DMwA-5UV=-fh1p$TAYxks::%6nks=.kX[n9<sJj2ikJU/CjUd20(pq/+t`)+p:pd)7e@A,WohZ-,T#o/6bOs8M2;m9,Sv]6,ZU2:%5&Q0DX$0)"
    "Hrjo&'kZd3//C)5DeeA,'GJP'&Kg7/uX1`+Tn>H3R+Fj1[=T4;/1OPC9VOf2Yc?x6H=qF*%2FM(Lm4$6Rl&5'M$dkKYx7e4?aMg(:Y8l'm/Ab4Pi<W%4*0R/bj`K2+CRB.&/(.*659f3"
    "U4MGHX@m)4t2cx#.;ph25r`D5Re@=-jVdx@6(:H<6P?>#F2I.qOu](s2&:8%[F,+#`:SC#$8wM0x'XD#qciO'*gWjLkCX$%)qf8.#%%]6/Z-N005%&4$2f]4dbcRALe(T/.+s;pDm-O$"
    "c<S@#M&B88m79B+pSnD*vPg;.Q,+M(6EX^+`H.K)#5+i(tr@P'L=pl'TJt_52>E^4sG7H)3ruv,h6X9%%(TKOF')E44Ate)$5f1(DY<j1Rrl>#woPU&O7;Z#*ufq/mmwk(j9D0a.AZ',"
    "4KJa+#s@1(vCcv,_QR0)R0Hl1#KrO'ufVT/9,4E*fjg6&SH@ha-l.@-13A&,(vrO'x4Pv,i#4L)ES>>#Y1<V?]gl1pgUb7D[IFB@+?633$),##jJXe#fGZ;#5NKT7J19MBR`--b_6>V/"
    "0)+$%&*YA#32-W-?&i0E'F$a$c]d8/GWRl1dC)69dPx->$H;f<YV`GYB8(=4ULD8.OPcY#SS4R0TaOB,w4b+3=I7X-A/8L(0?jd*Pfu#-MF6o'^)@H)Bd%F*grS#,^<:k18Ue*50xI79"
    "bd7S'.1QC+<b?,*<U[9.vE830t@k.)VAEa*@UXA-%%d61i>(g4`$ol/tN#1(B65N'vs87/mc%M1$#T>,n.AP'+O&[#6GY>#.@xe(:7C/)6N8*+0jif<g,m@7YTaD+SkSm&h]RL(=b`w,"
    ":fY$,wLJ5'i)H3'd;EA+.@Kh;V78SBX0^@#x),##o=5F#/(-;#HW=n%*dm$$E+tD#l&g;-Pj>g$1X_F*t'Cb*:ni@,=A^#,P&ih1OWWh)#E22U==@W$YSQ_#Z)-v$2>##uJ>[s$bVo]%"
    "8S.G;W&H/(NTgi',+bt$]+6##,N#,2,3=1)9dE.3+2pb4i@+w$ax;9/6`p>,Enn8%]R=F3Fh2u$OEA.*_Jio&u*V`+Cvxa+uu>>-#_Gf/<Y%i1M906&mTTc5,[OAuF.a9Dl?Nf:Zu(>l"
    "Jq*M*qV::%kJnL(Pv)E*E?u'&SUUa4PF*A-ES$W&xW(O1bnk31SlWu+=JgxtoA5n&Lfu>#s;05S4rJ,M'8+`#mYd_#RJ-.MWeA%#QO/N-q:k$%'V]D*[1f]4wMFaX.n7],)[<U..w*^,"
    "V9Pj'g=l;-uvboR3E+#-o<sm/p=9f)8VL;$W2Id)?a.kXT&q:HIA$JU&,###>H7g)WqpUZ3KhgM/Ig%#;r>3#rLKT70f_;$ux];6Xk=V/^q)HD>*cL;M5'##lwf&v%>[S#DBO&#bsIg9"
    "E=DH*Z-8M).M<dt`i<'.rqB_4uL>c4/-Gg1C0>g+n9bp%R*Q6'(C))+cJ2o&k9.x,r%8T&LTC1p_Nwj<i^Xt$r,/m(9cYg3(#Xs/lIB+*@K^R/^@Dv#iDZ7&n;%L(q&G6^6.<w6+[^d)"
    ">IV-QV@<uljA2MpI6%29_U#B#)D6hGA(?Z,'&.I2+l1T.dt587j[iE/w<7f3RWWjL-4E/4T.@x6-FGI)b_2Q/vVvc2X?hc)Y&pA,WEgQ&kIgF*f;<&+Y[fq/25xh(8+431Mu1Z#*(c%5"
    "*WjE*Fju016wXP&^SAH#D8M11AvTN&BNAE+_)2k'V?vZ%%W1_,Dbl+*-XV%,Ub7w#$:Z(+ch(X6VK'(6l3^2'WWE78o6v`4Ojv=5?SBJ3swQ6%u2_U%$->>#gWq^fX^JuuER#(fhEc(-"
    ")cn$$-a?T%'Po$$l13T)oj53'6<cZQr-oo/]KPj'qoj&+bv*`+?JGuuuJni1eet,<_FpW?2:V8.1d'*+sZ.O(emZH3@.Pi(_,2>5L>]6^9Sll/g5ll&-t;A+)7H42O5>L:#UiH*LE1K("
    "%&m]#:S4U0$V@C#OCTvGvxn]4n`WI)9Ogk1?kCs3M&`l0#)=.)J+o60KpbR8Ocnr%:?T_6*(dc*n5uX$hp&I*1gt#-U01PoorTE+BC_J)I*06&%C?E1;Ilm',bZL2j@d#@%`T5'me#c*"
    "%+L/),)4K)HV932BA^K)To5##ejhP9khg_P,%<5&P*88%+%x%#@#gb4p-_fLJOGx#ULVs->Y&E#RWWjLJQCx$/sYk#sf6;%`Gik'VS7wG:Yao&x<9,)oc&'+NaR/3LGba*>viC*xgin'"
    "36=Y-Ai,cAps)*42]Lf#>fAL(9VcV-(#u?6A$It9L),##1$;G##_)<#Fr,U7f@9F*V'b=$wVYg$[EkXA>GUv-`r/f)qiw^$rp@+4iB7f3]Uxc?eq;f4:6TQ&d:ke)mRt.)h?Eu-s/?8/"
    "?V.D,K6TQ&:$+>-@VlY#w?@s$[:G_+3(nI4)O=AuV3CP2Eetj1e^0U%01l2(lL>M9xFt7[^/VY5*)7?.AbtD#pDeQ:[qZ1M*0NT/P7n8%gZGQ/h=S_#HJJ]$t4*T%-Luc2qim$$97Ee+"
    "1*(:%`mVR'H;W0()nex,/P7Z$]N96&YR.W$8hxi'kxJ'+ImT7/O#[R&a#(^uNhY(+,</a+hYJ5'Rne+2x-=O:$hkF5MGa'41pJf3mIXi(7u.?$4fKd2&:)`+I=wS%pZjU.aY.21,K#DY"
    "A>q',L<wG*_l@1(>5,q.2x?>#Bgb%=iE&>ll:q>e)D+E*Z$4Q/QVvc2b`kMBTRBpR*Lj?#'.@x6r.NG)[;G^,U3'q%mX,c*%F]J1=Y/40)`Gc*.1c2(kp]R/&c7V8=TrN;:RF[$25M^#"
    "C-r[,2s%1(`c*I)k_SQ0?\?BW1Fgk%,W9cj'xD)O'dO_R1K(#7:SUo*F<K7'G]HN#5Mbf3:O39%#KaB#vj-rZ#n$M$#@GBT7/Puu#Z,eh2Uo%&4SlB;0dfRD*pW:D*jl3P'/:2@,EbnW$"
    "E6id)i$?p0Kqsq.-ruZ,lh;ZuTr>`+jWZ`*m0-`s0p88.ee>kOn.:gM:YJw#:/5J*a2gHMa5<i-%u7jOr`-j'2lG?,2,/l'Us1k'tn5(+b+Bf)qU&j-:4*WUaGwr)sJBI*o7KF*%^%L("
    "dcwH)ih]Q0GJ?H+`3S9`T9f&,0t3#-MG###HT0#ve?j'Milxv78UK/)B$PN0c`[D*n.=V/GPD.;flj/2E+]]4eoCa4-WD<%'O9T%T[fr.qv#R&TK^6&$)s4'#uFe)llwH)k&Qn&*M?Z&"
    "@r0f)dMik'd2%L(g-ST(-7n9.9+gH)^mV?YpGl]#CRGC+&a7h(eS[H)FoF-Zhe+PAwguN'o4te)lHf/1@X;w*VS4.)`ljE*O5Wa*DnI%)$`qW-@F#I)VNm^Xu(j%,)g=fMPDIV#&_H/9"
    "(PO&#&X;5%>hT^,S^':%DBc0.+N::8uQ9T7j=S_#<Fb/2#eFX$_Y@1(#/&5'')QY'*_[=-)jcA#<Ok312`1Z#ONe=-'kg2't7=I)q<(k1Hp5X1DH.1($ab,1_ws+/'EBe*@wsp%g`q0)"
    "73Nci4iXfCln/E#j$iH-BU$&/+W6C#Dw%.GxM$[$EM0gL&Z1#$s/wL%m0Vt6(;m]#/v%UhXL$1MRfxQ*i&l]#Lbc*4ow^N(]d^U%vl[-)E&h;.#riV$$7m_+UP?<.tnxp&w1]1(T3f[#"
    "E1DZ#;>kI)moEe)8;3b3(F2`+`T':%VS%l's7FI)luwh(S9>/(K3[>6jNbP&Fst>-WN+X$u4^Y,^puj'Y/rK($S'+*&=))+uXG(+l/F^4DcWO'[B'U%R+`Z#&TGo/WmHt%bJik'`j#R&"
    "pf2,)X#vN'$h[],;EOe'Z<KQ&bD`0(Sews$WPHp'oN`%,+cmXlKKAA,$),##q-QV#q%,>#U;0T7Pd$90g,Rl1]Kj?#BUfZ$LF9a#6Hrc)R'Up&XuTI*i&l]#hs`'5r$_N(8$3`+6ph-2"
    "T+aV$o(A1(rM)o&T2%l'DtwW$;>O.)gS[-)4v6e2FK3H*j;Km%W@>6'[gGn&GfSL(&N?r%f#-R&Ot[S%mdju-4+3Bk%tl3'])`K(vdfj0cC0b*W$S[umxL1.)p/kL^5'P#0+V$#]_oV%"
    "cqL,Mr4(:%nOL6(=OXV/ik+:0N9DE4Q`W?8$TQt.tX;?#n`p'F,/Y(5&^Xw?^K:Q&GTaIhrEC`-&S:`5^$?h2f_t)#cv)XCXJt7[$tEM0Zt$##@PYv,P.6J*6QFk2'cn$$iL[5/l?`[,"
    "DW2DNh8=qCT$@g)<V5$6t-S<$`PJJ3$IM%,$b3#-Xgm`*SHgm&Gvc#>gfXM(Jjqg(gEBHLR[Qc_Ch.&,E1mK3m;c'6r*m(+/[wX-jB+X$XYaE*4)Fe)`&F?6-t<<-.56b+r9vQ'c@F_0"
    "k0MG#7%,>#YJqQ9hNmG*L?UZ$&Xt`3]E4J*PIR8%WDcY#0Z42(.%5D4kOxq/td+A#TJIL(5KQV/`^/b*X@/N'$Y9F*Z4)?#Ro>U.7HsA5R5qe*>#?]u1iL>-evY3')bJ[uJdHl1p`V.j"
    "u?V@,MnP-3[),##p-QV#T0,##m>t,'t#j.3HOHM9#bsJ2uK.qTn-8V8]eu>>*e'v'u%S1(w.n7&c]3q)X;R7&K[V?#Gb*9%YYTT.>l#G*wFc;-LJNe)e.5?,1W]Nqj4Ak*;8'#,W6,N'"
    "HTC0(l1tE*ie^L(nD24'$f?>#a=tQNtD^]Frd%##EJ)m&+%S.c#Fi8.2uej:^*O^,Sg'u$d`#d.L^0#$$9Jn$3/3T%ch&I.w_K,3;3Br.paaV7U1Oj1(%HG*5'W%,hafe*&ded)MOOqI"
    "_E3d*5j37'J6EV7-OV]5Kh@=-GlSm*q/Vg(Pwlb*P:BY%o#cB,0Oh/)sGrg(+UwX-K5Cc+Eevc*d%%w-Y49)4Y+ID,0lkE*',9B+K[_=61wnc*L+pP'CSvc*#LEU.X7b)3(x#;#<(?U7"
    "UV/B+]9'>$G(<O:9eG<.]q.[#.0nO(^E>T%EiTN(j))j_BZCl0qAJk(,i9J)$O7x,J>$%-+[#>lH2Wn:7:S[udbZN'[g5n&pCkI)t^S@#J>-REL`;ulp]4Yuis88%DgZ(#r2=1)7(@m8"
    "hskA#P)w.%)W6C#:`Rr8DfTN(h1f0%Td&o8rdOg1qZxJ1+Yke)8+CV0OfJ'+wUT+*xHsm/2-jh)m)Qn&mo[h(O<m/,#+rX-hp&o8,6o*+%Z%1(M+W0(sNaQ/+6<61QBc[$w&%t/v[o-)"
    "oQ]j0ZvS,+`(hJ225`&Y6gBJ1&`Aq^jAGA#/:,L%L`x_4n`WI)W$:B>8VNp@$uKh(%F@X-ZuC80VLIs$kHZnAam>N'mK*>-a/'+*HouY#k^aL)L`Ri1E0HvHn7@<.g)0E+aQ:'-V?dR&"
    "x(-J*0e]98whU_,p++i(@Y>>#2LwXliRfY5$_2hP`15gLh845AV?kKP_+Fb3-][Z7&(^fL#Z1#$Uw`^%sD.E*g^ju-O0>j'd(Ge#*KE8Ar.xH),=&mC+HY]#YPiK(<Yj'43m`S0j=v]5"
    "Oo]l'CxDX.@fYuYl&n-3<`0n1[(gWn(HI12^EHI)xpEu-*Qhp/ACJ60%ELY$^xfB+QT9;.fPI:n7DpF*f.:2)DYn.2k@J/;x<q@>-,4i%O+wXlTSOxtxJ98%Z@#+#r2=1)c9FA#]CI8%"
    "t]DD3ncD<%5ZRD*Sl&*#Qt]U/C=gV6qf$.%i(]]4qLYjL'M)e$0(,E*s6<-*oXt.)L0kX$V=A60?Agb*YY8b*b-&@#pwYC+#Xe],3q7&,tc&^+uG%1(FlaJ2]27/()iFgL>sh2'tK67A"
    ";02%(841j(u`A'+b-7dDp8+p'<blgLJOhb*eOC@,eTB6&PEihLDN##-#h=]#rAdj'v<jv-;,FG#fjB6&_Nlf(*8]1(<a)kkhsl(3<Ys`*m/TS%KD/A$oieh2a#ID*FX+vKwQRP/sMYx6"
    "`RC5%chZ58TglS/m8];6qg`MgoevY#P:tN1w&r,)$G,C+x7e214cl_+)eEO)tFA1(liRL(o48M1*9:U%C'Ot$G`Cv#b70b*Pla8&bQZH3T1d#@xvV1)k]JF4YgWX0CZVj1&8t**a#v3'"
    "JqHC+c<$12KHZY#]6Sf1wRU%k0Gnx4DjLO'&`x_4:03gLua'QADe@U/Z$4Q/w<7f3sL>c4glQxeZ0;-G^MR-)D])X.sh'k1vG[h2.rsf2htX70wRGC+Zht'4mKR%dphbJ2I4?$,(&U%k"
    "vO0+*5BXY-V=Fn0j6dL2E]X+3I<BR/6(1,3hoiL1_:a50RS^HuOArs/7C49/%5F=?#(^,Nb*axkI0Gs%x)Qv$dsUO'`qh[,1bWj0V<@+4^7^V6'weP/K`3T%bTs7[9]/40QcZX7`?n>6"
    "Mn;a4%W<&+ZYbI3srj4'XFJ_#Le2k3oQVj(rC+a*A3r:@+<8k(]$HH3Rdlo9q'=T%rj:80Eb,v.@D>>#*4FiKc5#>lcFT,2/F>L2Jbw9.:kdjL$wxeUON8SW$[kwTB_:-MQP/[#wl,x6"
    "P9`#>$2f]4r=gV6]s?H3ekg5'DUDL3Di$f*ew&>.TgU>.pl8j2pv@0)=gNu+JTe#6H8p^5s3+9%8[*b5+coU/>a>O0f)@g+<El=$6=EU.&uI4CeVAk4;^T]=/rPS.o-g&$ww%j06I&s$"
    "]WD.34KG#Gs;u`4in+T%KU^:/%iH>#sfx>,*9uK1;WQN'kWbt$6H'R/DDak'r&?n&BO%[#r`sp%bv(k'vw1%,&@/30Q^g6&i]Do&Tn*T%$Un`%oh0PJD5#J_=Rno%t.rZ#])@L2r.NT/"
    "Omd)*$DXI)b=m=7eBI>#ArnN'H75U&BWAx,#Qq2'C;9L(E%Ym'$$e;@DL.kCIr>>#:ss.:(-AM^pO=A+>W[h(=tJn/w2H?$U4wK>R`'nf#o4j$b[wP/D:r?#<]E%$SQf;-`:lo$qp@+4"
    "jZU&$GCxT%3H?O&a2)4'D1AQ0b#dn&M.=x>0/Og(`m:k'>v4j).1bo9O`X87W=#<%MqE9%l^f[#l9Vk'C+%s$&Y+M(SgA*+D':lSIPi:%qO'J)FP8p&@mYW8'kN^,3U:_,qt?t-t63-v"
    ".r6-vKt7-vAW=-v)^nKYX_k[$02gTr#fNT/`V6C#gNK-Q2DXI)#R6.Mui=c45++T%4S*X$0Q3x%/2Mt.AI%],>n(F*e8nH)jTBQ&$QBW$&[t,2mYnW$Lf9u$`'U]$R3Nj9e/LY$AJil0"
    "/6%h(+t8t$j2nH)NTxu#:_[=lj%F`N-Lm;%;2]3OBiTp71Fv;%se_,NaDvf(q`fbuf37m/VJ&J=U$>c>,djfLk7I'v#V;s#O0`$#3De/(FI7w#l`sB8.9eh2LAXjL^Aj,)RnMH*k`<^4"
    "DAVrR/,l2,MsNfM3lr^#?Lho/K/LiubIupQT25',mxS4=6Ww%+Kc5G*ionL(IK^q%2`'nf72e*@Ds0@B>pk$B^FSnE0)AF%nj12hlgqD.SU-XRhv12hQR;K>K9,B4Et3xuK^ah#_A%%#"
    "(pm(#qH`,#Smk.#kb[(FAtGc#?nId#JWQ_#L>W`#W(`Z#Yee[#,2^KM_+b$%iqi;-Y^KW$wVVCG<Ddv$]tZ>#p*6s.WD[p.(aCD3G(gc3Y:9k1d?$L2GD(C,6@pP'ISer%4O<6/n_%s$"
    "Hdnh,$ih0MK?7*NB[-g)B*[;%7efh,Ls^$-bX_kL_ThV%rmgCkNtU5.3ISL17-Pt$'nGV/kWr/);UgK2bX:E41&Dp/g67^5p9<:.Wc;BM-t+X$'k5;/0JGjL$RkZQitX)W^DoH#3@_/#"
    ";[ewuqK>F#f*V$#'d1m&kKx+2p:SC#@=@8%eEXA#5%k%$Qiqlgas'Y$J39q%BD668;]]%kB+Vk'LmUK(fZI`+X9wS[%/5##<CDS$,'W<#n,d3#Wr^VQU#rfLmnL)3SBBs$Vt+^G)m]q)"
    "#&K+*>N?C#2^q=-v$)l%U4n8%=9*ni)S@`,P5f,7^uT(42h,^GZ8m296i:`5(jMo&k[=Q'W6/@#vE6,*v[1<7%,'f)^f]#,UCbK;6Y,C+*XWQ/WP.H5hT(41x-&Z7[_M`+9]?L#WL/N0"
    "E)=L+qXXF3-2@8/S.A%-AfT^+x(Pu^&38i)57MW-G9*V&Tix#,ItF/(LQBA=v]3a*RIPm'kSmN'HBR0)MIVZ#PueZ$8oU;$uY<m1^9Kp..>sh('-Ok<2t60)/X%d3P195'VCu$,B),##"
    "4Mf5#Z$$;#E#ZP&f*TM'F6u$./laj1ZYkv69jE0&Ib39%aF<T%Y15(+KLRs$l#:u$gv>N'Rp^:%ewZ49tu9^#TjS49g.Dp/:-oO9RPX#?>?@GDv6E)4O(#k=`3g8POHYQ'h/4j1BZsl%"
    "A3Ys$0Pr2'XJik'Q65/(X'b5&R3tT%d5;k'gGZr%ZPNe)Q?bp%qjwY#>Fes$=IeW$O:2Z#rTeW$C]0u$eYIh(ZJ<I)D%4O44mNI)[)M0(1a$.2;Q?3'/gX8Rb@xfL<bx8%]Xa9/.),##"
    "*,NM-:,NM-Yw')0BF>L2vPl=7'mlh'+TID*PCI8%gob4%LPO['ZCsl&0J`m&vTChLY($j'X$X5&B7@hYm%/h(VUIw#fSnD*'o5m84.Rs$<+r;$WPEe)<mrD'Fl&I)1*l(0SBBq%h/mN'"
    "*1Wi-8>'MYo(c/M&wI(#.xaT#d*^c$Y;WEGl%NO'w>0s$'`n$$.L`k'-a_h1eDMO'bG.L(p@8JLh8)O'*X?O(2RugL5^3O'6),##r#Io#w(5>#^jP]4m>sI3,RGV6sk%Y-m$1S*?>m=N"
    "M@uo74hRf_6)AC-J0o,,N=6##2H6C#l#$;#=@%%#%d1m&97pK2%q@+4oTIg)esSi)*@d;%gJWZ54qJt7)>KB31BE.3QNpf14p`&PId0K@.PG<9>j)K;hO3/(o3Ym/?:)$#0WTI*&,]]4"
    "/Q?C#+QD<%Q,qSgc,Lu$J6K6&Uo3X$J<sV#^Hk_$x?d^X)nE6/#Cx5#YYd$vIfGj#-`($#=6Fr$ZZ#,23nQd3d/H`,K#bv6oT^20b_2Q/qSID*p>D&7Hft/Mu<kAFnctA#ZYK/bpesD#"
    "M9([,`TcN'q18fLG'06&A&Ke*KpZd)$]?=*?;Ta'Kbxa+#NtM&mlRh(G7jA-RDa'4WS*_7fX.H`LxnVBdb1'%>C[s$tc:9.X=bw$5qVv55?x],JBbU.-[R9.@b401%(rK&vYr-,R5YY#"
    ">ADp.R5O`E27YaQ3=4:.Tx)c<aLl)+a0b5S3/m^%+:XnI#q[Y#<q<ulBOrV$PSsx+1W[s&QLR<$<nA2'AF[<$[Dp=#Vf'N2.K=c4aTQl1C4NT/R*8s$Jah,)Jpdw'xDp8%.QEH=NNA.*"
    "e0f8%'3+>-j^8R/?8#U&BQOU._pt^,`cF&5A*%4(#Qbf:IV_D,1V02B7DSH)_xRQ0g2DwKO[@>#c9m.hmEel/p,Si)?q>V/@GUv-Pl$LG$WD<%9Pa_=v(b)<wu6C#B/D4hk3'b<.PuY#"
    "L+`K*`tws$Z`TM)oj53'jlG',[pkA@kv:V.#h8A=Tr?%-+BHR&Ani'/mXr69f8[U9dX+muv1'K42-MfL[GS$vYqSa#hh1$#1/HU7R_rr$4O1#,ap%/0O*bi037bx-]r4G-.lK4.F.:GD"
    "[=;H*WP$`,q0SF/omqc)t$F4:acLa4R-Tv-2dD>2a%NT/=K?C#+M3T/vcMD3i`pMBS*>)4K'-T%Lkc<-5.QC+V^XA#vX8e4u/mx#v@p/(fIkI)T;`h1W/0M)O(NT/p#io0Z1`V$?=rZ#"
    "@)$s..#Jx.I@iV$C2ss$JjS&,o9OT%ZLqW-aEYj'Qkn8%b1t**dW'U%lIOPAYL369X&;Z-8G`o2Fg?H)K)?s.7Aka*/9pR/76)g(Wc/F*_D%P'k^gN:3rcx5@s%h(lIUs-k/9*+D0tX$"
    "cfW&+`+$##D3=&#L>a`#<A%%#`X2V7Vvd`*ROMM0l)LD3OkiV$(SV'#f(#L2]J>a*A.RW-+$Dtqlk=V/w<7f34hX8VSsdX/Pqi?#$`^F*$dPw-j$[(+&/Mv-hwZ(+Uf$+Bw9XT/`4mW0"
    "rr5c*#eeQBv3=9/a@;91sipF*84>fqs6XA-u7Qb+uBt]-s1Hb+xaN@0'Ke8//aL@0@gUN*IU(L5YTA>#Ct@xk?uv3r]h-##r2=1)>=OFF?KZ*3Xawtuo4V`>R>E`Nw*%v#@T$)*vru>#"
    "-JY##9+I8%aR:p$lp9H*k_Y)4vSt]>SY15/tVm>SZ,MfL$?U2:u-);Q@x2>>Tavx+S>[(#g/K%6Pq0^#o:%[#qc<n0*mo0#Ckaa4kCI8%h$D4BMFav69Oo$$9p%^$$Qg;-5+cwR#rXI)"
    "`qi?#Xfr13Jpkv-bNNjL/Ttv6x@T$9;rSfL%kc/]&#wr%>IT,30JYp't.ba*rd9U%:@k/2L&>b+*_`%,(h'U%U8@-)p:B'+&ILP:v#f^5v]UP0SNKr.h,`K(%-pr.OIRW$Kqe0)M(;Z#"
    "d,lx#;is8&S=rV$oo*I)h.P?,%#=E*>svO0*QMK(1ibi(mZLk1KG-w-73,O0ar]b*Y'BQ&Tj10('Kq)3iVi_=-fn`,b;=d*u;bh)L,l>-vX9J)WM8B+e-4T%NFE'0=`9J)vd/r.rB3Y-"
    "1LwX-`21'#Zi)`#?]:Z#K,ST%m735&oi_;$A#,=7Ij::%w.=^4h4vs027ISR%>uWLYN9D3;rw.L(KWV-LW<J:*0`v%Vg,j'?Dn7&2^c5/[:SC#=xdGE$@n'nCW.N%*_,V/L#cW'5_EdX"
    "@=]>.wnQ>#mVj&+7q7a+(qNu-8h-k(mkpJ)J6/&,pQb5&RE3m1mJt?6(<nP1lGt?6SMVE4i*Gr/[EME4Z)[#5@c3L(6$*-*5%,2(r;;g(iRW50Cl<c<:#e=.'?*m1?*>$6*H3m18eUD4"
    "wK?e6ck,)4Kk:p/7AVo%Yi-;HnD2AXep9D31W[s&$ah'#V,6?$>M:;$Z`'nf])@L2#xIw#8EB*48xI`,RZTM')ulW-FW$@'U78C#v]Fk%I=tD#^0..Mu=_]n_cIL(fiD4'1(pt%Jo<X$"
    "PK53'HV%V%mYM4'_N>j':dhd2%E2O'c5,J_?Uns$RLfn9RgPR&O@a&+3p.**KG/^+RE,n&3YJC4PpKZ-v=PZ,kS)S&x'5gLp1BwuQ=0U#f*V$#8Y^T7KWPN'_H5u;vW5H3@=@8%E-1d/"
    ";Mx_4'4_Y,jW9d;a.h3Cv.i0(a_>jB67$Q8QpY@%5T'MWNg10(M0vZ(+ECB#,uC>#,Ug+M2QEYP7efi'oPP>#0MlY#W2U'#*+'u.H16g)ob3C3GC8Yc:.-*3ZQ9D3:ow.Lpw/,)JD->>"
    "9SQP&?Bq'#T583%`9'v-5d[[H/PV=.-<Tv-dEsv67Io$$Aj2+3/oSfLRCOF3+Ov)4J_Aj0LQu[$08WZ.@rZH+]Z4t*j5g],[U-c*kA,#-e[F:I^[VZ#dGD0(Dn&q%>@[8%mcn)*b>EA+"
    "cHs/:dD***A%aj9s2_U%e^4]#3Mt?6BQ.Y/&5Vv-O#O(Xi7'O=etGD4TuCi3Ea=^,TK':%v*m_+O+@8%_A%h(v1c^%45*UITE,n&no2P*0,@n&9),##rnn@#GZr,#04QU78lqV$8ih;$"
    "[e%j0aFw8.stC.3$UMD#E'Ws-e>K.*0Evj'&AWS&LdlP0Rw3]#YIVS%8RI>#'5YY#uQ]%b^mA]k^hHP/3j<T'O.@<$;kE=$S:rV$7`@*F3QT/)(tLk$C/u;-GHr%%lx2N0G]FB,4gPS/"
    "QM'0M<W<-*^s3-*^7.`,9qZC#^12p/D)U?-;8M50C&L?-PpwL,.'T3O)Y.(#Z,q^#?f1$#EW]W7Ut82'2uH8%3FET%4(%W$EtA6&@^f#e[_Me$<-eh2:.OF3)vsI3)`j=.HF3]-TgW7m"
    "8YiS%GNJtuaL6230nak_NGH(#6tbf(dcic).$Y`3.H@W&gE9>$b_xI2[*/j0JP+o$R+lP;s8Kw#Ps$d)()TF4x]DD3e7#`4B-Tv-vJH>8d1K?/@0c40UIP3'U-]W$(Zhu$c6]<$MEgU%"
    ")P/L(Z:YQ'/ug(4a,ed)T;m40RcG>#T<;x-,(5N'.$tU.Zcae)jRUw,]taP&3o$W$el)[$N''q%jf@1(5p58/TOns$f:aM-ASbS4d9n0#UR)V7OeIw#c;uu-kQjM0xwcM97,70u'U2UV"
    ".ZVO'BUKC4;jgO9JUlS/DO]5/dEsv6f?%lLL8gf+Rl3O'OT5n&?:>2(d1Kb*i,7d)C]VaFdbnw--?o&,tbmY#acX?.ng_,)v*mc*GO@@#]=#c*sw-x,-.@p8mQnX-uR5c*$ttg1X[nW$"
    "X),##mN=I>VkV;$]bqp$<2+E*v8$MD^](2TsRo3MS3s#v>P@@#T4<)#kx?O05eQ?,fq+`#F1%&,+SO?gsOdN)g:-=7D]v59q03t_=RX/1gEW@,X&l]#YNU&$aF</1?w6u63@+w$Yd*P("
    "Hm6W%QxOA#KL#e6A^n-)`].l'?l[r%[);0(L.[8%CIiV$7:<5&.:x`,rQ;D+5vaA+.+>Q'7DAP'[Uxx,tdbO0#3)K(mB':%AbdWe@qt8Cik041$#TB+>3b6/Ai>C+;x+@5*12x,_JEA+"
    ":5>##rcWp#t385#BVs)#QpUY7QuAD*P)oZ5sew(#2rpu,8pCV?PBXT%@,)j0HvXD<<rG##x9tglKN#,2DO?p-a&FL#]:SC#8xI`,N&A+421099],E.3^B[(;`?b<Ssa9l:fQtm'W@C8."
    "Q6;hL9nmp-#bM3Vebej:fj%73wmo]#?Jc>#al`4'@U>H3kMQ8L%?'=@hl32:Owm,*i%O.)VG3e),gUh1X8i50-ZV4'V0kp%#%Cj(<PXe#iFKb*%X+=$FR*T%F*5j',]be)KiuY#8%LN("
    "dudb<Yv5R&-aEH6vF[/4;7Is$3+Rs$Q5rK(pxwd)[4e8%Y%]fL4>FrJZeZp.n[pUdR6i/)EblQ'(=vQ1s_5Q'ubOi(^(M(,$),##q:N)#H%###<r,U7xN;`+VC_GD:)pG3P,,Ze&Ui8."
    "<(?Z,k(TF4TV*T%LDn;%`.CS'^#ZN'<_2D+Dg$.2X#AS'hWX9%`bZp.1/rV$wh>C+`Qt5&Dx^'+vA)#5;nFW-4Ob&#o)R0#F@dU7`W+x#Y8w`*p9F5&,N#,2.[qK2]oh0G'ew%FM0Ev6"
    "_]MwK5i^F*ul?X$-?os&E4@s$9@WP&Ue*9%[?BQ&>Ie8%7]c>#N^FD<=[nQ/bhSYc`NP/(+s%302JcY#9ocY#]-#?c&CE$#tQPF#6#ga#-$M$#E.HU7'7-C+/MG##L9)KamWm^8mtV<%"
    "-K?C#rqh[,3s0o%>9N8.@FvV%$4oS%05e.2Fv+>-fkE6/3J0b*0I[X-#_][#J=Ba<Slki(Q#Rd)IwN=$Xcw1(1W>o/'R#v.]d:0(<]xb-Xbfjif^a$#8STT7Ph)$#hs6g<AUT;.Mv'a4"
    "T7B4MF2v(hb&Cu$I-0q%+xS,2ML>k%m7ke)VBLH)GBt=i.2?W-&]+%Rc8ZI%fRu?,TTSq.M)WFrx<G##XBSt#Xk#tLFBv&#gO>+#fx$t7#UnF40^=#--0.^4mW*#-^o*a*^xd[7;aQG2"
    "Jb78%p($p.t#Ks-U,KkL^>Ip0_8in0bS>L28T8f39>+E*8ZVO'-Xvq7.qJ,3L5<b0gfnU/G2QA#.#ko7^+Y(/L)VT%(e'B#cFfI(k06HMCE5n&R)7H)KL[W$I?>N'A7$Kh;?>YutOTq%"
    "6%`v#:to2'/oKp7]aOg1?@IW$TskA#g[fRncoou/4NMa05FKq%>I@[#3(Is$:OI[#Z,3Q1xvaI)]LjG8pM,U;8>958B10a#B5'##gR/&vfQPb#pFX&#PNi,#O4s.;.7,a*QCJZ$iaST."
    "C3%Z?W,rf1Q9F5&1ko5/A^Ip0(.PL8&XS?ps/p<-iCde'o#>T.h:AC#g%I.1WHX9%Wa9U%BF[W$]fQ'5P$`5.f;MF$8O;N9UVZZ$HB^U%L?1Eu`G8f3:#LW-kZus/h:q=uvV-X-Z%Ds-"
    "ZHWj9@p5@-U4M;$6fL;$cEJN0<Kfk9A72N9pt.<$u0K*#7k+/(]uxl8t+TV-JD->>nSAM9NvoF46,1Z-eBK2'@1Rs$/S5;-Mliv>exfu8[ooA$UpD@$pX2Q/9j498/;<TBeI):85G<TB"
    ":mUD3gn<TB#.8V81r1T/wx#l9Y@f,;*aCD3bkC[,NoNL#K0kT%QQcN'@RW5&@eNT%%=CV&i@Pv#)(vv6T3BQ&*Z(P0%)xL(lT4X$?-s]/]4H123_jU.75`^#Fk@D<e1@:..GP>#]GY<-"
    "6PR+';l(j9@Q^]$B5'##[l)%vXq@1$5:r$#6Y^T7VSE&+$*Xr$jkn8%[+KF*]D_5/H.<9/2H:A=Y@Mj9xi5w#F8bw7ZjV@.9//`#iv5O'>Od?,UZE9'tqf<RFA3ZB*fCB+[olY@)egX1"
    "Dw250.@lQ'S,R(#ZQ9D3/3f%k$n42'T'-HES3cc)12Hr'#b%d)[8kd*a/=B.5kvGD]ukOB$7'`30VG&8Cb),WWDGa.f1+E*PDDYAE]+D4lgb)8Y@k#$E_`,;<17<-i,6&%aW9^Z=p@XH"
    "Lej5C=2Jw'2(:/)(1q/)Ee]YKbvOt$>Gcr7-4Qw$4H+#-n9sm/#GeXH4l0-4((D*8cmZYQ.f$&/0SEF3>S)-cg]VoC]o2r$[c*I)DL0O1',gB+_H#N'-Y.9.LEPN'DwfN3,%E5&NTcj'"
    "bG`k'%#wr%kJHj'<5DT.-,###Q`O4%+<9dXewc.#GAFlGcQ.J3Y@B%BqYrRh1Cgm*PW/$/.e'wKnj#?@Xw*G%I,j+MEuPj)nMHG$EJUM<AUa]Ji#b1=oHt2()jqK2GxHA@1s>(FsR;NV"
    "3/g.,U9W`+hZHH&uuT<03?&A,U6ED+CAZCF,i8&$1ucCjBLc(s_:JfLpE85&h3;G#H``i0*3>D<.)MI3(.Jw#/G>c4-U%],uq-x6''x2&`=gV6-7;X-t-'<pVTdd3w?uD#tJ1T.)GL8."
    "[vE_R.'&],NXtX?v/v)4dmLO'3<n--E)09APWkA#3Y1AncX2W[4S<8&ZgRb4S_B1;/^WA+71Oj1W-HL2fRwS%U%P?,e0=T%J_&<pwf2</QSdY#:C+V/+^S0)xOXI)Ao4h<irK#I%.;u?"
    "CVcf*hDe0(kd+40O`h(,<G?w-4TbY-2v.;@>iDs'jA-i$DV)e5cP;4'=ZZ?.Yg*-*ds9u$a+FI)KTS;$xqDZouVGA#Zwc`4&q:K(K8Ij0lD^E+bTEe=[.x8Bu>@6'AZW%,^8nH)Ug#3D"
    "xi(?#pPvB?th/W$-RSMDHY/Z$3Yc##WYd$vRDDk#7%(/#=>P&Fpgo.#QGVE%6oS&$d8+c'1^6o)`_$6%oi,6/p8'+cSPUlSlLCj21aNfkwUkE*?gmj1F*S=J]H$R&o@G$,Gmgr.?A6t$"
    "(_tT%Y1lv,qgT1;gKh.tku*a*oN%12#43v9hp/w#b%+.)p5$v$Q5.K1XK9D3etco[%FQcDWr[=?n6T'Z6a8e*>Y,B.Fq)HD5aO?@EMAG3Dch/#'F*<-kvfdFPc>p7X)KqT)4gcP&7gf1"
    "/Jbw7(Ga;Rc;m.LRID7;Rt*H=AY=x'2%:/)a1n/;<?=Q'X6kE,8v*E-x2r6*/vHm/xBv3#NZO.#e>CmE/[@p7^_9N:2u:e*)T2^@+k(wK*k^Y#6L@xk/o:8R<vr.C13av6SO2e$]T3a*"
    "Vu(<-Bp$.OqYl]#WB:a#jrkk%$QX=?F9na5P_ec(]S<.)=LeX-3wts8Qr,c*E<7Mj[b2v#[Xcc*icp*4:mvGD<g=c4Wv2d,YG@i1^Wcj'ARY=?B%Ws%T)n`*VOxZ-P_kW/29Ir#&@h*8"
    ",p&0)F`x_40Wd]>0EY1Dx<4B-08e`IX./q92EIr#a-(/#95GA8/Qk[Ijr=c4f^os7e*bM:smj;-P>g(%,v.p&>Jlr7n_wP9L<S@@r1oD*8TJI*JTKdOFb34#:tI-#q3X_?r6[58f$0ZZ"
    "3x:e*IN<+=(Y+wK(h^Y#4:`@k+[qrZ,$4PSq9,B$,APB$a3vS@lLQ?$Ynn8%/29f33.fp.R@Iw#PP#?0Wj7u6r49f3twC.3r81#$GZASU)(^fLfe9a#=Pa''iceLEs84MaD+ph(JU3&-"
    "rqvg)&MkI).q)n'UJk.b)/s0(Du<x>.L2dE8d5'=)G2-*lnHt-OjqZH,<=x>uUY++HBQv.YYVo&5aJX%L6K6&EvwQD^2#DmOdW]/*S8p&5n*:.?+Ub*A>%7/`j=5&aJCWH>%^FG'C$(+"
    "XKXX$'*eW&A&fr#9qg:#8tI-#krYD<b9/j0HAf`*Y`$X-,GT:@$]dv$C]R_#f2'p.G=UF*3(jV$79gg'-*7W68A[d%Q[Z`*$:7b*<DJ#.;.t3;<1,)ko(GQ(6o'E#F`x_4=th[.]w@C%"
    "]v.F<rIP328Wer#W$s4#75GA8e[P61<n9a#w_158A>xSJgh+]>;@8N2HS+%;3QdF#;aR7$0Vr,#HagT7ei]*$0DR<$K5>4<'U988dc/?$_:0/)&TGr7b(U]cMd#n8KbgOj1BR7$oUr,#"
    ")$W?8M;#<%-F.Q8]=c8]2u:e*Ct-I<)0<&LP`i0#jnA*#gO@@#*i1$#IJr6&9YlY#6oCv#;LWP&x/_GdF(/*&U`x_4%Qaa4*dD<%jo;i.><G&c94`Dbv->_b>:V`b,B_s-&5>##<@28$"
    "9,###xNi,#0Z^T7Et+%$H3U'#@#gb4W7Qf*Z>u8.vw&>.2BL=-@`'c*0G7u-41`/:9O1E4S,m]#tpuw.]q.[#7J-U;.`Y&B?2JwH*7T,?,c9U@<uujL%5un'LJ*/2Znrx,4Ytr&p$wpK"
    "<UYH3:rvA0t:/m08wpU/4@dQ&i'rr09EBvZri$D6etRoB)t<F=cBo=7?PIp%Rqiw,aO,)4*bN]Fo;ug*?iua*L8wT%.NCTLk5.51Ct[x,p43.jM%)[.PLUX/xsQZ(XK9D3J//p78%fr?"
    "HD[=?x4$=]Kt0>]C(X7S3vqt+K,om>tkW$4/YP&8f$4B#F`x_4Q,*A;,qGA4jdb)8ME;'HBL(p7@=fr?U$190eZVu([Q-C-hBZA'lbrE%gMaD-fd(lilA-$$<q<ulYl2>c0'_o@P#or-"
    "HE('#pNjj3KQ7'$H_0^#9cCv#<GD-E6B>=7Q-'u.QD^+4nGUv-YiWI)^5Tq$ZFn8%MtR:.i.<9/ulc8.$DXI)F;-g)`_&I*%[*G4HcK+*JIF<%db*P(=_#V/xE(E#J`x_4gdB:%R]jj1"
    "hxNm$.>ru>EjKfD?D)D52X*^,wlA^+9oAh(JQF>-(=qJ)dYA5'hZ$d3YL/:/+okR0jaSn/[Xo*+FUWP&pdGR)u2wZ(H><b31`%U1tUol763MkNv?<J<S@vV-=.uI)qbGc*]M`l0es4#-"
    "RTXU./[b<86XhR9o8dk0^]kT'oi:69rR=n0$wvV%:g:B#`Y<T%[)?R&%fS,,)LlRem=A)4uf-_#[J<RN03(p$.AOoejIci91NViB*tI49S_Nj14tT&$=j5p%xg;E4'cXF3.M#K&<8Uv-"
    "Q6FA#Q.i?#OZD.3/M<F3Zq.[#lHuD#S,m]#+8m]#JgCT%l*`l9ge4r/7p$M1Cq`)+eUUw,9q%a+x[:<-YuG>#a=cZ,.((j(S/DS&LE(K(h;`g(ZC;?#bq(D+13a]+MU=i1ZVxH)Db[x,"
    "eQT,/uF9p&4e.=-B3h+,'vl984)lG4CB'v-Kb%=-TLg5'TheW$]vdh2ba0U%/.c2(?F[s$[:G_+jbnL*)h+x#J5#>uluae'29(u$hud`*mWce%-jrW/6X_J)v:$##mwf&v'Gwo#WMb&#"
    "7pm(#8OC#,;uG##n2=1)Ps$d)oYRD*r`>lL(j,X$FUSv$C]R_#HJJ]$186J*%qtc2'^ae)@GQ&u'cQg)+P&G3aec-3cxnL(k?7x,YBpm&I'X9%>TC1pg@G*$0o0f)XO7W$B4dk*9YNT%"
    "lHH6'F*(x$CoA5'xbYZ,8PrP0.#M6^6.<w6M?%&,%XaU.>[^`3w02mNY/#>`tKw;%vCHiZrf$MDtx#/[^eeQIXZ`eZ@k?eZ'ew>@1rNv'nVo`Fs34q.(FV<-TQSidM9tH;^VQZ$9/[I2"
    "h2eu%5FRS.neeu>#e(?%LG8A$ww%j0egd&mw?s?#^FWZ-pf7[Rm?HA8;w(^#b,;>-9gU]Ru0IG.6DgP8=.]p]#Qu(3;inx4E<q=.F<_=%C#Rt.V+ms0L`0dbsrk@t'MCtLMTOd8u2Ev6"
    ",EGd]ocL78)p3LDT,,',V55',r.S1(dfXXZmHQ>#rHLZ(Lcu.:--55J/(35&)o'58ln/E#8:OQ&I7>NKQfdF<sEWv6b/Ti#:Fd&4;=SPBs<?G*jaXt$P5hS0vW4/,=0F-$FeuW6FWnaN"
    "jxCT.>(sp/vtn5&KMKdb,'uS9NOE-$v7od)4$%?I5h2s-BQ#&#kwf&v$<+I#bFW)#5uS)*%&WK;>pcG*p8<.&t5M=%hwuPAMoIMh^(0g41VrSIvYH7&Clv<.R)Nd21MoH)#k.F4n%&5'"
    "B_Wp%aA<u?x-i;&uJni1[r`2:7Y4F3IB:g(rx7P''[401)&f1(fYc2r;ihi#G=G##^&U'##t_hL^Z92Bx>tX?Gi@mK>lgn)%R^1.Kn.v3B]hfu;Sr%e('+J-]IBc.f1+E*TG$D/fSuLD"
    "jpVVWO,>JINHU+MZrK3D8chi#WH.%#(2<)#,IYd&8^Rl1+MraS&2s3NE92m$=>BRXjD]ga9A&t/HS>M9@1LYP1U`Y#8n+/(6BoW_cQ.J3%H(b,=/kRAc'TspAGOp]'MCtLG]Vp9x_6##"
    ",(HV#TSn8#tvdiLS2UF*HOuX?WEv8/3_D[AwjY5*CpQ1;@#dFV4c-uQ6&uX?L.6;[>tP^65J.I2we<a).t>&4t`Vv:Q'h;[BMgEu>eG(.u2*jLA`l]#^T@%#le95'j1f]4%vrJmn<7RU"
    "e<98S3a&K8iS$_#EwEt$)00@us8nH.L.-5/(VeD+;#l;-;EpS%W$K7/$2vs0=+-?SURa&LPYVKurtUl39IIwuVcxt#XYt&#(HBT7UR02B#Ld]%>G%>-q>)d,:n+Pr3ub2EZ33JkKb4n0"
    "a%FIF2<Wd*@Mf:/?^B5&XSWa*P$'Y-fTuX?e=X=?lRo$$2R<ul3lq+`;9CG)?;=9'GZ`M%e)Ea*3F]T.Wr)HDPqsf4t@m+`xn&TIdHY=?wCikF<FnkFhd'pRimMV?_*X5Sn:r%/Fm.H;"
    "DhW3G<PHLGqx(kkmIMK*]a:]$EtAPpEVQX-pqd2Pk-]X-)@p[U,Ono.En(58*a-8@e_C#$4[$B#G'vVBR<Ck1',gB+h)(=-4IlH8^lbQbv_ev7QV4o$'Alr-3(JpR,=6Z*&F3oA,IK`8"
    "P@*j1`%G'#VD-(#],q^#YDNZ5Mqn%#aE7(+B_[8%4`P>#;L[<$WdP/(lP_<HJU-(#EC*$u*+'u.qLg_4^^D.3j3/W-%P7`&#k9^u)g&H*=tmG*?JZF%ZbC.3ho^;.<Z'Z-Q$ob%,9n,="
    "]-3$#?@%%#GWbBI7O1a4&8o]4c?XA#qYVO'3X(*4B1x;%+QD<%YW^:%1P`_,T7_m(3V%DZG'06&wkx5'V[wo%)4/.caI#Q'@lk?2[Z3l(%B+Z6U$>t$6E_[i>Lw8%b1FM(ulBM)-ue%-"
    "ZGKP8',5e)We(fhdP$`,19KR/cL;-4%jjI3YA,QqFogt#D=G##B37Y%Jq[P/S/Ea*f8%t.`lvGDJ7X=?xji^#s'^[%^Z#s*@etJNH4uvMU</Z$B2t1dHg1248L#Z_+1Dm^BM#bu#9KYN"
    "2_S8#5nl+#Oj01NZp.8[`ddi9bZOQ'cAC2:'Yk2mM0bX-*B9[rU5>##q-QV#/Q:7#*AP##l2vT%]_4We4x:e*:K[sj]2.wK/YR1^5aX_#/sl(3&?.;ZnR>M9D3b,OPF?v&OJ@PJa0b5S"
    "P)8p(,b);ZBMgEu__0j*3Mvu#T-Qp]*VUV$GQDpT.9VK/v2vs0@F*B^':1p$P9#(hn_+/(MKV.h<o1Z#0VlY#7:e8%1JlY#2]1v#-7$Z$wL'^#:$e?$<In8%=@Rs$bdj-$[U39%(7ZZ$"
    "?FRW$526X1U]1Z#2fU;$XKNh#OS(Z#Z?n0#,d-@'ZlU;$/YC;$2oh;$qYj#$:(rv#%(q^#:uU;$5rCZ#<]F#$QT%?$^C=,MRn;v#3oCZ#.7V9.7+@s$&7;s%4o8I$H7q;%iHlBA$u,Z$"
    "$Dg;--`JcM3UucMT[H##>r1Z#.i0wgYF*T%`U@BM-GVE17xh;$AIes$BU<T%URkn-cKn0#8'NW%AIe8%)x#xLH]nU03ihV$2cU;$`CxfLR0&W$64[8%[O%'M?G4p%(kd--T.eS%EBFV."
    "7%@s$bxtkL,4Js$9@,-M&-NZ#5.Is$[qCkb/o'dMH[:H-##9;-%>pV-U-F[T<6:kbhZRX((*^e$RGG##A%;Z#lT>[.<XNp%GWkV.5iL;$<Rov$wEn0#ZchV$=LEp%Ens5&BbNT%,M#<%"
    "nhb,MW^Xp%s)IwBRKM#$EVIm-u8_KcT`:v#84IW$NA4Z$`Z7F10fCv#::[s$:7@<$S4F3.iIofLB3jv#d_kGMx&&W$l1-L-L%+n-5%CwgX:@W$8s8W%:1C[g^eap%GEQ-H^5hwgbAqwg"
    "=1YR*^-,n&OlTs%vUmHH`L@<$9bdxga@[s$Dtjp%FhET%+O;W%o*CHMeeMM/GhNp%`.ffLX&jV$7+7W$9G8$27@3p%:7Rs$6(7s$dIxfL3k`V$G<DiLG]kP&fU4,McVhm&8=wS%:.[8%"
    "7x6s$4U`QjO=Z9iG]0wg,a$@'X:@W$WqT-H3L8[g`eWp%FMfo75.6Z$4(_wg5UZ;%B+Cv$+(('#F1DZ#1r0wgat8Q&SIB0/7lL;$4uMa,a9EZ#9Fes$F*06&mhFgLSeOT%pwkgL-c8,2"
    "MK53'AF7<$51[8%617W$QVbv$cO+gLS<Js$<UNp%'.6Z$wd)I-_L*9%?OnW$U4F3.lOofLEB&w#<@IW$W4L[$:(i;$8x_;$91`Z#..;t-jh'dMT5%*Np)PsLvhXT%fTn0#GK4W-71hwg"
    "6bvV%/U)w$EKuN'bR0)N-WJ<$8LNp%of=nEcOU%$=X39%J<TQ&J$kp%L'tp%PK#R&.wR9&L6^Q&L$96&&cLP/:1Rs$8xUv#3^N?$>U<9%eO4gLvA]s$>K1a35rq;$<O3T%DtS2'>4`;$"
    ";RjP&bC%'MateQ-2=%q.=77W$3r9wgY%V;$2#Uw0mYKd4/'MHMe6^gLGQafL%4_Q&#U/U`*P=,M&4pfL.7v<-D@lG-17rp.JN9U%J)0X:]XNT%1hHh,KG=Z$XCwK>ZmKwgqN#n&N696&"
    "rnOgL<J-R&/c:5/TvC0(r9UdMDDIU0;7[8%91[8%g[4gLQ?o8%3qrS&Vd0wgrC*T%BbET%IH>j'74Rs$F$4X$D'TQ&>ejp%.iBwgMo0wghYI?p`I7w#aI4gLB^(dME8rdMQL,W-i^w0#"
    ")$9I$WsZ)G,e(HM1d$qMd34,%jt+/(CrU;$<u$W$OTP3'bc,d.8fLv#e&4$3D[*9%SBT6&TQ>3'PBpm&vm(CA%R53'VNgm&<x$W$;:R8%?O*9%0B(h.>h8m&Wn>-0C[*t$8oCZ#ZC%'M"
    "rxOt$lF=,M=G>X$G9#n&@q/6&9..<$Ak&6&[b):&:F<p%>+Is$H*96&O.+n-tMGF%Q(1[gmEpm&XmlN'@s(P-7%T,MYKVt-jU=gLVQx8%Ojgq%SlTs%FLhwg*tN1#[1nS%LBPj'Vjuj'"
    "VL26'RKpQ&;QgoLdPQN'WQgm&T.+n-94_wgLl0wg-V,w$@UET%oq9dM/j49%FU[W$#=U-M22U2'HCes$E@@s$PxeQ-M.F3.l6H&Oq11[-fd*1#U1([gU&w-H77ugL2/m5MXCCp$gXUX("
    ",6^e$cll##TXrv#=1`;$KWcj'V@'k.?77s$gD-.%$On0#a:3p%MEPj'Z&D0(WpuN'+_.T&q^n0#sW#R&^p57&pUxfL`^&w#CbWp%@bjp%::IW$n95b@]@IW$r<hdM'BSs$lhXgL8Act$"
    "F:e8%O/R-)V3VE1E[j5&@@wS%>XNp%V[#h/<uh;$C7rV$5'/T&*BCkb^t;qiJH6hG*[.T&nan0#@v0[B/fO,Mv9#gL0IVt-m_FgLY/K<4Wb%w#?=%W$M^lj'B+.W$A@@s$7q2acnvA<$"
    "E-0q%X/;k'ZpP3'Y^5n&)[h-MxG<k'a`wH)N'=t$=Xa5&?X*9%jbtcMlSjN9uoBwgkO*T%9@Rs$K6^Q&L30:%pF+gL;CxH)j6E6'Hk8Q&g=ofL,'Yp%DR3T%>(r;$FI@s$5'/T&*BCkb"
    "=U_dM:MQ3'l_b-Ht;`0(&@3W-;@dTiNxBwg9'sS&/_`s%UYje)Zk%n'?nJm&%L1hL[(p;Q8Pqm&Tv:O'`,)4'B7L<hwM[H)rs<L#(9i0(as(O':7Rs$Dbjp%Ur^s%nwOgLcnXp%IKGN'"
    "ltB)N@ihQ&N<9:%KL3T%TMEe)G<5N'SreQ-XRB0/Hb*9%8x0wgltw<$F6pm&u0CdMYM.IMPtcW-qjw0#K3f2('t:_A4%;p&kUC=(e-06&QT0wg)KmR&h$(hL2AqQ&G7:wg_L[8%hD-.%"
    "&Un0#iLW5&Up_g(dM7h(aDr0([vl3'BJOZ/bAr0(c&Dk'T%$9&leOgL1;^W$E9>j'J*96&Ekj5&L'bp%=F*p%CkaT%NM6wg]OLwg;:lGM5A_m&F[a5&Bks5&T7bN.A17W$X:m5'km*1#"
    "c6a6jOmv-H$T7L(#X'99`pkT%$.q%%T.Z5'lsg-*O=@s$D:e8%_J@L(Ni^8&MV'W%Ps$i#jUsP&X&%-)fVR-)dP7L(`2;O'=5bV0c/v3'jM;o&we+gLVNS<$;.Lwg__j5&r-YgL&%(6&"
    "Ng10(9(Cwghw&q%Nq3t$/h6.M9cvJ(ShWT%Pk3T%sh=gL6/PT%Kk39%G_w8%I'+=$0.;t-w3CdM_Y<78u&4m'p#@eH#TRh(3nh-M2t`E-aC%'Mfj`qLk;63'G7$65Ng?D*Q@@s$F@n8%"
    "c]eh(@_j5&@(%<$g,$e%S4Lwg1dIh(eADk'f>Mk'2$7.Mv]Sh(oLTb*WKt9%gbkGM/&(q%mtXgLGH)K(.WDa-:1LwgY4rv#Rc.j1TgP3'`lNa*Ckjp%*-%iL9./L(VODQ'AU*t$VODQ'"
    "KEGN'YhIj(/i0wglQ^6&^*O,*N4Dm'D=#-M7T]h(jx*.)ORUwg_qHxgWF_wgME.iLYP@eMXDi-M.m4X$9=$xgohAm&^Aed)m%Fe)iieh(dD`k'9nh-MmCS-)o`.h(<i0wggw&Q&L0B6&"
    "U4HT&QWgQ&Esf=%TQ^q%B:%w#H'Tm&WvuN'c(t&+cEO0)X$'q%T$Op%ZL'k.NwNT%P(+n-D71wgqpuN''RqHM3Z7.MRh/,M7R+.)pZ4wg@v0[Bg306&wq>v$;-<=(kQ#n&+hIT&/,>>#"
    "+I*j0[VP-VodPir(_=Sn01l&#YNfoP.(L#v$]2@$JeSCM=LeM0&5YY#n$*)#KSFgLT7<K$:95hNCfcs-n+M*ML0b4#V&5>#T@j'MZpT(MdC(=#,$L'#4ul+#Z55##$0U'#u$o%#'b''#"
    "Y3`*Mfi9=#jLf5#<#7qL85P$M[_b)#:$5F%(9OS.`)2>5iJl.UZcQ>6?osY-Qg]G3dGKg22v?S@MhlS/rKLm9ii*878GBR3s<QwBw0;_8$`kA#r<8F-/)m<-@O#<-6M#<-UN#<-<N#<-"
    "j6T;-jgG<-7M#<-m7T;-s6T;-I6T;-RFn]-LK4F%/.m--)3,F%%6YF%b34F%:A[w'wq.F%J)l-$3q9D3m+[w'4<k-$[c8^#^GA;1H*2,#2:%@#u5$C#MX`=-jXp,03*gE#]iBB#<N#<-"
    "xNp,0XvJ*#%4r?#_ae6/xSk&#wMNjLb#p@#FsWN0pf39#$b2<#)W9%M6lw&MDX[&M]e=rLP@7&Me@W$#^0,)M+)<7#psq7#+C*1#[QC7#h(c(.M`ovLPlFrLFV>W-A..F%K<J`E>X>lf"
    "p$6GDVR8A4Om8G;WnTw0#o&,VLFBMB=(ZuusDW9/TJ95/[Z'5JUUm-$&f#MgX%(W%ViFv$6eXw9;l2F%I*2^#x`9T6Za5A4'5#/:?DZc2GEru,q5IS7XwNY>B2'/:PY_@-1w2:/qx]E#"
    "N0LhL?]IC##wvf;lAR>QEtow'6Qdf(`#12'&?0R<>7ai0]h)20DDMJ(5:(58%MUc;meIK)W<HS7RFm-$_u./(f/T9`ek[w02fK#$>Z`=-rnI3%v1%G%[CS2iN1Pp7soK#$@K`m$oWK-Q"
    "ntW_&3uN2i-u:T.Ys)D#[aoJ.3*gE#1]0^#Q@qS-Pr.>-9-,p77PP8/D.9v-DFP8/S3+9.j7$C#QNQ-H]Xow'B=KG)+lP8.jOHC#tCF&#XJP#YVNi,#v%>Y#2GhJ##p8.$Dtk2$+H'>$"
    "H)0Q$qNeW$nVvZ$6.<d$aqMw$5Nw8%w=Mk%Poed%Ks>r%0sp#&/qC(&Sc3=&nN0''-rEA':hnd'*Zd,(Z45J(GR.L(N(Rs(^P$S).BK#*)TD@*2TM[*G^Tb*FZ[-+sGq7+%M3]+(bDD+"
    "6<Ms+d8?g+*ZX:,KM?4,+1@D,7/Ur,YSk6-qcM)-?`ZS->0bI-vOhZ-?EG7.s3P/.]';8.(bEE.`pCo.aob^.h2bi.]#P+/3mFF/R8OQ/2?ed/1coi/;bAq/4bsx/t=p>0&?[h0CTLg0"
    ":oDl0`(JE1U'C?1A-[d11(LZ1iCvg1B8p#2#sq02V+dG2N:XU2L2&m2jl29303tm3N65?4mWEi4.l9$5>cbF5<aN26)ULS6,L8Y6sO.E72rBc7_#[18Sr)E8@]@e80)T/9J-(490Ptb9"
    ":D@&:2JBv9xC;5:SCQ`:DpaU:0[S';<-[p:*Q>0;$80Z;q3LO;v)P$<D8n9<Z*S6<XXf+#$####>4m3#,iu8.%/5##,_iW%%/5##c8+##Js+Q'^7DF%*lO&#Q;^;--`/,MCw(hL6oG<-"
    "Z=eA-2':W.4xL$#w@`T.6(V$#/A`T.+5>##:sG<-DKx>-?;Mt-_RFgLJ;^-#OA`T.B-4&#ZtG<-@Kx>-'C`T.C*=A#4@`T.F6OA#P@`T./Pu>#krG<-,s?W%>xSdF>M_oD2<XG-MhW?-"
    "g$fFHjFV.G/ZFC59].@-^meF-B#)<-9rCVC:1k(IivMh;$g1eG^:fP9,>IFHnR#SC#*N=BcI%+Na,Yv-@w+CPj-DP8XhdxFeBEYGIYo+DUw%;HlBel/Zl,20itbP9OpMDF*^o`=f$g'/"
    "Zet9)Zf3>5L3*j1j_As7i-7L#)0R.6NMWq)uc8JCrTn-$.R]rH3@b2CIgSc;rD?2CGQMDFBxj>-smOq2hf0DE?kdYHw;SD=B$L/De)4X:4$(@'=m.A0MWqx9a(CqDn*<SESFU^-M@?uu"
    "J-3)#SH(5gUmXs6.7t9#87>##,AP##0Mc##4Yu##8f1$#<rC$#@(V$#D4i$#H@%%#LL7%#PXI%#Te[%#Xqn%#]'+&#8wdiL'5Z[-iq18.m3io.qKIP/ud*20#'bi0'?BJ1+W#,2/pYc2"
    "32;D37Jr%4;cR]4?%4>5^2ds-E8E)#IDW)#MPj)#Q]&*#Ui8*#YuJ*#^+^*#b7p*#=1SnLnCQ+<n:Q`<rR2A=vkix=$.JY>(F+;?,_br?0wBS@49$5A8QZlAI5Qxb:eo=cQkOfLjeuFi"
    "S0](a5P,M^:SKl]aa-DN4;0P]4PbxO?POo[-q'DW-fJcMxUq4S=Gj(N=eC]OkKu=YRH)AOTue+MTm6fqOR(>PCeG`NoZlIqmW:YYdQqFrhod(WEH1VdDMSrZ>vViBn_t.CTp;JCbMMrd"
    "ku.Sek+f4f^w>igqOb1g:-r+DPn?VQ$(_.hbS8GDm*m(EM(1DER=L`E2m(GVYq)>G['EYGbKAVHdW]rHis=SIm/YoIrVq1KXpa._D1SiKx%n.L<U=loQSK&Fu@ViFv.1VC(3f:C$]eFH"
    "pg`9CB+KG0';G##PR`i0=Jm*%$vou,_>c'&[qFJ(x:v'&o.HP/GUw'&og;A+T*WV$c$A;$.o68%Nm8Ath6#@=";

static void StyleModerneFraktur() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Enhanced base colors with more variation
    ImVec4 textColor = ImVec4(0.92f, 0.88f, 0.75f, 1.00f); // Warmer parchment
    ImVec4 bgColor = ImVec4(0.12f, 0.09f, 0.06f, 1.00f);  // Slightly lighter for depth
    ImVec4 accentColor = ImVec4(0.45f, 0.28f, 0.16f, 0.95f); // Weathered wood tone
    ImVec4 darkAccentColor = ImVec4(0.22f, 0.16f, 0.11f, 1.00f); // Deep shadow
    ImVec4 frameBg = ImVec4(0.17f, 0.13f, 0.09f, 0.92f); // Slightly transparent
    ImVec4 activeColor = ImVec4(0.68f, 0.45f, 0.28f, 1.00f); // Warm highlight
    ImVec4 hoveredColor = ImVec4(0.55f, 0.38f, 0.23f, 0.97f); // Mid-tone hover

    // Add some "worn" variation to borders
    ImVec4 borderDark = ImVec4(0.25f, 0.18f, 0.12f, 0.85f);
    ImVec4 borderLight = ImVec4(0.52f, 0.35f, 0.20f, 0.75f);

    // Text and Global
    style.Colors[ImGuiCol_Text]                   = textColor;
    style.Colors[ImGuiCol_TextDisabled]           = ImVec4(0.45f, 0.42f, 0.38f, 0.80f);
    style.Colors[ImGuiCol_WindowBg]               = bgColor;
    style.Colors[ImGuiCol_ChildBg]                = ImVec4(0.14f, 0.11f, 0.08f, 0.85f);
    style.Colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.08f, 0.06f, 0.97f);
    style.Colors[ImGuiCol_Border]                 = borderDark;
    style.Colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.40f); // Subtle shadow
    style.Colors[ImGuiCol_FrameBg]                = frameBg;
    style.Colors[ImGuiCol_FrameBgHovered]         = hoveredColor;
    style.Colors[ImGuiCol_FrameBgActive]          = activeColor;
    style.Colors[ImGuiCol_TitleBg]                = ImVec4(0.32f, 0.22f, 0.14f, 0.95f);
    style.Colors[ImGuiCol_TitleBgActive]          = ImVec4(0.52f, 0.36f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.20f, 0.15f, 0.10f, 0.85f);

    // Scrollbar (make it look like aged wood)
    style.Colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.15f, 0.12f, 0.09f, 0.80f);
    style.Colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.35f, 0.24f, 0.16f, 0.90f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.48f, 0.33f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.62f, 0.42f, 0.26f, 1.00f);

    // Checkbox, Slider, Button
    style.Colors[ImGuiCol_CheckMark]              = ImVec4(0.82f, 0.65f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]             = ImVec4(0.48f, 0.32f, 0.20f, 0.95f);
    style.Colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.72f, 0.52f, 0.32f, 1.00f);
    style.Colors[ImGuiCol_Button]                 = ImVec4(0.38f, 0.26f, 0.16f, 0.90f);
    style.Colors[ImGuiCol_ButtonHovered]          = ImVec4(0.52f, 0.36f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]           = ImVec4(0.68f, 0.48f, 0.30f, 1.00f);

    // Header, Separator, ResizeGrip
    style.Colors[ImGuiCol_Header]                 = ImVec4(0.28f, 0.20f, 0.13f, 0.88f);
    style.Colors[ImGuiCol_HeaderHovered]          = ImVec4(0.45f, 0.32f, 0.20f, 0.95f);
    style.Colors[ImGuiCol_HeaderActive]           = ImVec4(0.58f, 0.42f, 0.26f, 1.00f);
    style.Colors[ImGuiCol_Separator]              = borderLight;
    style.Colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.62f, 0.45f, 0.28f, 0.90f);
    style.Colors[ImGuiCol_SeparatorActive]        = ImVec4(0.75f, 0.55f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ResizeGrip]             = ImVec4(0.32f, 0.22f, 0.14f, 0.65f);
    style.Colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.48f, 0.34f, 0.21f, 0.85f);
    style.Colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.65f, 0.48f, 0.30f, 1.00f);

    // Tabs
    style.Colors[ImGuiCol_Tab]                    = ImVec4(0.25f, 0.18f, 0.12f, 0.88f);
    style.Colors[ImGuiCol_TabHovered]             = ImVec4(0.52f, 0.37f, 0.23f, 0.95f);
    style.Colors[ImGuiCol_TabActive]              = ImVec4(0.48f, 0.34f, 0.21f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]           = ImVec4(0.20f, 0.15f, 0.10f, 0.85f);
    style.Colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.38f, 0.27f, 0.17f, 0.95f);

    // Plot, Text Input, Tables
    style.Colors[ImGuiCol_PlotLines]              = ImVec4(0.85f, 0.72f, 0.52f, 1.00f);
    style.Colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.95f, 0.80f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogram]          = ImVec4(0.75f, 0.58f, 0.38f, 1.00f);
    style.Colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.88f, 0.70f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.28f, 0.20f, 0.13f, 0.92f);
    style.Colors[ImGuiCol_TableBorderStrong]      = borderDark;
    style.Colors[ImGuiCol_TableBorderLight]       = ImVec4(0.32f, 0.23f, 0.15f, 0.65f);
    style.Colors[ImGuiCol_TableRowBg]             = ImVec4(0.16f, 0.12f, 0.09f, 0.75f);
    style.Colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.19f, 0.14f, 0.10f, 0.75f);
    style.Colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.72f, 0.58f, 0.38f, 0.45f);
    style.Colors[ImGuiCol_DragDropTarget]         = ImVec4(0.85f, 0.68f, 0.45f, 0.90f);
    style.Colors[ImGuiCol_NavHighlight]           = ImVec4(0.72f, 0.55f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.85f, 0.70f, 0.50f, 0.90f);
    style.Colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.15f, 0.12f, 0.09f, 0.65f);
    style.Colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.08f, 0.06f, 0.04f, 0.75f);

    // Styling properties - Add some "weight" and texture
    style.Alpha = 0.98f;                      // Slight transparency for depth
    style.FrameRounding = 2.0f;               // Very slight rounding (old wood bevels)
    style.WindowRounding = 0.0f;              // Keep windows sharp
    style.GrabRounding = 3.0f;                // Rounded grab (worn handles)
    style.TabRounding = 2.0f;                 // Slight tab rounding
    style.ScrollbarRounding = 4.0f;           // Rounded scrollbar (like a dowel)
    style.ChildRounding = 0.0f;               // Sharp child windows
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowBorderSize = 2.0f;            // Thicker border (aged frame)
    style.FrameBorderSize = 1.5f;             // Medium border
    style.PopupBorderSize = 2.0f;             // Thicker popup border
    style.TabBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f); // More breathing room
    style.FramePadding = ImVec2(8.0f, 4.0f);    // Comfortable padding
    style.ItemSpacing = ImVec2(10.0f, 6.0f);    // Generous spacing

    style.WindowMenuButtonPosition = ImGuiDir_Right;
}
