#include "LetterToSound.h"
#include "../include/PhonemeDefs.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace SharpVox {

// Static member definitions
uint8_t LetterToSound::CharacterFeatureTable[128] = {};
std::vector<LetterToSound::CompiledRule> LetterToSound::CompiledLetterToSoundRules[26];
bool LetterToSound::s_initialized = false;

// Format: "LEFT[MATCH]RIGHT=OUTPUT"
// Special symbols in LEFT / RIGHT (not between brackets):
//   #  1+ vowels          *  1+ consonants   .  voiced consonant
//   $  1 consonant + E/I  %  suffix           &  sibilant
//   @  long-U consonant   ^  exactly 1 cons   +  front vowel (E I Y)
//   :  0+ consonants      ' '  word boundary
// OUTPUT: space-separated NRL phoneme names, or empty for silence.

static const char* const LTS_A[] = {
    " [A]W^=AX0",
    " [A]BOU=AX0",
    " [A]GAIN=AX0",
    " [A]LONG=AX0",
    " [A]ROUND=AX0",
    " [A]CROSS=AX0",
    " [A]MONG=AX0",
    " [A]LONE=AX0",
    " [A]BOVE=AX0",
    " [A]LIKE=AX0",
    " [A]LIVE=AX0",
    " [A]HEAD=AX0",
    "#:#:[A]TOR=EY0",
    "#:#:[A]TED =EY0",
    "#:#:[A]TES =EY0",
    "#:#:[A]TING=EY0",
    "#:#:[ATE] =EY0 T0",
    "[A] =AX",
    " [ARE]=AA R",
    " [AR]O=AX R",
    "#[AR]#=EH R",
    " ^[AS]#=EY S",
    "[A]WA=AX",
    "[AW]=AO",
    " :[ANY]=EH N IY",
    "[A]^+#=EY",
    "#:[ALLY]=AX L IY",
    " [AL]#=AX L",
    "[AGAIN]=AX G EH N",
    "#:[AG]E=IH JH",
    "#[A]^+#=AE",
    " *[A]^+ =EY",
    "[ARE] =EH R",
    "[ARED] =EH R D",
    "[ARES] =EH R Z",
    "[AR]ING=EH R",
    "[ARE]NT=EH R",
    "[A]^%=EY",
    " *[ARR]=EH R",
    "[ARR]=AE R",
    " *[AR] =AA R",
    "[AR] =ER",
    "#:[AR]Y =EH2 R",
    "[AR]=AA R",
    "[AIR]=EH R",
    "[AI]=EY",
    "[AY]=EY",
    "[AU] =OW",
    "[AU]=AO",
    "#*:[AL] =AX L",
    "#*:[ALS] =AX L Z",
    "[ALK]=AO K",
    "[ALL] =AO L",
    "[ALL]S =AO L",
    "[ALL]%=AO L",
    "[A]LL=AE",
    "QU[AL]#=AA L",
    "[AL]#=AE L",
    "[AL]=AO L",
    " [ABLE]=EY B AX L",
    " *[ABLE]=EY B AX L",
    "[ABLE]=AX B AX L",
    "[ANG]+=EY N JH",
    "[A]=AE",
    nullptr,
};
static const char* const LTS_B[] = {
    " [BEEN] =B IH N",
    "[BB]=B",
    " [BE]^#=B IH0",
    "[BEING]=B IY IH NG",
    " [BOTH] =B OW TH",
    " [BUS]#=B IH Z",
    "[BUIL]=B IH L",
    "[B]=B",
    nullptr,
};
static const char* const LTS_C[] = {
    " [COMM]=K AX0 M",
    " [CONN]=K AX0 N",
    " [COM]P=K AX0 M",
    " [CON]^#=K AX0 N",
    " [CON]^^#=K AX0 N",
    "[CIALLY]=SH AX3 L IY",
    "[CIAL]=SH AX3 L",
    "[CIAN]=SH AX3 N",
    "[CIOUS]=SH AX3 S",
    " [CITY]=S IH T IY",
    " [CITI]=S IH T IY",
    " [CHR]=K R",
    "[CHR]=K R",
    " [CHOR]=K AO R",
    " [CHEM]=K EH M",
    "[CH]ARACT=K",
    "[CH]ITECT=K",
    "TE[CH]=K",
    "ME[CH]AN=K",
    "PSY[CH]=K",
    "AR[CH]I=K",
    "E[CH]O=K",
    "[CH]AEL=K",
    "[CH]ESTR=K",
    "MA[CH]IN=SH",
    " [CH]EF=SH",
    " [CH]IC=SH",
    "[CH]UTE=SH",
    "[CH]R=K",
    "[CH]L=K",
    "[CHA]OS=K EY",
    "[CHE]M=K EH",
    "[CH]OR=K",
    "[CH]OL=K",
    "[CH]ORE=CH",
    "[CH]=CH",
    " S[CI]#=S AY",
    "[CI]A=SH",
    "[CI]O=SH",
    "[CI]EN=SH",
    "X[C]+=",
    "[CC]+=K S",
    "[CC]=K",
    "[C]+=S",
    "[CK]=K",
    ".[COM]%=K AH M",
    "[C]QU=",
    "[C]=K",
    nullptr,
};
static const char* const LTS_D[] = {
    " [DIS]^#=D IH0 S",
    " [DIS]^^#=D IH0 S",
    "[DD]=D",
    "[D]G+=",
    "#*:[DED] =D IH D",
    " [DE]^#=D IH0",
    " [DO] =D UW",
    " [DOES]=D AH Z",
    " [DOING]=D UW IH NG",
    " [DOW]=D AW",
    "[DU]A=JH UW",
    "[D]=D",
    nullptr,
};
static const char* const LTS_E[] = {
    " [EX]^#=IH0 K S",
    " [EX]^^#=IH0 K S",
    "#:T[ED] =IH0 D",
    "#:D[ED] =IH0 D",
    "#:SS[ED] =T0",
    "#:NS[ED] =T0",
    "#:RS[ED] =T0",
    "#:PS[ED] =T0",
    "#:LS[ED] =T0",
    "#:C[ED] =T0",
    "#:X[ED] =T0",
    "#:SH[ED] =T0",
    "#:CH[ED] =T0",
    "#:PH[ED] =T0",
    "#:P[ED] =T0",
    "#:K[ED] =T0",
    "#:F[ED] =T0",
    "#:UGH[ED] =T0",
    "AS[ED] =T0",
    "#:[ED] =D0",
    "#:&[ES] =IH0 Z",
    "#:P[ES] =S0",
    "#:T[ES] =S0",
    "#:K[ES] =S0",
    "#:F[ES] =S0",
    "#:[ES] =Z0",
    "#*[E] =",
    "#*^[E] =",
    " :[E] =IY",
    "#[ED] =D",
    "[EV]ER=EH V",
    "[E]R%=IH",
    "[E]TE =IY1",
    "[E]ME =IY1",
    "[E]SE =IY1",
    "[E]TED =IY1",
    "[E]TELY=IY1",
    "[E]MELY=IY1",
    "#:[E]^%=IX",
    "[E]^ER#=EH",
    "[E]^ES#=EH",
    "[E]^%=IY",
    "[ERI]#=IH R IY",
    "[ERI]=EH R IH",
    "#:[ER]#=ER",
    "[ER]#=EH R",
    "[ER]=ER",
    " [EVEN]=IY V EH N",
    "#^[E]W#=",
    "#:[EW]=UW",
    "@[EW]=UW",
    "[EW]=Y UW",
    "[EOUS]=IY AX S",
    "[EO]N=IY AX",
    "G[EO]^=IY AA",
    "[EO]=IY OW",
    "#*[ELY] =L IY",
    "#*[EMENT] =M EH N T",
    "[EFULL]=F AX0 L",
    "[EFUL]=F AX0 L",
    "[EER]=IH R",
    "[EE]=IY",
    "[EARN]=ER N",
    "[EAR] =IH R",
    "[EAR]S =IH R",
    " [EAR]^=ER",
    "[EAD]=EH D",
    "#*[EA] =IY AX",
    "[EA]SU=EH",
    "[EA]LTH=EH",
    "[EA]TH=EH",
    "[EA]THER=EH",
    "[EA]SANT=EH",
    "[EA]VY=EH",
    "[EA]DY=EH",
    "[EA]ST=IY",
    "[EAR]^=ER",
    "[EAR]=IH R",
    "[EA]=IY",
    "[EIGH]=EY",
    "[EI]N=AY",
    "[EI]^ =AY",
    "[EIGN]=EY N",
    "[EIGH]=EY",
    "[EI]=IY",
    " [EYE]=AY",
    "[EY]=IY",
    "[EUR]=UH R",
    "^[EU]=UW",
    "[EU]=Y UW",
    " [E]MA=IY",
    "[E]=EH",
    nullptr,
};
static const char* const LTS_F[] = {
    " [FROM] =F R AH M",
    "[FF]=F",
    " [FULL]=F UH L",
    "#:[FULL]=F AX0 L",
    "#:[FUL]=F AX0 L",
    "[FUL]=F UH L",
    "[F]=F",
    nullptr,
};
static const char* const LTS_G[] = {
    " [GH]=G",
    " [GN]=N",
    "[GIV]=G IH V",
    " [G]I=G",
    "[GE]T=G EH",
    "SU[GGES]=G JH EH S",
    "[GG]=G",
    " B#[G]=G",
    "[G]+=JH",
    "[GREAT]=G R EY T",
    "#[GH]=",
    "[G]=G",
    nullptr,
};
static const char* const LTS_H[] = {
    " [HAV]=HH AE V",
    " [HERE]=HH IY R",
    " [HOUR]=AW ER",
    "[HOW]=HH AW",
    "[H]#=HH",
    "[H]=",
    nullptr,
};
static const char* const LTS_I[] = {
    "#:#:[I]ZE=AY0",
    "#:#:[I]SE =AY0",
    "#:[ITUDE]=IX0 T UW2 D",
    "#:[ITIS] =AY1 T IX0 S",
    "#:[ICALLY]=IH3 K L IY",
    "#:[ICALLY]=IH3 K AX L IY",
    "#:[IC] =IH3 K",
    "#:[ICS] =IH3 K S",
    "#:[ICAL]=IH3 K AX L",
    "#:[ICALLY]=IH3 K L IY",
    "#:[ITY] =IH3 T IY",
    "#:[ITIES] =IH3 T IY Z",
    " [I]NN=IH",
    " [IN]=IH N",
    " [I] =AY",
    "[IN]D =AY N",
    "[IN]DS =AY N",
    "[IN]DING=AY N",
    "[IN]DER=AY N",
    "[IN]DLY=AY N",
    "F[IER]=AY ER",
    "PL[IER]=AY ER",
    "[IE]LD=IY",
    "[IE]F=IY",
    "[IE]CE=IY",
    "[IE]VE=IY",
    "FR[IE]ND=EH",
    "#^[IE] =IY",
    "#^[IES] =IY Z",
    "[IER]=IY ER",
    "#*R[IED] =IY D",
    "[IED] =AY D",
    "[IEN]=IY EH N",
    "[IE]T=AY EH",
    " :[I]%=AY",
    "[I]%=IY",
    "[I]E=IY",
    "[I]^+#=IH",
    "[IR]I=IH R",
    "[IR]E =AY ER",
    "[IR]ED =AY ER",
    "[IR]ES =AY ER",
    "[IR]#=AY R",
    "[IZ]%=AY Z",
    "[IS]%=AY Z",
    "[ID]%=AY D",
    "+^[I]+=IH",
    "#:[I]TED =IX",
    "[I]T%=AY",
    "[I]BE =AY",
    "[I]LE =AY",
    "[I]NE =AY",
    "[I]ZE =AY",
    "[I]DE =AY",
    "[I]ME =AY",
    "#*:[I]^+=IH",
    "[I]^+=AY",
    "[IR]=ER",
    "[IGH]=AY",
    "[I]LDR=IH",
    "[ILD]=AY L D",
    "[IGN] =AY N",
    "[IGN]^=AY N",
    "[IGN]%=AY N",
    "[IQUE]=IY K",
    "LL[I]O=Y",
    "N[I]ON=Y",
    "LL[I]O=Y",
    "N[I]ON=Y",
    "L[I]A =Y",
    "N[I]A =Y",
    "#:[I]#=IY",
    "[I]=IH",
    "C[ION]=AX N",
    nullptr,
};
static const char* const LTS_J[] = {
    "[J]=JH",
    nullptr,
};
static const char* const LTS_K[] = {
    "[KK]=K",
    " [K]N=",
    "[K]=K",
    nullptr,
};
static const char* const LTS_L[] = {
    "#:[LESS]=L IX0 S",
    "#:[LY] =L IY0",
    "[LL]=L",
    "[LO]C#=L OW",
    "#^:[L]E =AX0 L",
    "#^:[L]ES =AX0 L",
    "#^:[L]ED =AX0 L",
    "[LEAD]=L IY D",
    "[L]=L",
    nullptr,
};
static const char* const LTS_M[] = {
    "[MM]=M",
    "[MOV]=M UW V",
    "[M]=M",
    nullptr,
};
static const char* const LTS_N[] = {
    "#:[NESS]=N IX0 S",
    "[NN]=N",
    "E[NG]+=N JH",
    "[NG]R=NG G",
    "[NG]#=NG G",
    "[NGL]%=NG G AX L",
    "[NG]=NG",
    "[NK]=NG K",
    " [NOW] =N AW",
    "[N]=N",
    nullptr,
};
static const char* const LTS_O[] = {
    "#:[OMETER]=AA1 M AX0 T ER",
    "#:[ONOMY]=AA1 N AX0 M IY",
    "#:[OLOGY]=AA1 L AX0 JH IY",
    "#:[OLOGIES]=AA1 L AX0 JH IY Z",
    "#:[OLOGIST]=AA1 L AX0 JH IX S T",
    "#:[OLOGISTS]=AA1 L AX0 JH IX S T S",
    "[OF] =AH V",
    "[OROUGH]=ER OW",
    "#:[OR] =ER",
    "#:[ORS] =ER Z",
    "[O]RR=AO",
    "[OR]=AO R",
    " [ONE]=W AH N",
    "SH[OW]=OW",
    "T[OW]N=AW",
    "D[OW]N=AW",
    "BR[OW]=AW",
    "ALL[OW]=AW",
    "C[OW]=AW",
    "H[OW]=AW",
    "V[OW]=AW",
    "P[OW]=AW",
    "[OW]=OW",
    " [OVER]=OW V ER",
    "N[OV]=OW V",
    "PR[OV]E=UW V",
    "[OV]=AH V",
    "S[O]ME=AH",
    "C[O]ME=AH",
    "[O]^%=OW",
    "[O]^EN=OW",
    "[O]^I#=OW",
    "[OLD]=OW L D",
    "[OUGHT]=AO T",
    "[OUGH]=AH F",
    " [OU]=AW",
    "H[OU]S#=AW",
    "[OUS]=AX S",
    "M[OUR]N=AO R",
    "[OUR]N=ER",
    "T[OUR]=UH R",
    "[OUR]=AO R",
    "[OULD]=UH D",
    "^^[OU]L=AH",
    "[OUP]=UW P",
    "C[OU]NTR=AH",
    "[OU]BLE=AH",
    "[OU]NG=AH",
    "[OU]=AW",
    "[OY]=OY",
    "[OING]=OW IH NG",
    "[OI]=OY",
    "[OOR]=AO R",
    "[OOK]=UH K",
    "BL[OOD]=AH D",
    "FL[OOD]=AH D",
    "H[OOD]=UH D",
    "W[OOD]=UH D",
    "ST[OOD]=UH D",
    "G[OOD]=UH D",
    "[OOD]=UW D",
    "[OO]=UW",
    "[O]E=OW",
    "[O] =OW",
    "[OA]=OW",
    " [ONLY]=OW N L IY",
    " [ONCE]=W AH N S",
    "*[ON] T=OW N",
    "[O]NG=AO",
    "[O]NN=AA",
    "[O]MM=AA",
    " M[ON]TH=AH N",
    " FR[ON]T=AH N",
    " S[ON] =AH N",
    " W[ON]D=AH N",
    " M[ON]D=AH N",
    " M[ON]EY=AH N",
    " T[ON] =AH N",
    " T[ON]S =AH N",
    " ^:[ON]=AA N",
    "#:[ON]=AX N",
    "#*[ON] =AX N",
    "#^[ON]=AX N",
    "[O]ST =OW",
    "[OFF]=AO F",
    "[OF]^=AO F",
    "[OTHER]=AH DH ER",
    "[OSS] =AO S",
    "#*:[OM]=AH M",
    "H[O]ST=OW",
    "P[O]ST=OW",
    "M[O]ST=OW",
    "GH[O]ST=OW",
    "[O]=AA",
    nullptr,
};
static const char* const LTS_P[] = {
    "[PP]=P",
    "[PH]=F",
    "[PEOP]=P IY P",
    "[POW]=P AW",
    "[PUT] =P UH T",
    "[P]=P",
    nullptr,
};
static const char* const LTS_Q[] = {
    "[QUAR]=K W AO R",
    "[QU]=K W",
    "[Q]=K",
    nullptr,
};
static const char* const LTS_R[] = {
    "[RR]=R",
    " [RE]^#=R IH0",
    "[R]=R",
    nullptr,
};
static const char* const LTS_S[] = {
    "[SSION]=SH AX3 N",
    " [SUB]^#=S AX0 B",
    " [SUC]C=S AX0 K",
    " [SUP]P=S AX0",
    " [SUR]^#=S ER0",
    "[SH]=SH",
    "#[SION]=ZH AX3 N",
    "[SOME]=S AH M",
    "#[SUR]#=ZH ER",
    "[SUR]#=SH ER",
    "#[SU]#=ZH UW",
    "#[SSU]#=SH UW",
    "#[SED] =Z D",
    "OU[S]E=S",
    "PLEA[S]E=Z",
    "EA[S]E=S",
    "^A[S]E=S",
    "CLO[S]E=S",
    " DO[S]E=S",
    "PO[S]E =S",
    "CO[S]E=S",
    "OMI[S]E=S",
    "EMI[S]E=S",
    " RE[S]OUR=S",
    " RE[S]EARCH=S",
    " RE[S]EA=S",
    " RE[S]ET=S",
    " RE[S]ELL=S",
    "AU[S]E=Z",
    "#[S]#=Z",
    "[SAID]=S EH D",
    "^^[SION]=SH AX3 N",
    "[SS]=S",
    ".[S] =Z0",
    "#*.E[S] =Z0",
    "#*^##[S] =Z0",
    "#*^#[S] =S",
    "U[S] =S",
    " :#[S] =Z",
    " [SCH]=S K",
    "[S]C+=",
    "#[SM] =Z AX0 M",
    "#[SM]S =Z AX0 M",
    "#[SN] =Z AX N",
    "[S]=S",
    nullptr,
};
static const char* const LTS_T[] = {
    "[TIALLY]=SH AX3 L IY",
    "[TIAL]=SH AX3 L",
    "[TIOUS]=SH AX3 S",
    "[TT]=T",
    "[T]CH=",
    " [THE] =DH AH",
    "[TO] =T UW",
    "[THAT] =DH AE T",
    " [THIS] =DH IH S",
    " [THEY]=DH EY",
    " [THERE]=DH EH R",
    "[THER]=DH ER",
    "[THEIR]=DH EH R",
    " [THAN] =DH AE N",
    " [THEM] =DH EH M",
    "[THESE] =DH IY Z",
    " [THEN]=DH EH N",
    "[THROUGH]=TH R UW",
    "[THOSE]=DH OW Z",
    "[THOUGH] =DH OW",
    " [THUS]=DH AH S",
    "[TH]=TH",
    "#:[TED] =T IH D",
    "S[TI]#N=CH",
    "[TION]=SH AX3 N",
    "[TIO]=SH",
    "[TIA]=SH",
    "[TIEN]=SH AX N",
    "[TUR]#=CH ER",
    "[TU]A=CH UW",
    " [TWO]=T UW",
    "[T]=T",
    nullptr,
};
static const char* const LTS_U[] = {
    " [UNDER] =AH1 N D ER",
    " [UNDERS] =AH1 N D ER Z",
    " [UNDER]=AH2 N D ER0",
    " [UN]^#=AH0 N",
    " [UN]^^#=AH0 N",
    " [UK] =Y UW K EY",
    " [USA] =Y UW EH S EY",
    " [U]NN=AH",
    " [UN]I=Y UW N",
    " [UN]=AH N",
    " [UPON]=AX P AO N",
    "@[UR]#=UH R",
    "[UR]#=Y UH R",
    "[UR]=ER",
    "[U]^ =AH",
    "[U]^^=AH",
    "[UY]=AY",
    " G[U]#=",
    "G[U]%=",
    "G[U]#=W",
    "#N[U]=Y UW",
    "@[U]=UW",
    "[U]=Y UW",
    nullptr,
};
static const char* const LTS_V[] = {
    "[VIEW]=V Y UW",
    "[V]=V",
    nullptr,
};
static const char* const LTS_W[] = {
    " [WOMEN]=W IH M IX N",
    " [WERE]=W ER",
    "[WA]NT=W AA",
    "[WA]ND=W AA",
    "[WA]TCH=W AA",
    "[WA]SH=W AA",
    "[WA]S=W AA",
    "[WA]T=W AA",
    "[WHERE]=WH EH R",
    "[WHOL]=HH OW L",
    "[WHO]=HH UW",
    "[WH]=WH",
    "[WA]RR=W AO",
    "[WAR]=W AO R",
    "[WOR]^=W ER",
    "[WR]=R",
    "[W]=W",
    nullptr,
};
static const char* const LTS_X[] = {
    "[X]=K S",
    nullptr,
};
static const char* const LTS_Y[] = {
    " [YOUR]=Y AO R",
    " [Y]U=",
    "[YOUNG]=Y AH NG",
    " [YOU]=Y UW",
    " [YES]=Y EH S",
    " [Y] =AY",
    "#^:[Y] =IY",
    "#^:[Y]I=IY",
    " :[Y] =AY",
    " :[Y]#=Y",  // initial Y before vowel = glide (year, yellow, yet)
    " :[Y]^+#=IH",
    " :[Y]^#=AY",
    "[Y]=IH",
    nullptr,
};
static const char* const LTS_Z[] = {
    "[ZZ]=Z",
    "[Z]=Z",
    nullptr,
};

