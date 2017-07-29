#ifndef BFLIMTOOL_H_
#define BFLIMTOOL_H_

#include <sdw.h>

class CBflimTool
{
public:
	enum EParseOptionReturn
	{
		kParseOptionReturnSuccess,
		kParseOptionReturnIllegalOption,
		kParseOptionReturnNoArgument,
		kParseOptionReturnOptionConflict
	};
	enum EAction
	{
		kActionNone,
		kActionDecode,
		kActionEncode,
		kActionHelp
	};
	struct SOption
	{
		const UChar* Name;
		int Key;
		const UChar* Doc;
	};
	CBflimTool();
	~CBflimTool();
	int ParseOptions(int a_nArgc, UChar* a_pArgv[]);
	int CheckOptions();
	int Help();
	int Action();
	static SOption s_Option[];
private:
	EParseOptionReturn parseOptions(const UChar* a_pName, int& a_nIndex, int a_nArgc, UChar* a_pArgv[]);
	EParseOptionReturn parseOptions(int a_nKey, int& a_nIndex, int a_nArgc, UChar* a_pArgv[]);
	bool decodeFile();
	bool encodeFile();
	EAction m_eAction;
	UString m_sFileName;
	UString m_sPngName;
	bool m_bVerbose;
};

#endif	// BFLIMTOOL_H_
