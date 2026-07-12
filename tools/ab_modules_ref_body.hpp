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
};
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
struct GameStub { dodBYTE LEVEL; bool RandomMaze; bool IsDemo; } game;
struct PlayerStub { dodBYTE PROW, PCOL; } player;
struct SchedStub { Uint32 curTime; } scheduler;
struct ViewerStub { void OUTSTI(dodBYTE*) {} } viewer;
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