static const char* const* LetterToSoundRulesSource[26] = {
    LTS_A, LTS_B, LTS_C, LTS_D, LTS_E, LTS_F, LTS_G, LTS_H, LTS_I, LTS_J,
    LTS_K, LTS_L, LTS_M, LTS_N, LTS_O, LTS_P, LTS_Q, LTS_R, LTS_S, LTS_T,
    LTS_U, LTS_V, LTS_W, LTS_X, LTS_Y, LTS_Z,
};

static uint8_t LookupPhoneme(const std::string& tok) {
    static constexpr struct { const char* k; uint8_t v; } kTable[] = {
        {"IY",(uint8_t)_IY_},{"IH",(uint8_t)_IH_},{"EH",(uint8_t)_EH_},{"AE",(uint8_t)_AE_},
        {"AA",(uint8_t)_AA_},{"AH",(uint8_t)_AH_},
        {"AO",(uint8_t)_AO_},{"UH",(uint8_t)_UH_},{"AX",(uint8_t)_AX_},{"ER",(uint8_t)_ER_},
        {"EY",(uint8_t)_EY_},{"AY",(uint8_t)_AY_},
        {"OY",(uint8_t)_OY_},{"AW",(uint8_t)_AW_},{"OW",(uint8_t)_OW_},{"UW",(uint8_t)_UW_},
        {"W",(uint8_t)_W_},{"Y",(uint8_t)_Y_},{"R",(uint8_t)_R_},{"L",(uint8_t)_L_},
        {"HH",(uint8_t)_HH_},{"M",(uint8_t)_M_},{"N",(uint8_t)_N_},
        {"NG",(uint8_t)_NG_},{"F",(uint8_t)_F_},{"V",(uint8_t)_V_},
        {"TH",(uint8_t)_TH_},{"DH",(uint8_t)_DH_},{"S",(uint8_t)_S_},{"Z",(uint8_t)_Z_},
        {"SH",(uint8_t)_SH_},{"ZH",(uint8_t)_ZH_},{"P",(uint8_t)_P_},{"B",(uint8_t)_B_},
        {"T",(uint8_t)_T_},{"D",(uint8_t)_D_},
        {"K",(uint8_t)_K_},{"G",(uint8_t)_G_},{"CH",(uint8_t)_CH_},{"JH",(uint8_t)_JH_},
        {"WH",(uint8_t)_W_},
    };
    for (const auto& e : kTable)
        if (tok == e.k) return e.v;
    return (uint8_t)_AX_;
}

