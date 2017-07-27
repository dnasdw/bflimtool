#include "bflimtool.h"
#include "bflim.h"

CBflimTool::SOption CBflimTool::s_Option[] =
{
	{ "decode", 'd', "decode the bflim file" },
	{ "encode", 'e', "encode the bflim file" },
	{ "file", 'f', "the bflim file" },
	{ "png", 'p', "the png file for the bflim file" },
	{ "verbose", 'v', "show the info" },
	{ "help", 'h', "show this help" },
	{ nullptr, 0, nullptr }
};

CBflimTool::CBflimTool()
	: m_eAction(kActionNone)
	, m_pFileName(nullptr)
	, m_pPngName(nullptr)
	, m_bVerbose(false)
{
}

CBflimTool::~CBflimTool()
{
}

int CBflimTool::ParseOptions(int a_nArgc, char* a_pArgv[])
{
	if (a_nArgc <= 1)
	{
		return 1;
	}
	for (int i = 1; i < a_nArgc; i++)
	{
		int nArgpc = static_cast<int>(strlen(a_pArgv[i]));
		if (nArgpc == 0)
		{
			continue;
		}
		int nIndex = i;
		if (a_pArgv[i][0] != '-')
		{
			printf("ERROR: illegal option\n\n");
			return 1;
		}
		else if (nArgpc > 1 && a_pArgv[i][1] != '-')
		{
			for (int j = 1; j < nArgpc; j++)
			{
				switch (parseOptions(a_pArgv[i][j], nIndex, a_nArgc, a_pArgv))
				{
				case kParseOptionReturnSuccess:
					break;
				case kParseOptionReturnIllegalOption:
					printf("ERROR: illegal option\n\n");
					return 1;
				case kParseOptionReturnNoArgument:
					printf("ERROR: no argument\n\n");
					return 1;
				case kParseOptionReturnOptionConflict:
					printf("ERROR: option conflict\n\n");
					return 1;
				}
			}
		}
		else if (nArgpc > 2 && a_pArgv[i][1] == '-')
		{
			switch (parseOptions(a_pArgv[i] + 2, nIndex, a_nArgc, a_pArgv))
			{
			case kParseOptionReturnSuccess:
				break;
			case kParseOptionReturnIllegalOption:
				printf("ERROR: illegal option\n\n");
				return 1;
			case kParseOptionReturnNoArgument:
				printf("ERROR: no argument\n\n");
				return 1;
			case kParseOptionReturnOptionConflict:
				printf("ERROR: option conflict\n\n");
				return 1;
			}
		}
		i = nIndex;
	}
	return 0;
}

int CBflimTool::CheckOptions()
{
	if (m_eAction == kActionNone)
	{
		printf("ERROR: nothing to do\n\n");
		return 1;
	}
	if (m_eAction != kActionHelp)
	{
		if (m_pFileName == nullptr)
		{
			printf("ERROR: no --file option\n\n");
			return 1;
		}
		if (m_pPngName == nullptr)
		{
			printf("ERROR: no --png option\n\n");
			return 1;
		}
	}
	if (m_eAction == kActionDecode || m_eAction == kActionEncode)
	{
		if (!CBflim::IsBflimFile(m_pFileName))
		{
			printf("ERROR: %s is not a bflim file\n\n", m_pFileName);
			return 1;
		}
	}
	return 0;
}

int CBflimTool::Help()
{
	printf("bflimtool %s by dnasdw\n\n", BFLIMTOOL_VERSION);
	printf("usage: bflimtool [option...] [option]...\n");
	printf("sample:\n");
	printf("  bflimtool -dvfp title.bflim title.png\n");
	printf("  bflimtool -evfp title.bflim title_new.png\n");
	printf("\n");
	printf("option:\n");
	SOption* pOption = s_Option;
	while (pOption->Name != nullptr || pOption->Doc != nullptr)
	{
		if (pOption->Name != nullptr)
		{
			printf("  ");
			if (pOption->Key != 0)
			{
				printf("-%c,", pOption->Key);
			}
			else
			{
				printf("   ");
			}
			printf(" --%-8s", pOption->Name);
			if (strlen(pOption->Name) >= 8 && pOption->Doc != nullptr)
			{
				printf("\n%16s", "");
			}
		}
		if (pOption->Doc != nullptr)
		{
			printf("%s", pOption->Doc);
		}
		printf("\n");
		pOption++;
	}
	return 0;
}

int CBflimTool::Action()
{
	if (m_eAction == kActionDecode)
	{
		if (!decodeFile())
		{
			printf("ERROR: decode file failed\n\n");
			return 1;
		}
	}
	if (m_eAction == kActionEncode)
	{
		if (!encodeFile())
		{
			printf("ERROR: encode file failed\n\n");
			return 1;
		}
	}
	if (m_eAction == kActionHelp)
	{
		return Help();
	}
	return 0;
}

CBflimTool::EParseOptionReturn CBflimTool::parseOptions(const char* a_pName, int& a_nIndex, int a_nArgc, char* a_pArgv[])
{
	if (strcmp(a_pName, "decode") == 0)
	{
		if (m_eAction == kActionNone)
		{
			m_eAction = kActionDecode;
		}
		else if (m_eAction != kActionDecode && m_eAction != kActionHelp)
		{
			return kParseOptionReturnOptionConflict;
		}
	}
	else if (strcmp(a_pName, "encode") == 0)
	{
		if (m_eAction == kActionNone)
		{
			m_eAction = kActionEncode;
		}
		else if (m_eAction != kActionEncode && m_eAction != kActionHelp)
		{
			return kParseOptionReturnOptionConflict;
		}
	}
	else if (strcmp(a_pName, "file") == 0)
	{
		if (a_nIndex + 1 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		m_pFileName = a_pArgv[++a_nIndex];
	}
	else if (strcmp(a_pName, "png") == 0)
	{
		if (a_nIndex + 1 >= a_nArgc)
		{
			return kParseOptionReturnNoArgument;
		}
		m_pPngName = a_pArgv[++a_nIndex];
	}
	else if (strcmp(a_pName, "verbose") == 0)
	{
		m_bVerbose = true;
	}
	else if (strcmp(a_pName, "help") == 0)
	{
		m_eAction = kActionHelp;
	}
	return kParseOptionReturnSuccess;
}

CBflimTool::EParseOptionReturn CBflimTool::parseOptions(int a_nKey, int& a_nIndex, int m_nArgc, char* a_pArgv[])
{
	for (SOption* pOption = s_Option; pOption->Name != nullptr || pOption->Key != 0 || pOption->Doc != nullptr; pOption++)
	{
		if (pOption->Key == a_nKey)
		{
			return parseOptions(pOption->Name, a_nIndex, m_nArgc, a_pArgv);
		}
	}
	return kParseOptionReturnIllegalOption;
}

bool CBflimTool::decodeFile()
{
	CBflim bflim;
	bflim.SetFileName(m_pFileName);
	bflim.SetPngName(m_pPngName);
	bflim.SetVerbose(m_bVerbose);
	return bflim.DecodeFile();
}

bool CBflimTool::encodeFile()
{
	CBflim bflim;
	bflim.SetFileName(m_pFileName);
	bflim.SetPngName(m_pPngName);
	bflim.SetVerbose(m_bVerbose);
	return bflim.EncodeFile();
}

int main(int argc, char* argv[])
{
	FSetLocale();
	CBflimTool tool;
	if (tool.ParseOptions(argc, argv) != 0)
	{
		return tool.Help();
	}
	if (tool.CheckOptions() != 0)
	{
		return 1;
	}
	return tool.Action();
}
