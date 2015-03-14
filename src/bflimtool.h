#ifndef BFLIMTOOL_H_
#define BFLIMTOOL_H_

#include "utility.h"
#include "bflim.h"

class CBflimTool
{
public:
	enum EParseOptionReturn
	{
		kParseOptionReturnSuccess,
		kParseOptionReturnIllegalOption,
		kParseOptionReturnNoArgument,
		kParseOptionReturnUnknownArgument,
		kParseOptionReturnOptionConflict
	};
	enum EAction
	{
		kActionNone,
		kActionDecode,
		kActionEncode,
		kActionCreate,
		kActionHelp
	};
	struct SOption
	{
		const char* Name;
		int Key;
		const char* Doc;
	};
	CBflimTool();
	~CBflimTool();
	int ParseOptions(int a_nArgc, char* a_pArgv[]);
	int CheckOptions();
	int Help();
	int Action();
	static SOption s_Option[];
private:
	EParseOptionReturn parseOptions(const char* a_pName, int& a_nIndex, int a_nArgc, char* a_pArgv[]);
	EParseOptionReturn parseOptions(int a_nKey, int& a_nIndex, int a_nArgc, char* a_pArgv[]);
	bool decodeFile();
	bool encodeFile();
	bool createFile();
	EAction m_eAction;
	const char* m_pFileName;
	const char* m_pPngName;
	CBflim::ETextureFormat m_eTextureFormat;
	n32 m_nRotate;
	u32 m_uVersion;
	bool m_bVerbose;
	const char* m_pMessage;
};

#endif	// BFLIMTOOL_H_