std::vector<uint8_t> LetterToSound::ParseOutput(const std::string& s, std::vector<uint8_t>& hints) {
    // Split on ' '; a trailing stress digit on a token becomes its hint. An
    // output holding a stressless (0) or pre-stressing (3) vowel is affix
    // material, so all of its unmarked phonemes are hinted stressless too.
    std::vector<uint8_t> buf;
    hints.clear();
    bool affix = false;
    size_t start = 0;
    while (start < s.size()) {
        size_t end = s.find(' ', start);
        if (end == std::string::npos) {
            end = s.size();
        }
        if (end > start) {
            std::string tok = s.substr(start, end - start);
            uint8_t h = 0;
            char d = tok.back();
            if (d >= '0' && d <= '3') {
                tok.pop_back();
                h = (d == '0') ? 3 : (d == '3') ? 4 : (uint8_t)(d - '0');
                affix |= (d == '0' || d == '3');
            }
            buf.push_back(LookupPhoneme(tok));
            hints.push_back(h);
        }
        start = end + 1;
    }
    if (affix) {
        for (auto& h : hints) {
            if (h == 0) h = 3;
        }
    }
    return buf;
}

LetterToSound::CompiledRule LetterToSound::ParseRule(const char* s) {
    // Splits "LEFT[MATCH]RIGHT=OUTPUT" into its four fields.
    const char* lbr = strchr(s, '[');
    const char* rbr = strchr(s, ']');
    const char* eq  = strchr(rbr + 1, '=');

    std::string left  = (lbr > s) ? std::string(s, lbr - s) : "";
    std::string match = std::string(lbr + 1, rbr - lbr - 1);
    std::string right = std::string(rbr + 1, eq - rbr - 1);
    std::string out   = std::string(eq + 1);

    std::vector<uint8_t> hints;
    std::vector<uint8_t> phons = ParseOutput(out, hints);
    return CompiledRule(std::move(left), std::move(match), std::move(right), std::move(phons), std::move(hints));
}

