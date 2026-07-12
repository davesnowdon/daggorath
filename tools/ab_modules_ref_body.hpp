#include <cstdio>
#include <cstdlib>
#include <string>
typedef unsigned char dodBYTE;
typedef unsigned short dodSHORT;
typedef unsigned int Uint32;
static Uint32 SDL_GetTicks() { return 0; }
struct Utils {
	template<class T> static void LoadFromHex(T * b, std::string h){
		char hexbuf[3];
		char * end;
		hexbuf[2] = 0;
		int ctr = 0;
		auto hiter = h.begin();
		while (hiter != h.end())
		{
			hexbuf[0] = *hiter;
			hexbuf[1] = *(++hiter);
			*(b + ctr) = (dodBYTE) strtoul(hexbuf, &end, 16);
			++ctr;
			hiter++;
		}
	}
	template<class T> static void LoadFromDecDigit(T * b, std::string dd){
		auto dditer = dd.begin();
		while (dditer != dd.end()){
			*b++ = (*dditer++ - '0');
		}
	}
};
typedef struct Mix_Chunk Mix_Chunk;	/* stub: declared members only, never used */
class RowCol
{
public:
	RowCol() : row(0), col(0) {}
	RowCol(dodBYTE r, dodBYTE c) : row(r), col(c) {}
	RowCol(int idx) : row(idx/32), col(idx%32) {}
	void setRC(dodBYTE r, dodBYTE c) { row=r; col=c; }
	dodBYTE row;
	dodBYTE col;
};
/* --- data block classes pasted VERBATIM from the port's dod.h --- */
// Attack Block
// Creatures and players use the same algorithm
// for attacking and for damage infliction.  These
// values are the common ones used.
class ATB
{
public:
	// Fields
	dodSHORT	P_ATPOW;
	dodBYTE		P_ATMGO;
	dodBYTE		P_ATMGD;
	dodBYTE		P_ATPHO;
	dodBYTE		P_ATPHD;
	dodSHORT	P_ATXX1;
	dodSHORT	P_ATXX2;
	dodSHORT	P_ATDAM;
};

// Creature control block
// Holds all the data for a particular creature.
class CCB
{
public:
	// Constructor
	CCB()
	{ clear(); }

	void clear()
	{
	P_CCPOW = 0;
	P_CCMGO = 0;
	P_CCMGD = 0;
	P_CCPHO = 0;
	P_CCPHD = 0;
	P_CCTMV = 0;
	P_CCTAT = 0;
	P_CCOBJ = -1;
	P_CCDAM = 0;
	P_CCUSE = 0;
	creature_id = 0;
	P_CCDIR = 0;
	P_CCROW = 0;
	P_CCCOL = 0;
	}

	// Fields
	dodSHORT	P_CCPOW;
	dodBYTE		P_CCMGO;
	dodBYTE		P_CCMGD;
	dodBYTE		P_CCPHO;
	dodBYTE		P_CCPHD;
	int			P_CCTMV;
	int			P_CCTAT;
	int			P_CCOBJ;
	dodSHORT	P_CCDAM;
	dodBYTE		P_CCUSE;
	dodBYTE		creature_id;
	dodBYTE		P_CCDIR;
	dodBYTE		P_CCROW;
	dodBYTE		P_CCCOL;
};

// Creature definition block
// Holds the data for a creature type.
class CDB
{
public:
	// Constructors
	CDB(dodSHORT pow, dodBYTE mgo, dodBYTE mgd,
		dodBYTE pho, dodBYTE phd, int tmv,
		int tat)
		: P_CDPOW(pow), P_CDMGO(mgo), P_CDMGD(mgd),
		  P_CDPHO(pho), P_CDPHD(phd), P_CDTMV(tmv),
		  P_CDTAT(tat)
	{}

	CDB()
		: P_CDPOW(0), P_CDMGO(0), P_CDMGD(0),
		  P_CDPHO(0), P_CDPHD(0), P_CDTMV(0),
		  P_CDTAT(0)
	{}

	// Fields
	dodSHORT	P_CDPOW;
	dodBYTE		P_CDMGO;
	dodBYTE		P_CDMGD;
	dodBYTE		P_CDPHO;
	dodBYTE		P_CDPHD;
	int			P_CDTMV;
	int			P_CDTAT;
};

// Object control block
// Hold the data for a particular object.
class OCB
{
public:
	// Default constructor
	OCB()
		{ clear(); }

	void clear()
	{
		P_OCPTR = -1;
		P_OCROW = 0;
		P_OCCOL = 0;
		P_OCLVL = 0;
		P_OCOWN = 0;
		P_OCXX0 = 0;
		P_OCXX1 = 0;
		P_OCXX2 = 0;
		obj_id = 0;
		obj_type = 0;
		obj_reveal_lvl = 0;
		P_OCMGO = 0;
		P_OCPHO = 0;
	}

	// Fields
	int			P_OCPTR;
	dodBYTE		P_OCROW;
	dodBYTE		P_OCCOL;
	dodBYTE		P_OCLVL;
	dodBYTE		P_OCOWN;
	dodSHORT	P_OCXX0;
	dodSHORT	P_OCXX1;
	dodSHORT	P_OCXX2;
	dodBYTE		obj_id;
	dodBYTE		obj_type;
	dodBYTE		obj_reveal_lvl;
	dodBYTE		P_OCMGO;
	dodBYTE		P_OCPHO;
};

// Object definition block
// Used for constructing specific objects.
class ODB
{
public:
	// Constructors
	ODB(dodBYTE cls, dodBYTE rev, dodBYTE mgo,
		dodBYTE pho)
		: P_ODCLS(cls), P_ODREV(rev), P_ODMGO(mgo),
		  P_ODPHO(pho)
	{}

	ODB()
		: P_ODCLS(0), P_ODREV(0), P_ODMGO(0),
		  P_ODPHO(0)
	{}

	// Fields
	dodBYTE		P_ODCLS;
	dodBYTE		P_ODREV;
	dodBYTE		P_ODMGO;
	dodBYTE		P_ODPHO;
};

// Extra definition block
// Holds extra data for torches, rings, and shields:
// torch timers, ring shot counters and incantation indices,
// and shield magical and physical defense values.
class XDB
{
public:
	// Constructors
	XDB(int idx, dodSHORT x0, dodSHORT x1,
		dodSHORT x2)
		: P_OXIDX(idx), P_OXXX0(x0), P_OXXX1(x1),
		  P_OXXX2(x2)
	{}

	XDB()
		: P_OXIDX(-1), P_OXXX0(0), P_OXXX1(0),
		  P_OXXX2(0)
	{}

	// Fields
	int			P_OXIDX;
	dodSHORT	P_OXXX0;
	dodSHORT	P_OXXX1;
	dodSHORT	P_OXXX2;
};

// The new Task class for use in the rewritten
// Scheduler algorithm.  Not all fields are being
// used currently.  They may go away, if the current
// algorthim tests well, otherwise, they may be used
// for increasing the accuracy of the scheduler.
struct Task
{
public:
	int		type;		// One of the seven task types
	int		data;		// Stores creatures ID
	Uint32	frequency;	// in milliseconds
	Uint32	prev_time;	// previous execution timestamp
	Uint32	next_time;	// next scheduled execution timestamp
	long	count;		// number of times executed

	Task()
		{ clear(); }

	// Convenience Setter
	void setValues(int t, int d, long f, long p, long n, long c, bool e)
	{
		type = t;
		data = d;
		frequency = f;
		prev_time = p;
		next_time = n;
		count = c;
	}

	void clear()
	{
		type = -1;
		data = -1;
		frequency = 0;
		prev_time = 0;
		next_time = 0;
		count = 0;
	}
};
/* --- end verbatim dod.h data classes --- */

struct GameStub { dodBYTE LEVEL; bool RandomMaze; bool IsDemo;
                  bool ShieldFix; bool VisionScroll;
                  bool CreaturesIgnoreObjects; } game;
/* Player stub: PROW/PCOL plus just enough state for the two verbatim
 * pure members (DAMAGE, and the HEARTR formula line from HUPDAT). */
