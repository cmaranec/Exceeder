#include "Global.h"
#include "Log.h"
#include "Storage.h"
#include "Parsers/StyleParser.h"
#include "Defines/Styles.h"

bool StyleParser::ParseFile(const wchar_t *path)
{
    if (!path)
        return false;

#ifdef _WIN32
    FILE* sfile = _wfopen(path, L"r, ccs=UTF-8");
#else
    FILE* sfile = fopen(ToMultiByteString(path), "r, ccs=UTF-8");
#endif
    if (!sfile)
        return false;

    std::vector<std::wstring> stylefile;
    wchar_t* ln = NULL;
    while ((ln = ReadLine(sfile)) != NULL)
    {
        if (PrepareLine(ln))
            stylefile.push_back(ln);
    }

    return Parse(&stylefile);
}

bool StyleParser::Parse(std::vector<std::wstring> *input)
{
    if (!input)
        return false;

    wchar_t* left = NULL;
    wchar_t* right = NULL;

    wchar_t* stylename = NULL;
    Style* tmp = NULL;

    for (std::vector<std::wstring>::const_iterator itr = input->begin(); itr != input->end(); ++itr)
    {
        left = LeftSide(itr->c_str(), ' ');
        right = RightSide(itr->c_str(), ' ');

        // when parsing style definition
        if (stylename)
        {
            if (!tmp)
                tmp = new Style;

            // font family
            if (EqualString(left, L"\\FONT_FAMILY"))
            {
                tmp->fontFamily = right;

                // And set "flag" for generate a new font
                if (tmp->fontId >= 0)
                    tmp->fontId = -2;
            }
            // font size in pixels
            else if (EqualString(left, L"\\FONT_SIZE"))
            {
                if (IsNumeric(right))
                    tmp->fontSize = new uint32(ToInt(right));
                else
                    RAISE_ERROR("StyleParser: Non-numeric value '%s' used as font size", (right)?ToMultiByteString(right):"none");

                if (tmp->fontId >= 0)
                    tmp->fontId = -2;
            }
            // font color
            else if (EqualString(left, L"\\FONT_COLOR"))
            {
                uint32 dst = 0;
                if (ParseColor(right, &dst))
                    tmp->fontColor = new uint32(dst);
                else
                    RAISE_ERROR("StyleParser: Invalid expression '%s' used as font color", (right)?ToMultiByteString(right):"none");

                if (tmp->fontId >= 0)
                    tmp->fontId = -2;
            }
            else if (EqualString(left, L"\\BOLD"))
            {
                tmp->bold = true;
            }
            else if (EqualString(left, L"\\NOBOLD"))
            {
                tmp->bold = false;
            }
            else if (EqualString(left, L"\\ITALIC"))
            {
                tmp->italic = true;
            }
            else if (EqualString(left, L"\\NOITALIC"))
            {
                tmp->italic = false;
            }
            else if (EqualString(left, L"\\UNDERLINE"))
            {
                tmp->underline = true;
            }
            else if (EqualString(left, L"\\NOUNDERLINE"))
            {
                tmp->underline = false;
            }
            else if (EqualString(left, L"\\STRIKE"))
            {
                tmp->strikeout = true;
            }
            else if (EqualString(left, L"\\NOSTRIKE"))
            {
                tmp->strikeout = false;
            }
            else if (EqualString(left, L"\\DEF_END"))
            {
                sStorage->AddNewStyle(stylename, tmp);
                stylename = NULL;
                tmp = NULL;
            }

            continue;
        }

        // when not parsing style definition
        // file version
        if (EqualString(left, L"\\EXCEEDER_STYLES_FILE_VERSION"))
        {
            //
        }
        // start of definition
        else if (EqualString(left, L"\\DEF_BEGIN"))
        {
            stylename = right;
            continue;
        }
        // end
        else if (EqualString(left, L"\\END"))
        {
            return true;
        }
        else
        {
            RAISE_ERROR("StyleParser: Unrecognized key '%s'", left);
        }
    }

    return true;
}

bool StyleParser::ParseColor(const wchar_t *input, uint32 *dest)
{
    if (!input || !dest)
        return false;

    // At first, search known colors
    for (uint32 i = 0; i < sizeof(KnownColors)/sizeof(KnownColor); i++)
    {
        if (EqualString(input, KnownColors[i].name))
        {
            (*dest) = KnownColors[i].color;
            return true;
        }
    }

    // hexa color?
    uint32 len = wcslen(input);
    if (len == 6 || (len == 7 && input[0] == '#'))
    {
        for (uint32 i = (len==7) ? 1 : 0; i < len; i++)
        {
            if (input[i] < 'A' || input[i] > 'F')
                if (input[i] < '0' || input[i] > '9')
                    return false;

            const wchar_t* ptr = (input+len-6);
            (*dest) = uint32(wcstol(ptr, (wchar_t**)&(ptr), 16) << 8);
            return true;
        }
    }

    return false;
}