void LetterToSound::Initialize() {
    if (s_initialized) {
        return;
    }
    s_initialized = true;

    // Build character feature table
    for (const char* p = "AEIOUY"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CV;
    }
    for (const char* p = "EIY"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CF;
    }
    for (const char* p = "BCDFGHJKLMNPQRSTVWXZ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CC;
    }
    for (const char* p = "BDGJLMNRVWZ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CZ;
    }
    for (const char* p = "SCGZXJ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CS;
    }
    for (const char* p = "TSRDLZNJ"; *p; p++) {
        CharacterFeatureTable[(unsigned char)*p] |= CU;
    }

    // Called once at static construction time; converts each rule string to a CompiledRule.
    for (int li = 0; li < 26; li++) {
        const char* const* group = LetterToSoundRulesSource[li];
        for (int ri = 0; group[ri] != nullptr; ri++) {
            CompiledLetterToSoundRules[li].push_back(ParseRule(group[ri]));
        }
    }
}

bool LetterToSound::MatchMid(const std::vector<char>& inp, int pos, const std::string& match, int& end) {
    end = pos;
    for (char m : match) {
        if (end >= (int)inp.size() || inp[end] != m) {
            return false;
        }
        end++;
    }
    return true;
}

bool LetterToSound::IsVowel(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CV) != 0;
}