struct Player {
	dodBYTE PROW, PCOL;
	ATB PLRBLK;
	dodBYTE HEARTR;
	bool DAMAGE(int AP, int AMO, int APO,
	            int DP, int DMD, int DPD, dodSHORT * DD);
	void HUPDAT_heartr();	/* stub wrapper; body line is verbatim */
} player;
/* Scheduler stub: only what NEWLVL/CBIRTH touch.  The SYSTCB and GETTCB
 * bodies below are the port's sched.cpp code VERBATIM; TCBPTR is public
 * here (private in the port) so the harness can CRC it. */
class Scheduler
{
public:
	void		SYSTCB();
	int			GETTCB();

	Task	TCBLND[38];

	enum { // task IDs
		TID_CLOCK		= 0,
		TID_PLAYER		= 1,
		TID_REFRESH_DISP = 2,
		TID_HRTSLOW		= 3,
		TID_TORCHBURN	= 4,
		TID_CRTREGEN	= 5,
		TID_CRTMOVE		= 6,
	};

	Uint32		curTime;
	int			TCBPTR;
};
Scheduler scheduler;

// Creates initial Task Blocks
void Scheduler::SYSTCB()
{
	int	ctr;
	int	TCBindex;

	for (ctr = 0; ctr < 38; ++ctr)
	{
		TCBLND[ctr].clear();
	}

	TCBPTR = 0;

	TCBLND[0].type = TID_CLOCK;
	TCBLND[0].frequency = 17;		// One JIFFY
	TCBindex = GETTCB();

	TCBLND[1].type = TID_PLAYER;
	TCBLND[1].frequency = 17;		// One JIFFY
	TCBindex = GETTCB();

	TCBLND[2].type = TID_REFRESH_DISP;
	TCBLND[2].frequency = 300;		// Three TENTHs
	TCBindex = GETTCB();

	TCBLND[3].type = TID_HRTSLOW;
	TCBindex = GETTCB();

	TCBLND[4].type = TID_TORCHBURN;
	TCBLND[4].frequency = 5000;		// Five Seconds
	TCBindex = GETTCB();

	TCBLND[5].type = TID_CRTREGEN;
	TCBLND[5].frequency = 300000;	// Five minutes
	TCBindex = GETTCB();
}

// Gets next available Task Block and updates the index
int Scheduler::GETTCB()
{
	++TCBPTR;
	return (TCBPTR - 1);
}

struct ViewerStub { void OUTSTI(dodBYTE*) {} void setVidInv(bool) {} } viewer;
// This class is a port of Daggorath's custom Random Number Generator
class RNG
{
public:
	// Constructor
	RNG()
	{
		carry = 0;
		SEED[0] = 0;
		SEED[1] = 0;
		SEED[2] = 0;
	}

	// Accessors
	dodBYTE RANDOM()
	{
		int x, y;
		dodBYTE a, b;
		carry = 0;
		for (x = 8; x != 0; --x)
		{
			b = 0;
			a = (SEED[2] & 0xE1);
			for (y = 8; y != 0; --y)
			{
				a = lsl(a);
				if (carry != 0)
					++b;
			}
			b = lsr(b);
			SEED[0] = rol(SEED[0]);
			SEED[1] = rol(SEED[1]);
			SEED[2] = rol(SEED[2]);
		}
		return SEED[0];
	}

	dodBYTE getSEED(int idx)
	{
		return SEED[idx];
	}

	// Mutators
	void setSEED(int idx, dodBYTE val)
	{
		SEED[idx] = val;
	}

	void setSEED(dodBYTE val0, dodBYTE val1, dodBYTE val2)
	{
		SEED[0] = val0;
		SEED[1] = val1;
		SEED[2] = val2;
	}

	dodBYTE SEED[3];
	dodBYTE carry;

private:
	// Internal Implementation
	dodBYTE lsl(dodBYTE c)
	{
		carry = (((c & 128) == 128) ? 1 : 0);
		return c << 1;
	}

	dodBYTE lsr(dodBYTE c)
	{
		carry = (((c & 1) == 1) ? 1 : 0);
		return c >> 1;
	}

	dodBYTE rol(dodBYTE c)
	{
		dodBYTE cry;
		cry = (((c & 128) == 128) ? 1 : 0);
		c <<= 1;
		c += carry;
		carry = cry;
		return c;
	}
};
RNG rng;
/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: parser.h
//
// This class will manage the command parser



class Parser
{
public:
	// Constructor
	Parser();

	// Public Interface
	void	KBDPUT(dodBYTE c);
	dodBYTE	KBDGET();
	void	EXPAND(dodBYTE * X, int * Xup, dodBYTE * U);
	dodBYTE	GETFIV(dodBYTE * X, int * Xup, dodBYTE * zeroY);
	void	ASRD(dodBYTE & A, dodBYTE & B, int num);
	bool	GETTOK();
	int		PARSER(dodBYTE * X, dodBYTE &A, dodBYTE &B, bool norm);
	void	CMDERR();
	int		PARHND();
	void	Reset();

	// Public Data Member
	dodSHORT	LINPTR;
	dodBYTE		PARFLG;
	dodBYTE		PARCNT;
	dodBYTE		VERIFY;
	dodBYTE		FULFLG;
	dodBYTE		KBDHDR;
	dodBYTE		KBDTAL;
	dodBYTE		BUFFLG;
	dodBYTE		KBDBUF[33];
	dodBYTE		LINBUF[33];
	dodSHORT	LINEND;
	dodBYTE		TOKEN[33];
	dodBYTE		TOKEND;
	dodBYTE		STRING[35];
	dodBYTE		SWCHAR[11];
	dodBYTE		OBJSTR[33];
	dodBYTE		CMDTAB[69];
	dodBYTE		DIRTAB[26];

	enum {
		C_BS=0x08,
		C_CR=0x0D,
		C_SP=0x20,

		I_SP=0x00,
		I_BAR=0x1C,
		I_DOT=0x1E,
		I_CR=0x1F,
		I_EXCL=0x1B,
		I_QUES=0x1D,
		I_SHL=0x20,
		I_SHR=0x21,
		I_LHL=0x22,
		I_LHR=0x23,
		I_BS=0x24,
		I_NULL=0xff, // string terminator char

		CMD_ATTACK=0,
		CMD_CLIMB,
		CMD_DROP,
		CMD_EXAMINE,
		CMD_GET,
		CMD_INCANT,
		CMD_LOOK,
		CMD_MOVE,
		CMD_PULL,
		CMD_REVEAL,
		CMD_STOW,
		CMD_TURN,
		CMD_USE,
		CMD_ZLOAD,
		CMD_ZSAVE,

		DIR_LEFT=0,
		DIR_RIGHT,
		DIR_BACK,
		DIR_AROUND,
		DIR_UP,
		DIR_DOWN,
	};
	
	dodBYTE M_PROM1[5];
	dodBYTE M_CURS[3];
	dodBYTE M_ERAS[6];
	dodBYTE CERR[3];

private:
};

Parser parser;
/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: parser.cpp
//
// Implementation of Parser class



// Constructor
Parser::Parser() : LINPTR(0),
				   PARFLG(0),
				   PARCNT(0),
				   VERIFY(0),
				   FULFLG(0),
				   KBDHDR(0),
				   KBDTAL(0),
				   BUFFLG(0),
				   LINEND(0),
				   TOKEND(0)
{
	int ctr;
	for (ctr = 0; ctr < 33; ++ctr)
	{
		KBDBUF[ctr] = 0;
		LINBUF[ctr] = 0;
		TOKEN[ctr] = 0;
		OBJSTR[ctr] = 0;
		STRING[ctr] = 0;
	}
	STRING[33] = 0;
	STRING[34] = 0;
	for (ctr = 0; ctr < 11; ++ctr)
	{
		SWCHAR[ctr] = 0;
	}

	M_PROM1[0] = I_CR;
	M_PROM1[1] = I_DOT;
	M_PROM1[2] = I_BAR;
	M_PROM1[3] = I_BS;
	M_PROM1[4] = I_NULL;

	M_CURS[0] = I_BAR;
	M_CURS[1] = I_BS;
	M_CURS[2] = I_NULL;

	M_ERAS[0] = I_SP;
	M_ERAS[1] = I_BS;
	M_ERAS[2] = I_BS;
	M_ERAS[3] = I_BAR;
	M_ERAS[4] = I_BS;
	M_ERAS[5] = I_NULL;

	Utils::LoadFromHex(CERR, "177BD0");
	Utils::LoadFromHex(CMDTAB, "0F30034A046B2806C4B440200927C0380B80B52E28180E5A003012E185D42018F7AC201AFB142021563030245B142C202747DC20295938182B32802834C78480283530D8A0");
	Utils::LoadFromHex(DIRTAB, "0620185350282493A280200411AC300327D5C4102B002008FBB8");

/*	Utils::LoadFromHex(CERR,"177BD0");

	Utils::LoadFromHex(CMDTAB, 
		"0F30034A046B2806C4B440200927C0380B80B52E28180E5A003012E185D42018F7AC20"
		"1AFB142021563030245B142C202747DC20295938182B32802834C78480283530D8A0"
		);

	Utils::LoadFromHex(DIRTAB,
		"0620185350282493A280200411AC300327D5C4102B002008FBB8");*/
}