bool LetterToSound::IsConsonant(const std::vector<char>& inp, int p) {
    if (p < 0 || p >= (int)inp.size()) {
        return false;
    }
    char c = inp[p];
    if ((CharacterFeatureTable[(unsigned char)c] & CC) != 0) {
        return true;
    }
    if ((c == 'Q' || c == 'G') && p + 1 < (int)inp.size() && inp[p + 1] == 'U') {
        return true;
    }
    return false;
}

bool LetterToSound::IsVoiced(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CZ) != 0;
}

bool LetterToSound::IsUMod(const std::vector<char>& inp, int p) {
    return p >= 0 && p < (int)inp.size() && (CharacterFeatureTable[(unsigned char)inp[p]] & CU) != 0;
}

bool LetterToSound::MatchSibilant(const std::vector<char>& inp, int& pos, int dir) {
    if (pos < 0 || pos >= (int)inp.size()) {
        return false;
    }
    char c = inp[pos];
    if ((CharacterFeatureTable[(unsigned char)c] & CS) != 0) {
        pos += dir;
        return true;
    }
    if (dir == 1 && (c == 'C' || c == 'S') && pos + 1 < (int)inp.size() && inp[pos + 1] == 'H') {
        pos += 2;
        return true;
    }
    if (dir == -1 && c == 'H' && pos > 0 && (inp[pos - 1] == 'C' || inp[pos - 1] == 'S')) {
        pos -= 2;
        return true;
    }
    return false;
}

bool LetterToSound::MatchSuffix(const std::vector<char>& inp, int pos, int& end) {
    end = pos;
    if (pos < 0 || pos >= (int)inp.size()) {
        return false;
    }
    char c = inp[pos];
    if (c == 'E') {
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'R') {
            end = pos + 2;
            return true;
        }
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'D') {
            end = pos + 2;
            return true;
        }
        if (pos + 2 < (int)inp.size() && inp[pos + 1] == 'S') {
            end = pos + 2;
            return true;
        }
        if (pos + 3 < (int)inp.size() && inp[pos + 1] == 'L' && inp[pos + 2] == 'Y') {
            end = pos + 3;
            return true;
        }
        if (pos + 1 < (int)inp.size() && inp[pos + 1] == ' ') {
            end = pos + 1;
            return true;
        }
        return false;
    }
    if (c == 'I' && pos + 3 < (int)inp.size() && inp[pos + 1] == 'N' && inp[pos + 2] == 'G') {
        end = pos + 3;
        return true;
    }
    return false;
}

// Recursive context matcher with backtracking for #, *, :
// dir=+1: left-to-right (right context), ci advances forward
// dir=-1: right-to-left (left context), ci retreats toward -1
bool LetterToSound::MatchCtx(const std::vector<char>& inp, int pos, const std::string& ctx, int ci, int dir) {
    int cEnd = (dir == 1) ? (int)ctx.size() : -1;
    if (ci == cEnd) {
        return true;
    }

    char sym = ctx[ci];
    int nci = ci + dir;

    switch (sym) {
        case '#': // one or more vowels
            if (!IsVowel(inp, pos)) {
                return false;
            }
            pos += dir;
            while (true) {
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
                if (!IsVowel(inp, pos)) {
                    return false;
                }
                pos += dir;
            }

        case '*': // one or more consonants
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            pos += dir;
            while (true) {
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
                if (!IsConsonant(inp, pos)) {
                    return false;
                }
                pos += dir;
            }

        case ':': // zero or more consonants
            if (MatchCtx(inp, pos, ctx, nci, dir)) {
                return true;
            }
            while (IsConsonant(inp, pos)) {
                pos += dir;
                if (MatchCtx(inp, pos, ctx, nci, dir)) {
                    return true;
                }
            }
            return false;

        case '^': // exactly one consonant
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '+': // one front vowel (E I Y)
            if (pos < 0 || pos >= (int)inp.size() || (CharacterFeatureTable[(unsigned char)inp[pos]] & CF) == 0) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '.': // one voiced consonant
            if (!IsVoiced(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '&': // one sibilant
        {
            int p2 = pos;
            if (!MatchSibilant(inp, p2, dir)) {
                return false;
            }
            return MatchCtx(inp, p2, ctx, nci, dir);
        }

        case '@': // one long-U consonant
            if (!IsUMod(inp, pos)) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case '%': // suffix (right context only): ER, E, ES, ED, ING, ELY
            {
                int sfxEnd = 0;
                if (!MatchSuffix(inp, pos, sfxEnd)) {
                    return false;
                }
                return MatchCtx(inp, sfxEnd, ctx, nci, dir);
            }

        case '$': // one consonant followed by E or I
            if (!IsConsonant(inp, pos)) {
                return false;
            }
            pos += dir;
            if (pos < 0 || pos >= (int)inp.size() || (CharacterFeatureTable[(unsigned char)inp[pos]] & CF) == 0) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        case ' ': // word boundary
            if (pos < 0 || pos >= (int)inp.size() || inp[pos] != ' ') {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);

        default: // literal character
            if (pos < 0 || pos >= (int)inp.size() || inp[pos] != sym) {
                return false;
            }
            return MatchCtx(inp, pos + dir, ctx, nci, dir);
    }
}

std::vector<uint8_t> LetterToSound::Convert(const std::string& word) {
    std::vector<uint8_t> hints;
    return Convert(word, hints, nullptr);
}

std::vector<uint8_t> LetterToSound::Convert(const std::string& word, std::vector<uint8_t>& hints,
                                            std::vector<RuleHit>* hits) {
    hints.clear();
    if (word.empty()) {
        return {};
    }

    Initialize();

    // Pad: leading space (word boundary) + word uppercase + two trailing spaces
    std::vector<char> inp(word.size() + 3);
    inp[0] = ' ';
    for (int i = 0; i < (int)word.size(); i++) {
        inp[i + 1] = (char)toupper((unsigned char)word[i]);
    }
    inp[word.size() + 1] = ' ';
    inp[word.size() + 2] = ' ';

    std::vector<uint8_t> phons;
    phons.reserve(word.size() * 2);
    int pos = 1;

    while (inp[pos] != ' ') {
        char c = inp[pos];
        if (c == '\'' || c == '.') {
            pos++;
            continue;
        }
        int li = c - 'A';
        if (li < 0 || li >= 26) {
            pos++;
            continue;
        }

        bool matched = false;
        const auto& group = CompiledLetterToSoundRules[li];
        for (int ri = 0; ri < (int)group.size(); ri++) {
            const auto& rule = group[ri];
            int endPos = 0;
            if (!MatchMid(inp, pos, rule.Match, endPos)) {
                continue;
            }
            if (!MatchCtx(inp, pos - 1, rule.Left, (int)rule.Left.size() - 1, -1)) {
                continue;
            }
            if (!MatchCtx(inp, endPos, rule.Right, 0, +1)) {
                continue;
            }

            phons.insert(phons.end(), rule.Out.begin(), rule.Out.end());
            hints.insert(hints.end(), rule.Hint.begin(), rule.Hint.end());
            if (hits) {
                hits->push_back({(uint8_t)li, (uint16_t)ri, (uint8_t)(pos - 1), (uint8_t)(endPos - 1)});
            }
            pos = endPos;
            matched = true;
            break;
        }
        if (!matched) {
            pos++;
        }
    }

    return phons;
}

bool LetterToSound::IsVowelPhon(uint8_t p) {
    // _IY_.._UR_ (0..22) are vocalic nuclei; _SIL_ = 23 and up are not.
    return p <= 22;
}

bool LetterToSound::IsLongVowelPhon(uint8_t p) {
    // Tense/long vowels per MITalk 6.3: ey iy ay ow uw oy aw (plus yu).
    // r-colored vowels (er + ir/xr/ar/or/ur) are treated as heavy so a syllable
    // built on them never counts as "weak". Everything else (ih eh ae ix ax ah
    // aa ao uh) is short.
    switch (p) {
        case _IY_: case _EY_: case _AY_: case _OY_: case _AW_:
        case _OW_: case _UW_: case _YU_:
        case _ER_: case _IR_: case _XR_: case _AR_: case _OR_: case _UR_:
            return true;
        default:
            return false;
    }
}

uint8_t LetterToSound::ShortenVowel(uint8_t p) {
    // 6.3.5 tenseness reduction table. oy and aw are left long per MITalk.
    switch (p) {
        case _EY_: return _AE_;
        case _IY_: return _EH_;
        case _AY_: return _IH_;
        case _OW_: return _AA_;
        case _UW_: return _UH_;
        default:   return p;
    }
}

uint8_t LetterToSound::ReduceVowel(uint8_t p) {
    // Only short vowels reduce; long/tense/r-colored and already-reduced ix/ax pass.
    if (IsLongVowelPhon(p)) {
        return p;
    }
    switch (p) {
        case _EH_: case _IH_: return _IX_;
        case _IX_: case _AX_: return p;
        default:              return _AX_;
    }
}

void LetterToSound::BuildStressSyls(const std::vector<uint8_t>& phons, std::vector<StressSyl>& out,
                                    const std::vector<uint8_t>* hints) {
    // A syllable is a vowel nucleus plus the consonants that follow it up to
    // (not including) the next vowel. coda = that consonant count. With hints,
    // stressless affix material neither forms a syllable nor counts as coda.
    auto affix = [&](int i) { return hints && (*hints)[i] >= 3; };
    for (int i = 0; i < (int)phons.size(); i++) {
        if (!IsVowelPhon(phons[i]) || affix(i)) {
            continue;
        }
        StressSyl s;
        s.idx = i;
        s.isLong = IsLongVowelPhon(phons[i]);
        s.coda = 0;
        int j = i + 1;
        for (; j < (int)phons.size() && !IsVowelPhon(phons[j]) && !affix(j); j++) {
            s.coda++;
        }
        // A stop or f plus a liquid before the next vowel is its onset (cele-brate).
        for (; j < (int)phons.size() && !IsVowelPhon(phons[j]); j++) {
        }
        if (s.coda >= 2 && j < (int)phons.size() && IsOnsetCluster(phons[i + s.coda - 1], phons[i + s.coda])) {
            s.coda--;
        }
        out.push_back(s);
    }
}

bool LetterToSound::IsOnsetCluster(uint8_t a, uint8_t b) {
    switch (a) {
        case _P_: case _B_: case _K_: case _G_: case _F_: return b == _R_ || b == _L_;
        case _T_: case _D_: case _TH_:                    return b == _R_;
        default:                                          return false;
    }
}

int LetterToSound::MainStressSyl(const std::vector<StressSyl>& syls, bool suffixed) {
    int n = (int)syls.size();
    if (n == 0) {
        return -1;
    }
    if (n == 1) {
        return 0;  // Rule 3 on a monosyllable: its only vowel.
    }

    const StressSyl& ult = syls[n - 1];
    const StressSyl& pen = syls[n - 2];
    // The ultima is "light" when it has a short vowel (any coda) or is an open
    // final syllable (a bare morph-final vowel).
    bool ultLight = !ult.isLong || ult.coda == 0;
    if (suffixed) {
        ultLight = ult.coda == 0 || (!ult.isLong && ult.coda <= 1);
    }
    // The penult qualifies for antepenult stress when it is a weak syllable
    // (short vowel, at most one coda consonant) or an open syllable.
    bool penWeakOrOpen = pen.coda == 0 || (!pen.isLong && pen.coda <= 1);

    // Rule 1: antepenult stress (difficult, oregano, secretariat, oratorio).
    if (n >= 3 && penWeakOrOpen && ultLight) {
        return n - 3;
    }
    // Rule 2: penult stress (edit, agenda, bitumen).
    if (ultLight) {
        return n - 2;
    }
    // Rule 3: last-syllable stress (parole, cascade, stand).
    return n - 1;
}

int LetterToSound::MainStressVowelIndex(const std::vector<uint8_t>& phons) {
    std::vector<StressSyl> syls;
    BuildStressSyls(phons, syls);
    int m = MainStressSyl(syls);
    return m < 0 ? -1 : syls[m].idx;
}

std::vector<uint8_t> LetterToSound::AssignStress(std::vector<uint8_t>& phons) {
    return AssignStress(phons, std::vector<uint8_t>(phons.size(), 0));
}

std::vector<uint8_t> LetterToSound::AssignStress(std::vector<uint8_t>& phons, const std::vector<uint8_t>& hints) {
    std::vector<uint8_t> marks(phons.size(), 0);
    std::vector<StressSyl> syls;
    BuildStressSyls(phons, syls, &hints);
    if (syls.empty()) {
        // Every vowel is affix material; fall back to treating them as stem.
        BuildStressSyls(phons, syls);
    }
    int n = (int)syls.size();
    int primary = -1;
    for (int k = 0; k < n && primary < 0; k++) {
        if (hints[syls[k].idx] == 1) {
            primary = k;
        }
    }
    bool forced = primary >= 0;
    // A pre-stressing affix (-tion, -ity) puts the primary on the syllable before it.
    for (int i = (int)phons.size() - 1; i >= 0 && !forced; i--) {
        if (hints[i] == 4) {
            for (int k = n - 1; k >= 0 && !forced; k--) {
                if (syls[k].idx < i) {
                    primary = k;
                    forced = true;
                }
            }
            break;
        }
    }
    if (!forced) {
        bool suffixed = false;
        for (int i = n ? syls[n - 1].idx + 1 : 0; n && i < (int)phons.size(); i++) {
            suffixed |= hints[i] >= 3 && IsVowelPhon(phons[i]);
        }
        primary = MainStressSyl(syls, suffixed);
    }
    if (primary < 0) {
        return marks;
    }
    // 6.3.2 -ic (ih k) attracts primary to the penult: symBOLic, teleGRAPHic.
    // Penult is the productive SPE behavior; antepenult exceptions (catholic) are
    // lexical and out of scope for a fallback. The paper text reads "first syllable".
    if (n >= 2 && !forced) {
        const StressSyl& last = syls[n - 1];
        if (phons[last.idx] == _IH_ && last.coda == 1 && phons[last.idx + 1] == _K_) {
            primary = n - 2;
        }
    }
    marks[syls[primary].idx] = 1;
    for (int i = 0; i < (int)phons.size(); i++) {
        if (hints[i] == 2 && IsVowelPhon(phons[i]) && marks[i] == 0) {
            marks[i] = 2;
        }
    }

    // 6.3.5 Destressing Rule 1: shorten a non-first-syllable long vowel one
    // consonant before the stress (instrumental: uw -> uh). Those vowels are
    // already unmarked here, so only the shortening shows; Rule 2 is a no-op.
    for (int k = 1; k + 1 < n; k++) {
        if (marks[syls[k].idx] != 0 || hints[syls[k].idx] != 0) {
            continue;
        }
        if (syls[k].coda == 1 && marks[syls[k + 1].idx] != 0) {
            phons[syls[k].idx] = ShortenVowel(phons[syls[k].idx]);
        }
    }

    // 6.3.7 Strong First Syllable: secondary on a non-primary first syllable when
    // heavy (long vowel or coda >= 2: dielectric, circum-), or when the primary is
    // on syllable 3+ so an alternating beat fits (telegraphic, ptolemaic). A light
    // syllable right before the primary stays reduced (photography, banana).
    if (n >= 2 && primary != 0 && hints[syls[0].idx] == 0) {
        const StressSyl& first = syls[0];
        if (first.isLong || first.coda >= 2 || primary >= 2) {
            marks[first.idx] = 2;
        }
    }

    // 6.3.9 Vowel Reduction: every remaining unstressed short vowel goes to schwa
    // (eh/ih -> ix, else -> ax). Runs last, on the final marks. Hinted vowels keep
    // the quality their rule wrote.
    for (int i = 0; i < (int)phons.size(); i++) {
        if (marks[i] == 0 && hints[i] == 0 && IsVowelPhon(phons[i])) {
            phons[i] = ReduceVowel(phons[i]);
        }
    }
    // A schwa before r past the first phoneme merges into r-colored schwa (inform-a-tion, memory).
    for (int i = (int)phons.size() - 2; i > 0; i--) {
        if (marks[i] == 0 && hints[i] == 0 && (phons[i] == _AX_ || phons[i] == _IX_) && phons[i + 1] == _R_) {
            phons[i] = _ER_;
            phons.erase(phons.begin() + i + 1);
            marks.erase(marks.begin() + i + 1);
        }
    }
    return marks;
}

}  // namespace SharpVox