void Parser::Reset()
{
	LINPTR = 0;
	PARFLG = 0;
	PARCNT = 0;
	VERIFY = 0;
	FULFLG = 0;
	KBDHDR = 0;
	KBDTAL = 0;
	BUFFLG = 0;
	LINEND = 0;
	TOKEND = 0;
	int ctr;
	for (ctr = 0; ctr < 33; ++ctr)
	{
		KBDBUF[ctr] = 0;
		LINBUF[ctr] = 0;
		TOKEN[ctr] = 0;
		OBJSTR[ctr] = 0;
		STRING[ctr] = 0;
	}
	STRING[33] = 0;
	STRING[34] = 0;
	for (ctr = 0; ctr < 11; ++ctr)
	{
		SWCHAR[ctr] = 0;
	}
}

// This method puts a character into the DoD buffer
void Parser::KBDPUT(dodBYTE c)
{
	KBDBUF[KBDTAL] = c;
	++KBDTAL;
	KBDTAL &= 31;
}

// This method gets a character from the DoD buffer
dodBYTE Parser::KBDGET()
{
	dodBYTE c = 0;
	if (KBDHDR == KBDTAL)
		return c;
	c = KBDBUF[KBDHDR];
	++KBDHDR;
	KBDHDR &= 31;
	return c;
}

// The rest of these methods are direct ports from the source,
// including all the GOTOs.  Someday, these should probably be
// updated to a more C/C++ programming style, but for the moment
// they work just fine.
//
int Parser::PARSER(dodBYTE * pTABLE, dodBYTE & A, dodBYTE & B, bool norm)
{
	bool	tok;
	int		U, Xup, Y;
	dodBYTE	retA, retB;

	if (norm)
	{
		A = 0;
		B = 0;
		tok = GETTOK();
		if (tok == false)
		{
			return 0;
		}
	}
	else
	{
		A = 0;
	}

	PARFLG = 0;
	FULFLG = 0;
	B = *pTABLE;
	++pTABLE;
	PARCNT = B;

PARS10:
	U = 0;
	EXPAND(pTABLE, &Xup, 0);
	pTABLE += Xup;
	Y = 2;
	
PARS12:
	B = TOKEN[U++];
	if (B == 0xFF)
	{
		goto PARS20;
	}
	if (B != STRING[Y++])
	{
		goto PARS30;
	}
	if (STRING[Y] != I_NULL && STRING[Y] != 0)
	{
		goto PARS12;
	}
	if (TOKEN[U] != 0xFF && TOKEN[U] != 0)
	{
		goto PARS30;
	}
	--FULFLG;

PARS20:
	if (PARFLG != 0)
	{
		goto PARS90;
	}
	++PARFLG;
	B = STRING[1];
	retA = A;
	retB = B;

PARS30:
	++A;
	--PARCNT;
	if (PARCNT != 0)
	{
		goto PARS10;
	}

	if (PARFLG != 0)
	{
		A = retA;
		B = retB;
		return 1;
	}

PARS90:
	A = 0xFF;
	B = 0xFF;
	return -1;
}

bool Parser::GETTOK()
{
	int		U = 0;
	int		X = LINPTR;
	dodBYTE	A;
	
	do
	{
		A = LINBUF[X++];
	} while (A == 0);
	goto GTOK22;

GTOK20:
	A = LINBUF[X++];

GTOK22:
	if (A == 0 || A == 0xFF)
	{
		goto GTOK30;
	}
	TOKEN[U++] = A;
	if (U < 32)
	{
		goto GTOK20;
	}

GTOK30:
	TOKEN[U++] = 0xFF;
	LINPTR = X;

	if (TOKEN[0] == 0xFF)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void Parser::EXPAND(dodBYTE * X, int * Xup, dodBYTE * U)
{
	dodBYTE * Y;
	dodBYTE	A, B;
	int Xup2;

	*Xup = 0;

	if (U != 0)
	{
		Y = (U - 1);
	}
	else
	{
		Y = &STRING[0];
		U = Y + 1;
	}
	*Y = 0;
	B = GETFIV(X, &Xup2, Y);
	X += Xup2;
	A = B;

EXPAN10:
	B = GETFIV(X, &Xup2, Y);
	X += Xup2;
	*Xup += Xup2;
	*U = B;
	++U;
	--A;
	if (A != 0xFF)
	{
		goto EXPAN10;
	}
	*U = A;

	if ( (*Y) != 0)
	{
		++X;
		++*Xup;
	}
}

dodBYTE Parser::GETFIV(dodBYTE * X, int * Xup, dodBYTE * zeroY)
{
	dodBYTE		A, B;
	
	*Xup = 0;

	A = *zeroY;

	switch (A)
	{
	case 0:
		B = *X;
		B = (B >> 3);
		break;
	case 1:
		A = *X;
		++X;
		++*Xup;
		B = *X;
		ASRD(A, B, 6);
		break;
	case 2:
		B = *X;
		B = (B >> 1);
		break;
	case 3:
		A = *X;
		++X;
		++*Xup;
		B = *X;
		ASRD(A, B, 4);
		break;
	case 4:
		A = *X;
		++X;
		++*Xup;
		B = *X;
		ASRD(A, B, 7);
		break;
	case 5:
		B = *X;
		B = (B >> 2);
		break;
	case 6:
		A = *X;
		++X;
		++*Xup;
		B = *X;
		ASRD(A, B, 5);
		break;
	case 7:
		B = *X;
		++X;
		++*Xup;
		break;
	}

	A = *zeroY;
	++A;
	A = (A & 7);
	*zeroY = A;

	return (B & 0x1F);
}

void Parser::ASRD(dodBYTE & A, dodBYTE & B, int num)
{
	signed short D = ((signed short)A<<8) + B;
	signed short sign = D & 0x8000;

	while (num--)
		D = (D>>1) |sign;

	A=D>>8;
	B=(dodBYTE)D;
}

void Parser::CMDERR()
{
	viewer.OUTSTI(CERR);
}

int Parser::PARHND()
{
	int		res;
	dodBYTE	A, B;
	
	res = PARSER(DIRTAB, A, B, true);
	if (res != 1)
	{
		CMDERR();
		return -1;
	}
	if (A == 0 || A == 1)
	{
		return A;
	}
	else
	{
		CMDERR();
		return -1;
	}
}

/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: dungeon.h
//
// This class manages the maze data, and includes
// methods related to row/column calculations.




class Dungeon
{
public:
	// Scaffolding Code
	void printMaze();
	void SetLEVTABOrig();
	void SetVFTTABOrig();
	void SetVFTTABRandomMap();
	void SetLEVTABRandomMap();
	void ReseedMap();

	// Constructor
	Dungeon();

	// Public Interface
	void	DGNGEN();
	void	CalcVFI();
	int		RC2IDX(dodBYTE R, dodBYTE C);
	bool	STEPOK(dodBYTE R, dodBYTE C, dodBYTE dir);
	dodBYTE	VFIND(RowCol rc);
	bool	TryMove(dodBYTE dir);

	// Public Data Fields
	dodBYTE		MAZLND[1024];	// The Maze
	dodBYTE		NEIBOR[9];		// The cells around the player
								// Also used to store the walls/doors
								// of a given cell (for the 3D-Viewer)
	dodBYTE		LEVTAB[7];		// The RNG seeds
	RowCol		DROW;
	int			STPTAB[8];
	dodBYTE		VFTTAB[42];
	int			VFTPTR;

	// Constants
	enum {
		N_WALL=0x03,
		E_WALL=0x0c,
		S_WALL=0x30,
		W_WALL=0xc0,
		HF_PAS=0,
		HF_DOR=1,
		HF_SDR=2,
		HF_WAL=3,

		VF_HOLE_UP=0,
		VF_LADDER_UP=1,
		VF_HOLE_DOWN=2,
		VF_LADDER_DOWN=3,
		VF_NULL=255,
	};

private:
	// Private Implementation
	bool	BORDER(dodBYTE R, dodBYTE C);
	void	MAKDOR(dodBYTE * table);
	void	FRIEND(RowCol RC);
	void	RndDstDir(dodBYTE * DIR, dodBYTE * DST);
	bool	VFINDsub(dodBYTE & a, int & u, RowCol * rc);

	// Data Fields
	dodBYTE		MSKTAB[4];
	dodBYTE		DORTAB[4];
	dodBYTE		SDRTAB[4];

	// Scaffolding Data Fields
	char NS[4];
	char EW[4];
};

// Inline Definitions
inline void Dungeon::RndDstDir(dodBYTE * DIR, dodBYTE * DST)
{
	*DIR = (rng.RANDOM() & 3);
	*DST = (rng.RANDOM() & 7) + 1;
}

// Private Implementation
inline bool Dungeon::BORDER(dodBYTE R, dodBYTE C)
{
	if ((R & 224) != 0) return false;
	if ((C & 224) != 0) return false;
	return true;
}

inline int Dungeon::RC2IDX(dodBYTE R, dodBYTE C)
{
	R &= 31;
	C &= 31;
	return (R * 32 + C);
}

Dungeon dungeon;
/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: dungeon.cpp
//
// Implementation of Dungeon class



// Scaffolding Code


// Prints a text drawing of the maze
void Dungeon::printMaze()
{
	int idx, row, x;
	dodBYTE val, n, e, s, w;
	for (idx=0; idx<1024; idx+=32)
	{
		for (x = 0; x < 3; ++x)
		{
			for (row = 0; row < 32; ++row)
			{
				val = MAZLND[idx+row];
				n = (val & 0x03);
				e = (val & 0x0C) >> 2;
				s = (val & 0x30) >> 4;
				w = (val & 0xC0) >> 6;
				switch (x)
				{
				case 0:
					printf("x%c", NS[n]);
					if (row >= 31)
					{
						printf("x");
					}
					break;
				case 1:
					printf("%c", EW[w]);
					if (val == 0xFF)
						printf("#");
					else
						printf(" ");
					if (row >= 31)
					{
						printf("%c", EW[e]);
					}
					break;
				case 2:
					if (idx >= 992)
					{
						printf("x%c", NS[s]);
						if (row >= 31)
						{
							printf("x");
						}
					}
				}
			}
			if (x < 2)
			{
				printf("\n");
			}
		}
	}
}

// Constructor
Dungeon::Dungeon() : VFTPTR(0)
{
	SetLEVTABOrig();  //Original seed values will be overwritten (in Player::setInitialObjects())
					 //if new random map game.

	MSKTAB[0] = 0x03;
	MSKTAB[1] = 0x0C;
	MSKTAB[2] = 0x30;
	MSKTAB[3] = 0xC0;

	DORTAB[0] = HF_DOR;
	DORTAB[1] = (HF_DOR << 2);
	DORTAB[2] = (HF_DOR << 4);
	DORTAB[3] = (HF_DOR << 6);

	SDRTAB[0] = HF_SDR;
	SDRTAB[1] = (HF_SDR << 2);
	SDRTAB[2] = (HF_SDR << 4);
	SDRTAB[3] = (HF_SDR << 6);

	STPTAB[0] = -1;
	STPTAB[1] = 0;
	STPTAB[2] = 0;
	STPTAB[3] = 1;
	STPTAB[4] = 1;
	STPTAB[5] = 0;
	STPTAB[6] = 0;
	STPTAB[7] = -1;

	SetVFTTABOrig();  //Original vertical feature table values will be overwritten (in Dungeon::DGNGEN())
					 //if new random map game.

	// Scaffolding Inits
	NS[3]='x';
	NS[2]='=';
	NS[1]='x';
	NS[0]=' ';
	EW[3]='|';
	EW[2]=')';
	EW[1]='|';
	EW[0]=' ';
}

// This method can probably be streamlined since it
// was written very early.  It builds the maze.
void Dungeon::DGNGEN()
{
	/* Locals */
	int		mzctr;
	int		maz_idx;
	int		cell_ctr;
	dodBYTE	a_row;
	dodBYTE	a_col;
	dodBYTE	b_row;
	dodBYTE	b_col;
	dodBYTE	DIR;
	dodBYTE	DST;
	RowCol	DROW;
	RowCol	ROW;
	int		spin;

	/* Phase 1: Create Maze */

	/* Set Cells to 0xFF */
	for (mzctr=0; mzctr<1024; ++mzctr)
	{
		MAZLND[mzctr] = 0xFF;
	}

	rng.setSEED(LEVTAB[game.LEVEL], LEVTAB[game.LEVEL+1], LEVTAB[game.LEVEL+2]);  //Initialize Random Number Generator
	cell_ctr = 500;  // Room Counter

	/* Set Starting Room */
	if (!game.RandomMaze || game.IsDemo)
	{  //Is this an original game?  Yes:
		a_col = (rng.RANDOM() & 31);
		a_row = (rng.RANDOM() & 31);
		DROW.setRC(a_row, a_col);
		RndDstDir(&DIR, &DST);
		SetVFTTABOrig();  //Make sure the vertical feature table isn't overwritten from pervious new game.
	} else {  //Is this an original game?  No:
		switch (game.LEVEL)
		{
			case 0:
			case 3:
				a_col = (rng.RANDOM() & 31);
				a_row = (rng.RANDOM() & 31);
				break;
			case 1:
				a_row = VFTTAB[5];
				a_col = VFTTAB[6];
				break;
			case 2:
				a_row = VFTTAB[9];
				a_col = VFTTAB[10];
				break;
			default:
				a_row = VFTTAB[14];
				a_col = VFTTAB[15];
				break;
		}

		if (player.PROW == 0x10 && player.PCOL == 0x0B && game.LEVEL == 0)
		{  //Are we starting a new game?
			player.PROW = a_row;
			player.PCOL = a_col;

			//Override veritical features.
			//Will override other level's col & row during map generation.
			SetVFTTABRandomMap();
			VFTTAB[1] = a_row;
			VFTTAB[2] = a_col;
		}  //Are we starting a new game?

		//Original didn't tunnel out original room.
		//Need to do it now so that player doesn't start in wall in beginning of game.
		//Also need to make sure ladder back up to each level is in a tunneled out room.
		DROW.setRC(a_row, a_col);
		RndDstDir(&DIR, &DST);
		maz_idx = RC2IDX(a_row, a_col);
		MAZLND[maz_idx] = 0;
		--cell_ctr;
	}  //Is this an original game?


	while (cell_ctr > 0)
	{
		/* Take a step */
		b_row = DROW.row;
		b_col = DROW.col;
		b_row += STPTAB[DIR * 2];
		b_col += STPTAB[(DIR * 2) + 1];

		/* Check if it's out of bounds */
		if (BORDER(b_row, b_col) == false)
		{
			RndDstDir(&DIR, &DST);
			continue;
		}

		/* Store index and temp room */
		maz_idx = RC2IDX(b_row, b_col);
		ROW.setRC(b_row, b_col);

		/* If not yet touched */
		if (MAZLND[maz_idx] == 0xFF)
		{
			FRIEND(ROW);
			if (NEIBOR[3] + NEIBOR[0] + NEIBOR[1] == 0 ||
				NEIBOR[1] + NEIBOR[2] + NEIBOR[5] == 0 ||
				NEIBOR[5] + NEIBOR[8] + NEIBOR[7] == 0 ||
				NEIBOR[7] + NEIBOR[6] + NEIBOR[3] == 0)
			{
				RndDstDir(&DIR, &DST);
				continue;
			}
			MAZLND[maz_idx] = 0;
			--cell_ctr;
		}
		if (cell_ctr > 0)
		{
			DROW = ROW;
			--DST;
			if (DST == 0)
			{
				RndDstDir(&DIR, &DST);
				continue;
			}
			else
			{
				continue;
			}
		}
	}

	/* Phase 2: Create Walls */

	for (DROW.row = 0; DROW.row < 32; ++DROW.row)
	{
		for (DROW.col = 0; DROW.col < 32; ++DROW.col)
		{
			maz_idx = RC2IDX(DROW.row, DROW.col);
			if (MAZLND[maz_idx] != 0xFF)
			{
				FRIEND(DROW);
				if (NEIBOR[1] == 0xFF)
					MAZLND[maz_idx] |= N_WALL;
				if (NEIBOR[3] == 0xFF)
					MAZLND[maz_idx] |= W_WALL;
				if (NEIBOR[5] == 0xFF)
					MAZLND[maz_idx] |= E_WALL;
				if (NEIBOR[7] == 0xFF)
					MAZLND[maz_idx] |= S_WALL;
			}
		}
	}

	/* Phase 3: Create Doors/Secret Doors */

	for (mzctr = 0; mzctr < 70; ++mzctr)
	{
		MAKDOR(this->DORTAB);
	}

	for (mzctr = 0; mzctr < 45; ++mzctr)
	{
		MAKDOR(this->SDRTAB);
	}

	/* Phase 4: Create vertical feature */
	if (game.RandomMaze && !game.IsDemo && (game.LEVEL == 0 || game.LEVEL == 1 || game.LEVEL == 3))
	{
		do
		{
			do
			{
				a_col = (rng.RANDOM() & 31);
				a_row = (rng.RANDOM() & 31);
				ROW.setRC(a_row, a_col);
				maz_idx = RC2IDX(a_row, a_col);
			} while (MAZLND[maz_idx] == 0xFF);
		} while ((game.LEVEL == 0 && VFTTAB[1] == a_row && VFTTAB[2] == a_col) ||
				 (game.LEVEL == 1 && VFTTAB[5] == a_row && VFTTAB[6] == a_col));
		switch (game.LEVEL)
		{
			case 0:
				if (VFTTAB[5] == 0 && VFTTAB[6] == 0) {
					VFTTAB[5] = a_row;
					VFTTAB[6] = a_col;
				}
				break;
			case 1:
				if (VFTTAB[9] == 0 && VFTTAB[10] == 0) {
					VFTTAB[9] = a_row;
					VFTTAB[10] = a_col;
				}
				break;
			default:
				if (VFTTAB[14] == 0 && VFTTAB[15] == 0) {
					VFTTAB[14] = a_row;
					VFTTAB[15] = a_col;
				}
				break;
		}
	}



	// Spin the RNG
	if (scheduler.curTime == 0)
	{
		if (game.LEVEL == 0)
		{
			spin = 6;
		}
		else
		{
			spin = 21;
		}
	}
	else
	{
		spin = (scheduler.curTime % 60);
	}

	while (spin > 0)
	{
		rng.RANDOM();
		--spin;
	}
}

// Adds vertical features
void Dungeon::CalcVFI()
{
	dodBYTE lvl = game.LEVEL;
	dodBYTE idx = 0;
	do
	{
		VFTPTR = idx;
		while (VFTTAB[idx++] != 0xFF)
			;	// loop !!!
		--lvl;
	} while (lvl != 0xFF);
}

// Checks if a hole/ladder is in cell
// It has to check above and below, since each
// vertical feature is stored only once in the VFT
dodBYTE	Dungeon::VFIND(RowCol rc)
{
	int u = VFTPTR;
	dodBYTE a = 0;
	bool res;
	res = VFINDsub(a, u, &rc);
	if (res == true)
		return a;
	res = VFINDsub(a, u, &rc);
	if (res == true)
		return a + 2;
	else
		return -1;
}

// Used by VFIND
bool Dungeon::VFINDsub(dodBYTE & a, int & u, RowCol * rc)
{
	dodBYTE r, c;

	do
	{
		a = VFTTAB[u++];
		if (a == 0xFF)
			return false;
		r = VFTTAB[u++];
		c = VFTTAB[u++];
	} while ( !((r == rc->row) && (c == rc->col)) );
	return true;
}

// Checks for a wall in the given direction
bool Dungeon::TryMove(dodBYTE dir)
{
	int idx;
	dodBYTE a;
	idx = RC2IDX(player.PROW, player.PCOL);
	a = ((MAZLND[idx] >> (dir * 2)) & 3);
	if (a != 3)
		return true;
	else
		return false;
}

// Adds doors
void Dungeon::MAKDOR(dodBYTE * table)
{
	dodBYTE	a_row;
	dodBYTE	a_col;
	int		maz_idx;
	dodBYTE	val;
	dodBYTE	DIR;
	RowCol	ROW;

	do
	{
		do
		{
			a_col = (rng.RANDOM() & 31);
			a_row = (rng.RANDOM() & 31);
			ROW.setRC(a_row, a_col);
			maz_idx = RC2IDX(a_row, a_col);
			val = MAZLND[maz_idx];
		} while (val == 0xFF);

		DIR = (rng.RANDOM() & 3);
	} while ((val & MSKTAB[DIR]) != 0);

	MAZLND[maz_idx] |= table[DIR];

	ROW.row += STPTAB[DIR * 2];
	ROW.col += STPTAB[(DIR * 2) + 1];
	DIR += 2;
	DIR &= 3;
	maz_idx = RC2IDX(ROW.row, ROW.col);
	MAZLND[maz_idx] |= table[DIR];
}

// Finds surrounding cells
void Dungeon::FRIEND(RowCol RC)
{
	dodBYTE r3, c3;
	int u = 0;

	for (r3 = RC.row; r3 <= (RC.row+2); ++r3)
	{
		for (c3 = RC.col; c3 <= (RC.col+2); ++c3)
		{
			if (BORDER((r3-1), (c3-1)) == false)
			{
				NEIBOR[u] = 0xFF;
			}
			else
			{
				NEIBOR[u] = MAZLND[RC2IDX((r3-1), (c3-1))];
			}
			++u;
		}
	}
}

// Checks if a step can be taken from the given row/col
// in the given direction
bool Dungeon::STEPOK(dodBYTE R, dodBYTE C, dodBYTE dir)
{
	R += STPTAB[dir * 2];
	C += STPTAB[(dir * 2) + 1];
	if (BORDER(R,C) == false) return false;
	if (MAZLND[RC2IDX(R,C)] == 255) return false;
	return true;
}

//Sets original vertical feature table values.
void Dungeon::SetVFTTABOrig()
{
	VFTTAB[0] = -1;
	VFTTAB[1] = 1;
	VFTTAB[2] = 0;
	VFTTAB[3] = 23;
	VFTTAB[4] = 0;
	VFTTAB[5] = 15;
	VFTTAB[6] = 4;
	VFTTAB[7] = 0;
	VFTTAB[8] = 20;
	VFTTAB[9] = 17;
	VFTTAB[10] = 1;
	VFTTAB[11] = 28;
	VFTTAB[12] = 30;
	VFTTAB[13] = -1;
	VFTTAB[14] = 1;
	VFTTAB[15] = 2;
	VFTTAB[16] = 3;
	VFTTAB[17] = 0;
	VFTTAB[18] = 3;
	VFTTAB[19] = 31;
	VFTTAB[20] = 0;
	VFTTAB[21] = 19;
	VFTTAB[22] = 20;
	VFTTAB[23] = 0;
	VFTTAB[24] = 31;
	VFTTAB[25] = 0;
	VFTTAB[26] = -1;
	VFTTAB[27] = -1;
	VFTTAB[28] = 0;
	VFTTAB[29] = 0;
	VFTTAB[30] = 31;
	VFTTAB[31] = 0;
	VFTTAB[32] = 5;
	VFTTAB[33] = 0;
	VFTTAB[34] = 0;
	VFTTAB[35] = 22;
	VFTTAB[36] = 28;
	VFTTAB[37] = 0;
	VFTTAB[38] = 31;
	VFTTAB[39] = 16;
	VFTTAB[40] = -1;
	VFTTAB[41] = -1;
}

//Sets original maze seed values.
void Dungeon::SetLEVTABOrig()
{
	LEVTAB[0] = 0x73;
	LEVTAB[1] = 0xC7;
	LEVTAB[2] = 0x5D;
	LEVTAB[3] = 0x97;
	LEVTAB[4] = 0xF3;
	LEVTAB[5] = 0x13;
	LEVTAB[6] = 0x87;
}

//Override original vertical feature table values with new ones.
//Will override other level's col & row when during map generation.
void Dungeon::SetVFTTABRandomMap()
{
	VFTTAB[0] = 0;
	VFTTAB[1] = 0;
	VFTTAB[2] = 0;
	VFTTAB[3] = -1;
	VFTTAB[4] = 1;
	VFTTAB[5] = 0;
	VFTTAB[6] = 0;
	VFTTAB[7] = -1;
	VFTTAB[8] = 1;
	VFTTAB[9] = 0;
	VFTTAB[10] = 0;
	VFTTAB[11] = -1;
	VFTTAB[12] = -1;
	VFTTAB[13] = 1;
	VFTTAB[14] = 0;
	VFTTAB[15] = 0;
	VFTTAB[16] = -1;
	VFTTAB[17] = -1;
}

//Override seeds with true random numbers.
void Dungeon::SetLEVTABRandomMap()
{
	//srand(GetTickCount());
        srand(SDL_GetTicks());
	LEVTAB[0] = rand() & 255;
	LEVTAB[1] = rand() & 255;
	LEVTAB[2] = rand() & 255;
	LEVTAB[3] = rand() & 255;
	LEVTAB[4] = rand() & 255;
	LEVTAB[5] = rand() & 255;
	LEVTAB[6] = rand() & 255;
}

/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: object.h
//
// This class manages the objects (torches, etc.) in the
// game.



class Object
{
public:
	// Constructor
	Object();

	// Public Interface
	void	CreateAll();
	int		FNDOBJ();
	void	OBJNAM(int idx);
	int		OFIND(RowCol rc);
	int		OBIRTH(dodBYTE OBJCNT, dodBYTE OBJLVL);
	void	OCBFIL(dodBYTE OBJTYP, int ptr);
	bool	PAROBJ();
	void	Reset();
	void	LoadSounds();

	// Public Data Membes
	OCB			OCBLND[72];		// Holds most of the object data
	int			OFINDF;
	dodBYTE		ADJTAB[119];
	dodBYTE		GENTAB[30];
	dodBYTE		OBJTYP;
	dodBYTE		OBJCLS;
	dodBYTE		SPEFLG;
	int			OBJWGT[6];
	int			objChannel;
	Mix_Chunk *	objSound[6];

	// Constants
	enum {

		OBJ_SWORD_WOOD=17,
		OBJ_SWORD_IRON=13,
		OBJ_SWORD_ELVISH=2,

		OBJ_SHIELD_LEATHER=16,
		OBJ_SHIELD_BRONZE=11,
		OBJ_SHIELD_MITHRIL=3,

		OBJ_SCROLL_SEER=4,
		OBJ_SCROLL_VISION=7,

		OBJ_RING_JOULE=1,
			OBJ_RING_ENERGY=19, // incanted
		OBJ_RING_RIME=6,
			OBJ_RING_ICE=20,	// incanted
		OBJ_RING_VULCAN=12,
			OBJ_RING_FIRE=21,	// incanted
		OBJ_RING_SUPREME=0,
			OBJ_RING_FINAL=18,	// incanted
		OBJ_RING_GOLD=22,

		OBJ_FLASK_THEWS=5,
		OBJ_FLASK_ABYE=8,
		OBJ_FLASK_HALE=9,
		OBJ_FLASK_EMPTY=23,

		OBJ_TORCH_SOLAR=10,
		OBJ_TORCH_LUNAR=14,
		OBJ_TORCH_PINE=15,
		OBJ_TORCH_DEAD=24,

		OBJT_FLASK=0,
		OBJT_RING=1,
		OBJT_SCROLL=2,
		OBJT_SHIELD=3,
		OBJT_WEAPON=4,
		OBJT_TORCH=5,
	};

	int			OCBPTR;
	int			OFINDP;

private:
	// Data Fields
	ODB			ODBTAB[25];
	XDB			XXXTAB[11];
	dodBYTE		OMXTAB[18];
	dodBYTE		GENVAL[6];
};

Object object;
/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: object.cpp
//
// Implementation of Object class



// Constructor
Object::Object()
{
	Reset();
}

void Object::Reset()
{
	OFINDP = 0;
	OFINDF = 0;
	OCBPTR = 0;
	OBJTYP = 0;
	OBJCLS = 0;
	SPEFLG = 0;
	objChannel=1;

	ODBTAB[0] = ODB(OBJT_RING, 255, 0, 5);		// Supreme Ring
	ODBTAB[1] = ODB(OBJT_RING, 170, 0, 5);		// Joule Ring
	ODBTAB[2] = ODB(OBJT_WEAPON, 150, 64, 64);	// Elvish Sword
	ODBTAB[3] = ODB(OBJT_SHIELD, 140, 13, 26);	// Mithril Shield
	ODBTAB[4] = ODB(OBJT_SCROLL, 130, 0, 5);	// Seer Scroll
	ODBTAB[5] = ODB(OBJT_FLASK, 70, 0, 5);		// Thews Flask
	ODBTAB[6] = ODB(OBJT_RING, 52, 0, 5);		// Rime Ring
	ODBTAB[7] = ODB(OBJT_SCROLL, 50, 0, 5);		// Vision Scroll
	ODBTAB[8] = ODB(OBJT_FLASK, 48, 0, 5);		// Abye Flask
	ODBTAB[9] = ODB(OBJT_FLASK, 40, 0, 5);		// Hale Flask
	ODBTAB[10] = ODB(OBJT_TORCH, 70, 0, 5);		// Solar Torch
	ODBTAB[11] = ODB(OBJT_SHIELD, 25, 0, 26);	// Bronze Shield
	ODBTAB[12] = ODB(OBJT_RING, 13, 0, 5);		// Vulcan Ring
	ODBTAB[13] = ODB(OBJT_WEAPON, 13, 0, 40);	// Iron Sword
	ODBTAB[14] = ODB(OBJT_TORCH, 25, 0, 5);		// Lunar Torch
	ODBTAB[15] = ODB(OBJT_TORCH, 5, 0, 5);		// Pine Torch
	ODBTAB[16] = ODB(OBJT_SHIELD, 5, 0, 10);	// Leather Shield
	ODBTAB[17] = ODB(OBJT_WEAPON, 5, 0, 16);	// Wooden Sword
	ODBTAB[18] = ODB(OBJT_RING, 0, 0, 0);		// Incanted Supreme Ring
	ODBTAB[19] = ODB(OBJT_RING, 0, 255, 255);	// Incanted Joule Ring
	ODBTAB[20] = ODB(OBJT_RING, 0, 255, 255);	// Incanted Rime Ring
	ODBTAB[21] = ODB(OBJT_RING, 0, 255, 255);	// Incanted Vulcan Ring
	ODBTAB[22] = ODB(OBJT_RING, 0, 0, 5);		// Gold Ring
	ODBTAB[23] = ODB(OBJT_FLASK, 0, 0, 5);		// Empty Flask
	ODBTAB[24] = ODB(OBJT_TORCH, 5, 0, 5);		// Dead Torch

	XXXTAB[0] = XDB(0x00, 0x03, 0x12, 0x00);	// Supreme Ring
	XXXTAB[1] = XDB(0x01, 0x03, 0x13, 0x00);	// Joule Ring
	XXXTAB[2] = XDB(0x03, 0x40, 0x40, 0x00);	// Mithril Shield
	XXXTAB[3] = XDB(0x06, 0x03, 0x14, 0x00);	// Rime Ring
	XXXTAB[4] = XDB(0x0A, 0x02D0, 0x0D, 0x0B);	// Solar Torch
	XXXTAB[5] = XDB(0x0B, 0x60, 0x80, 0x00);	// Bronze Shield
	XXXTAB[6] = XDB(0x0C, 0x03, 0x15, 0x00);	// Vulcan Ring
	XXXTAB[7] = XDB(0x0E, 0x0168, 0x0A, 0x04);	// Lunar Torch
	XXXTAB[8] = XDB(0x0F, 0x00B4, 0x07, 0x00);	// Pine Torch
	XXXTAB[9] = XDB(0x10, 0x6C, 0x80, 0x00);	// Leather Shield
	XXXTAB[10] = XDB(0x18, 0x00, 0x00, 0x00);	// Dead Torch
	if (game.ShieldFix) {  //Do they want the shield fix?
		XXXTAB[5] = XDB(0x0B, 0x80, 0x60, 0x00);	// Bronze Shield
		XXXTAB[9] = XDB(0x10, 0x80, 0x6C, 0x00);	// Leather Shield
	}  //Do they want the shield fix?:

	if (game.VisionScroll)  //Do we need an extra vision scroll in level 1?  Yes:
		Utils::LoadFromHex(OMXTAB, "413131322323110416141416010408080304");
	else  //Do we need an extra vision scroll in level 1?  No:
		Utils::LoadFromHex(OMXTAB, "413131322323111316141416010408080304");
	Utils::LoadFromHex(OBJWGT, "05010A19190A");
	Utils::LoadFromHex(GENVAL, "FFFFFF10110F");
	Utils::LoadFromHex(ADJTAB,
		"1938675848AD282854FAB0A0310ACB266838DA9A22496020A652C8282882DE60"
		"2064969430AC99A5EE20022C94201016142966F6064030C527BB45306D560C2E"
		"211327B829595706402160971438D850D10590312EF790AE284C970580304AE2"
		"C8F918523280204C9914204EF610280AD8532021485090");
	Utils::LoadFromHex(GENTAB,
		"06280CC0CD602064971C30A6393D8C30E6849584292777C8802968F90D00");
}

// Creates all the objects during initialization
void Object::CreateAll()
{
	dodBYTE	a = 0, b, x;
	dodBYTE	OBJCNT, OBJLVL;

	for (x = 0; x < 72; ++x)
	{
		OCBLND[x].clear();
	}

	do
	{
		OBJCNT = (OMXTAB[a] & 0x0F);
		OBJLVL = (OMXTAB[a] >> 4);
		b = OBJLVL;

		do
		{
			x = OBIRTH(a, b);
			OCBLND[x].P_OCOWN = 0xFF;
			++b;
			if (b > 4)
			{
				b = OBJLVL;
			}
			--OBJCNT;
		} while (OBJCNT != 0);

		++a;
	} while (a < 18);
}

// Finds object on the floor in a cell
int Object::OFIND(RowCol rc)
{
	int idx;
	do
	{
		idx = FNDOBJ();
		if (idx == -1)
			return -1;
	} while ( (!((OCBLND[idx].P_OCROW == rc.row) && (OCBLND[idx].P_OCCOL == rc.col))) ||
			 (OCBLND[idx].P_OCOWN != 0) );
	return idx;
}

// Finds objects in the OCB table
int Object::FNDOBJ()
{
	int x = OFINDP;
	if (OFINDF == 0)
	{
		x = -1;
		OFINDF = -1;
	}

	do
	{
		++x;
		OFINDP = x;
		if (x == OCBPTR)
		{
			return -1;
		}
	} while (OCBLND[x].P_OCLVL != game.LEVEL);
	return x;
}

// Returns the object's name.
void Object::OBJNAM(int idx)
{
	if (idx == -1)
	{
		// return "EMPTY"
		parser.TOKEN[0] = 0x05;
		parser.TOKEN[1] = 0x0D;
		parser.TOKEN[2] = 0x10;
		parser.TOKEN[3] = 0x14;
		parser.TOKEN[4] = 0x19;
		parser.TOKEN[5] = Parser::I_NULL;
		return;
	}

	int ctr = 0;
	dodBYTE * X;
	int Xup;
	dodBYTE A;

	if (OCBLND[idx].obj_reveal_lvl == 0)
	{
		X = &ADJTAB[1];
		A = OCBLND[idx].obj_id;
		while (A != 0xFF)
		{
			parser.EXPAND(X, &Xup, 0);
			X += Xup;
			--A;
		}

		do
		{
			parser.TOKEN[ctr] = parser.STRING[ctr + 2];
		} while (parser.STRING[ctr++ + 2] != Parser::I_NULL);

		parser.TOKEN[ctr - 1] = 0;
	}

	X = &GENTAB[1];
	A = OCBLND[idx].obj_type;
	while (A != 0xFF)
	{
		parser.EXPAND(X, &Xup, 0);
		X += Xup;
		--A;
	}

	int offset = ctr;

	do
	{
		parser.TOKEN[ctr] = parser.STRING[ctr - offset + 2];
	} while (parser.STRING[ctr++ - offset + 2] != Parser::I_NULL);
}

// Parses an object name
bool Object::PAROBJ()
{
	int		res;
	dodBYTE	A, B;

	SPEFLG = 0;
	res = parser.PARSER(GENTAB, A, B, true);
	if (res == 0)
	{
		parser.CMDERR();
		return false;
	}
	if (res > 0)
	{
		OBJTYP = A;
		OBJCLS = B;
		return true;
	}

	--SPEFLG;
	res = parser.PARSER(ADJTAB, A, B, false);
	if (res <= 0)
	{
		parser.CMDERR();
		return false;
	}
	OBJTYP = A;
	OBJCLS = B;
	res = parser.PARSER(GENTAB, A, B, true);
	if (res <= 0)
	{
		parser.CMDERR();
		return false;
	}
	if (B != OBJCLS)
	{
		parser.CMDERR();
		return false;
	}
	return true;
}

// Creates new object
int Object::OBIRTH(dodBYTE OBJTYP, dodBYTE OBJLVL)
{
	dodBYTE tmp;
	int originalOCBPTR = OCBPTR;
	OCBLND[OCBPTR].obj_id = OBJTYP;
	OCBLND[OCBPTR].P_OCLVL = OBJLVL;
	OCBFIL(OBJTYP, OCBPTR);
	if (GENVAL[OCBLND[OCBPTR].obj_type] != 0xFF)
	{
		tmp = OCBLND[OCBPTR].obj_reveal_lvl;
		OBJTYP = GENVAL[OCBLND[OCBPTR].obj_type];
		OCBFIL(OBJTYP, OCBPTR);
		OCBLND[OCBPTR].obj_reveal_lvl = tmp;
	}
	++OCBPTR;
	return originalOCBPTR;
}

// Fills in default values for object
void Object::OCBFIL(dodBYTE OBJTYP, int ptr)
{
	int ctr = 0;

	OCBLND[ptr].obj_type = ODBTAB[OBJTYP].P_ODCLS;
	OCBLND[ptr].obj_reveal_lvl = ODBTAB[OBJTYP].P_ODREV;
	OCBLND[ptr].P_OCMGO = ODBTAB[OBJTYP].P_ODMGO;
	OCBLND[ptr].P_OCPHO = ODBTAB[OBJTYP].P_ODPHO;

	while (ctr < 11)
	{
		if (OBJTYP == XXXTAB[ctr].P_OXIDX)
		{
			OCBLND[ptr].P_OCXX0 = XXXTAB[ctr].P_OXXX0;
			OCBLND[ptr].P_OCXX1 = XXXTAB[ctr].P_OXXX1;
			OCBLND[ptr].P_OCXX2 = XXXTAB[ctr].P_OXXX2;
		}
		++ctr;
	}
}

/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: creature.h
//
// This class manages the creature data and movement



class Creature
{
public:
	// Constructor
	Creature();

	// Public Interface
	void		NEWLVL();
	int			CREGEN();
	int			CMOVE(int task, int cidx);
	bool		CWALK(dodBYTE dir, CCB * cr);
	bool		CFIND(dodBYTE rw, dodBYTE cl);
	int			CFIND2(RowCol rc);
	void		Reset();
	void		LoadSounds();
	void		UpdateCreSpeed();

	// Public Data Fields
	CCB			CCBLND[32];
	dodBYTE		FRZFLG;
	int			CMXPTR;
	dodBYTE		CMXLND[60];
	dodBYTE		MOVTAB[7];
	Mix_Chunk * creSound[12];
	Mix_Chunk * clank;
	Mix_Chunk * kaboom;
	Mix_Chunk *	buzz;
	int			creChannel;
	int			creChannelv;
	int			creSpeedMul;

	enum { // creature ID#s
		CRT_SPIDER=0,
		CRT_VIPER=1,
		CRT_GIANT1=2,
		CRT_BLOB=3,
		CRT_KNIGHT1=4,
		CRT_GIANT2=5,
		CRT_SCORPION=6,
		CRT_KNIGHT2=7,
		CRT_WRAITH=8,
		CRT_GALDROG=9,
		CRT_WIZIMG=10,
		CRT_WIZARD=11,
	};

private:
	// Internal Implementation
	void CBIRTH(dodBYTE a);

	// Data Fields
	CDB			CDBTAB[12];

	// Constants
	enum {
		CTYPES=12,
	};
};

Creature creature;
/****************************************
Daggorath PC-Port Version 0.2.1
Richard Hunerlach
November 13, 2002

The copyright for Dungeons of Daggorath
is held by Douglas J. Morgan.
(c) 1982, DynaMicro
*****************************************/

// Dungeons of Daggorath
// PC-Port
// Filename: creature.cpp
//
// Implementation of Creature class



// Constructor
Creature::Creature()
{
	Reset();
}

void Creature::Reset()
{
	creChannel=1;
	creChannelv=2;
	creSpeedMul=100;

	CMXPTR = 0;
	FRZFLG = 0;

	CDBTAB[0] = CDB(32,0,255,128,255,2300,1100);
	CDBTAB[1] = CDB(56,0,255,80,128,1500,700);
	CDBTAB[2] = CDB(200,0,255,52,192,2900,2300);
	CDBTAB[3] = CDB(304,0,255,96,167,3100,3100);
	CDBTAB[4] = CDB(504,0,128,96,60,1300,700);
	CDBTAB[5] = CDB(704,0,128,128,48,1700,1300);
	CDBTAB[6] = CDB(400,255,128,255,128,500,400);
	CDBTAB[7] = CDB(800,0,64,255,8,1300,700);
	CDBTAB[8] = CDB(800,192,16,192,8,300,300);
	CDBTAB[9] = CDB(1000,255,5,255,3,400,300);
	CDBTAB[10] = CDB(1000,255,6,255,0,1300,700);
	CDBTAB[11] = CDB(8000,255,6,255,0,1300,700);

	if (game.VisionScroll)  //Do we need to replace a snake with a blob w/ a vision scroll?  Yes:
		Utils::LoadFromDecDigit(CMXLND, "984300000000240666000000000406840010000000866400222222244801");
	else  //Do we need to replace a snake with a blob w/ a vision scroll?  No:
		Utils::LoadFromDecDigit(CMXLND, "994200000000240666000000000406840010000000866400222222244801");
	Utils::LoadFromDecDigit(MOVTAB, "0310130");
}

// This routine creates a new dungeon level,
// filling it with objects and creatures.  It
// should probably be moved to the Dungeon class.
void Creature::NEWLVL()
{
	dodBYTE	a, b;
	int		u, idx, tmp;

	CMXPTR = game.LEVEL * CTYPES;
	dungeon.CalcVFI();
	for (tmp = 0; tmp < 32; ++tmp)
	{
		CCBLND[tmp].clear();
	}
	scheduler.SYSTCB();
	dungeon.DGNGEN();
	u = CMXPTR;
	a = CTYPES - 1;
	do
	{
		b = CMXLND[u + a];
		if (b != 0)
		{
			do
			{
				CBIRTH(a);
				--b;
			} while (b != 0);
		}
		--a;
	} while (a != 0xFF);

	u = -1;
	object.OFINDF = 0;
	do
	{
		idx = object.FNDOBJ();
		if (idx == -1)
		{
			break;
		}
		if (object.OCBLND[idx].P_OCOWN == 0xFF)
		{
			do
			{
				++u;
				if (u == 32)
				{
					u = 0;
				}
				if (CCBLND[u].P_CCUSE != 0)
				{
					tmp = CCBLND[u].P_CCOBJ;
					CCBLND[u].P_CCOBJ = idx;
					object.OCBLND[idx].P_OCPTR = tmp;
					break;
				}
			} while (true);
		}
	} while (true);

	// Determine video invert Setting
	viewer.setVidInv((game.LEVEL % 2) ?true: false);
}

// Creates a new creature and places it in the maze
void Creature::CBIRTH(dodBYTE typ)
{
	int			u, maz_idx;
	RowCol		rndcell;
	dodBYTE		rw, cl;
	int			TCBindex;

	u = -1;
	do
	{
		++u;
	} while (CCBLND[u].P_CCUSE != 0);
	--CCBLND[u].P_CCUSE;

	CCBLND[u].creature_id = typ;
	CCBLND[u].P_CCPOW = CDBTAB[typ].P_CDPOW;
	CCBLND[u].P_CCMGO = CDBTAB[typ].P_CDMGO;
	CCBLND[u].P_CCMGD = CDBTAB[typ].P_CDMGD;
	CCBLND[u].P_CCPHO = CDBTAB[typ].P_CDPHO;
	CCBLND[u].P_CCPHD = CDBTAB[typ].P_CDPHD;
	CCBLND[u].P_CCTMV = CDBTAB[typ].P_CDTMV;
	CCBLND[u].P_CCTAT = CDBTAB[typ].P_CDTAT;

	do
	{
		do
		{
			cl = (rng.RANDOM() & 31);
			rw = (rng.RANDOM() & 31);
			//printf("          %02X, %02X = %02X\n", rw, cl, dungeon.MAZLND[dungeon.RC2IDX(rw, cl)]);
			rndcell.setRC(rw, cl);
			maz_idx = dungeon.RC2IDX(rw, cl);
		} while (dungeon.MAZLND[maz_idx] == 0xFF);
	} while (CFIND(rw, cl) == false);

	//printf("----- %02X: %02X, %02X -----\n", typ, rw, cl);
	CCBLND[u].P_CCROW = rw;
	CCBLND[u].P_CCCOL = cl;

	TCBindex = scheduler.GETTCB();
	scheduler.TCBLND[TCBindex].data = u;
	scheduler.TCBLND[TCBindex].type = Scheduler::TID_CRTMOVE;
	scheduler.TCBLND[TCBindex].frequency = CCBLND[u].P_CCTMV;
}

// These two routines should probably be combined.
// They check for a creature in the given cell
bool Creature::CFIND(dodBYTE rw, dodBYTE cl)
{
	int ctr = 0;
	while (ctr < 32)
	{
		if (CCBLND[ctr].P_CCROW == rw &&
			CCBLND[ctr].P_CCCOL == cl)
		{
			if (CCBLND[ctr].P_CCUSE != 0)
				return false;
		}
		++ctr;
	}
	return true;
}

// These two routines should probably be combined.
// They check for a creature in the given cell
int Creature::CFIND2(RowCol rc)
{
	int ctr = 0;
	while (ctr < 32)
	{
		if (CCBLND[ctr].P_CCROW == rc.row &&
			CCBLND[ctr].P_CCCOL == rc.col)
		{
			if (CCBLND[ctr].P_CCUSE != 0)
			{
				return ctr;
			}
		}
		++ctr;
	}
	return -1;
}

/* player.cpp VERBATIM: only the pure combat/heart math is copied (the
 * rest of the port's Player is SDL-entangled). */

// Calculates and assesses damage from a successful attack
bool Player::DAMAGE(int AP, int AMO, int APO,
					int DP, int DMD, int DPD, dodSHORT * DD)
{
	int a;

	a = ((AP * AMO) >> 7);
	a = ((a * DMD) >> 7);
	*DD += (dodSHORT) a;

	a = ((AP * APO) >> 7);
	a = ((a * DPD) >> 7);
	*DD += (dodSHORT) a;

	if ((dodSHORT) DP > *DD)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* stub wrapper; the HEARTR assignment line is player.cpp's HUPDAT line
 * VERBATIM (the only non-SDL statement of Player::HUPDAT). */
void Player::HUPDAT_heartr()
{
	HEARTR = (((PLRBLK.P_ATPOW) * 64) /
			  ((PLRBLK.P_ATPOW) + ((PLRBLK.P_ATDAM) * 2))) - 18;
}
