#include "Global.h"
#include "Log.h"
#include "Storage.h"
#include "Presentation.h"
#include "Parsers/SlideParser.h"
#include "Parsers/StyleParser.h"
#include "Defines/Slides.h"
#include "Parsers/ExpressionParser.h"

bool SlideParser::ParseFile(const wchar_t *path)
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

    std::vector<std::wstring> slidefile;
    wchar_t* ln = NULL;
    while ((ln = ReadLine(sfile)) != NULL)
    {
        if (PrepareLine(ln) && PreParseLine(ln))
            slidefile.push_back(ln);
    }

    return Parse(&slidefile);
}

bool SlideParser::Parse(std::vector<std::wstring> *input)
{
    if (!input)
        return false;

    wchar_t* left = NULL;
    wchar_t* right = NULL;
    wchar_t* middle = NULL; // for additional element definitions

    wchar_t* persistent = NULL;

    SlideElement* tmp = NULL;

    uint32 len = 0;

    ParsedDefs defs;

    uint8 special = 0;

    for (std::vector<std::wstring>::const_iterator itr = input->begin(); itr != input->end(); ++itr)
    {
        sStorage->SetCriticalError(false);

        tmp = ParseElement(itr->c_str(), &special, &persistent);

        if (tmp != NULL)
        {
            sStorage->AddSlideElement(tmp);
            tmp = NULL;
            continue;
        }
        // non-template elements are also not available for adding into storage element map
        else if (special & SEPF_NON_TEMPLATE)
        {
            // do not do anything
            continue;
        }
        // only-template elements are handler by child parser
        else if (special & SEPF_ONLY_TEMPLATE)
        {
            // elements are inserted "all in one" from slide template, and the child parser only takes care of overwriting the values
            // so.. do not do anything
            continue;
        }

        defs.clear();

        middle = LeftSide(itr->c_str(), ' ');
        len = wcslen(middle);
        // we have to try it again with { delimiter, because of additional definitions of elements
        left = LeftSide(middle, '{');
        if (wcslen(left) != wcslen(middle))
            middle = RightSide(middle, '{');

        right = RightSide(itr->c_str(), ' ');

        // file version
        if (EqualString(left, L"\\EXCEEDER_SLIDE_FILE_VERSION", true))
        {
            //
        }
        // end
        else if (EqualString(left, L"\\END", true))
        {
            return true;
        }
        else
        {
            if (special & SEPF_REPORT_ERROR)
            {
                sLog->ErrorLog("SlideParser: Line '%S' Unrecognized key '%S'", itr->c_str(), left);
                special &= ~SEPF_REPORT_ERROR;
            }

            // break the process only if the error is unrecoverable
            if (sStorage->IsCriticalError())
                return false;
        }
    }

    return true;
}

SlideElement* SlideParser::ParseElement(const wchar_t *input, uint8* special, wchar_t** persistentIdentificator)
{
    if (special)
    {
        // Clear only output flags, the input ones should stay here for manual removal in parse procedure
        (*special) &= SEPF_ONLY_TEMPLATE;
    }

    if (!input)
        return NULL;

    wchar_t* left = NULL;
    wchar_t* right = NULL;
    wchar_t* middle = NULL; // for additional element definitions

    SlideElement* tmp = NULL;

    uint32 len = 0;

    ParsedDefs defs;

    middle = LeftSide(input, ' ');
    len = wcslen(middle);
    // we have to try it again with { delimiter, because of additional definitions of elements
    left = LeftSide(middle, '{');
    if (wcslen(left) != wcslen(middle))
        middle = RightSide(middle, '{');

    right = RightSide(input, ' ');

    // Special cases for input flags
    if ((*special) & SEPF_ONLY_TEMPLATE)
    {
        if (EqualString(left, L"\\TEMPLATE_END", true))
        {
            (*special) &= ~SEPF_ONLY_TEMPLATE;
            (*special) |= SEPF_NON_TEMPLATE;
            persistentIdentificator = NULL;
            return NULL;
        }
        else
        {
            // check if it's attempt to add new element (left side has more chars than \), or overwrite existing (left side has only \)
            if (wcslen(left) > 1)
            {
                uint8 outputSpecial = 0;
                SlideElement* ntmp = ParseElement(input, &outputSpecial, NULL);

                // this means, that we are attempting to parse new template call inside template call
                // DiCaprio would hate me --> avoiding inception
                if (outputSpecial & SEPF_ONLY_TEMPLATE)
                    return NULL;

                sStorage->InsertAfterLastOverwritten(ntmp);

                return NULL;
            }
            else
            {
                ParseInputDefinitions(middle, &defs);

                const wchar_t* idc = GetDefinitionKeyValue(&defs, L"ID");

                if (!idc)
                    RAISE_ERROR_NULL("SlideParser: template filling: couldn't parse element replacement line '%S' - ID armugent missing", input);

                std::wstring idstr = idc;
                idstr.append(TEMPLATE_ID_DELIMITER);
                idstr.append((*persistentIdentificator));

                SlideElement* live = sStorage->GetTemplateSlideElementById(idstr.c_str());

                if (!live)
                    RAISE_ERROR_NULL("SlideParser: template filling: couldn't find element with ID '%S' in template '%S'", idc, persistentIdentificator);

                wchar_t* efc = GetDefinitionKeyValue(&defs, L"E");
                if (efc)
                    live->elemEffect = efc;

                sStorage->UpdateLastOverwritten(idstr.c_str());

                if (live->elemType == SLIDE_ELEM_TEXT)
                {
                    if (!right)
                        live->typeText.text = L"";
                    else if (wcslen(right) > 0)
                        live->typeText.text = right;
                }
            }
        }

        return NULL;
    }

    // background element
    if (EqualString(left, L"\\BACKGROUND", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_BACKGROUND;

        tmp->typeBackground.color = 0;
        tmp->typeBackground.imageResourceId = 0;

        tmp->typeBackground.position[0] = POS_CENTER;
        tmp->typeBackground.position[1] = POS_CENTER;

        tmp->typeBackground.spread = SPREAD_NONE;

        tmp->typeBackground.dimensions[0] = 0;
        tmp->typeBackground.dimensions[1] = 0;

        tmp->typeBackground.gradientEdges = true;

        for (uint32 i = 0; i < 4; i++)
            tmp->typeBackground.gradients[i] = NULL;

        wchar_t* el = right;
        wchar_t* er = NULL;

        wchar_t* inner_id = NULL, *inner_value = NULL;

        do
        {
            er = RightSide(el, L',');
            el = LeftSide(el, L',');

            inner_id = LeftSide(el, L'=');
            inner_value = RightSide(el, L'=');

            if (!inner_value)
                RAISE_ERROR_NULL("SlideParser: invalid background definition chunk identified by %s", (inner_id != NULL)?ToMultiByteString(inner_id):"nothing");

            // Background mono-color
            if (EqualString(inner_id, L"COLOR", true))
            {
                if (!StyleParser::ParseColor(inner_value, &tmp->typeBackground.color))
                    RAISE_ERROR_NULL("SlideParser: invalid color name / value used in background definition: %s", ToMultiByteString(inner_value));
            }
            // Image as background
            else if (EqualString(inner_id, L"RESOURCE", true) || EqualString(inner_id, L"IMAGE", true))
            {
                ResourceEntry* res = sStorage->GetResource(inner_value);
                if (!res)
                    RAISE_ERROR_NULL("SlideParser: Resource identified by '%s' not found", (inner_value)?ToMultiByteString(inner_value):"none");
                if (res->type != RESOURCE_IMAGE)
                    RAISE_ERROR_NULL("SlideParser: Resource identified by '%s' is not an image", (inner_value)?ToMultiByteString(inner_value):"none");

                tmp->typeBackground.imageResourceId = res->internalId;

                if (tmp->typeBackground.dimensions[0] == 0)
                    tmp->typeBackground.dimensions[0] = res->implicitWidth;
                if (tmp->typeBackground.dimensions[1] == 0)
                    tmp->typeBackground.dimensions[1] = res->implicitHeight;
            }
            // horizontal position of background
            else if (EqualString(inner_id, L"POS-X", true))
            {
                if (EqualString(inner_value, L"CENTER", true))
                    tmp->typeBackground.position[0] = POS_CENTER;
                else if (EqualString(inner_value, L"LEFT", true))
                    tmp->typeBackground.position[0] = POS_LEFT;
                else if (EqualString(inner_value, L"RIGHT", true))
                    tmp->typeBackground.position[0] = POS_RIGHT;
                else if (IsNumeric(inner_value))
                    tmp->typeBackground.position[0] = ToInt(inner_value);
                else
                    RAISE_ERROR_NULL("SlideParser: invalid (non-numeric or special) value '%s' used as X position of background", ToMultiByteString(inner_value));
            }
            // vertical position of background
            else if (EqualString(inner_id, L"POS-Y", true))
            {
                if (EqualString(inner_value, L"CENTER", true))
                    tmp->typeBackground.position[1] = POS_CENTER;
                else if (EqualString(inner_value, L"TOP", true))
                    tmp->typeBackground.position[1] = POS_TOP;
                else if (EqualString(inner_value, L"BOTTOM", true))
                    tmp->typeBackground.position[1] = POS_BOTTOM;
                else if (IsNumeric(inner_value))
                    tmp->typeBackground.position[1] = ToInt(inner_value);
                else
                    RAISE_ERROR_NULL("SlideParser: invalid (non-numeric or special) value '%s' used as Y position of background", ToMultiByteString(inner_value));
            }
            // horizontal and vertical position of background
            else if (EqualString(inner_id, L"POS", true))
            {
                if (EqualString(inner_value, L"CENTER", true))
                {
                    tmp->typeBackground.position[0] = POS_CENTER;
                    tmp->typeBackground.position[1] = POS_CENTER;
                }
                else if (EqualString(inner_value, L"TOP", true))
                    tmp->typeBackground.position[1] = POS_TOP;
                else if (EqualString(inner_value, L"BOTTOM", true))
                    tmp->typeBackground.position[1] = POS_BOTTOM;
                else if (EqualString(inner_value, L"LEFT", true))
                    tmp->typeBackground.position[0] = POS_LEFT;
                else if (EqualString(inner_value, L"RIGHT", true))
                    tmp->typeBackground.position[0] = POS_RIGHT;
                else if (EqualString(inner_value, L"TOPRIGHT", true))
                {
                    tmp->typeBackground.position[0] = POS_RIGHT;
                    tmp->typeBackground.position[1] = POS_TOP;
                }
                else if (EqualString(inner_value, L"TOPLEFT", true))
                {
                    tmp->typeBackground.position[0] = POS_LEFT;
                    tmp->typeBackground.position[1] = POS_TOP;
                }
                else if (EqualString(inner_value, L"BOTTOMRIGHT", true))
                {
                    tmp->typeBackground.position[0] = POS_RIGHT;
                    tmp->typeBackground.position[1] = POS_BOTTOM;
                }
                else if (EqualString(inner_value, L"BOTTOMLEFT", true))
                {
                    tmp->typeBackground.position[0] = POS_LEFT;
                    tmp->typeBackground.position[1] = POS_BOTTOM;
                }
                else
                    RAISE_ERROR_NULL("SlideParser: invalid value '%s' used as X-Y position of background", ToMultiByteString(inner_value));
            }
            // image width/height spread
            else if (EqualString(inner_id, L"SPREAD", true))
            {
                if (EqualString(inner_value, L"NONE", true))
                    tmp->typeBackground.spread = SPREAD_NONE;
                else if (EqualString(inner_value, L"WIDTH", true))
                    tmp->typeBackground.spread = SPREAD_WIDTH;
                else if (EqualString(inner_value, L"HEIGHT", true))
                    tmp->typeBackground.spread = SPREAD_HEIGHT;
                else if (EqualString(inner_value, L"BOTH", true))
                    tmp->typeBackground.spread = SPREAD_BOTH;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid value '%s' used as background spread", ToMultiByteString(inner_value));
            }
            // image width dimension
            else if (EqualString(inner_id, L"WIDTH", true))
            {
                if (EqualString(inner_value, L"FULL", true))
                    tmp->typeBackground.spread = SPREAD_WIDTH;
                else if (IsNumeric(inner_value))
                    tmp->typeBackground.dimensions[0] = ToInt(inner_value);
                else
                    RAISE_ERROR_NULL("SlideParser: invalid (non-numeric or special) value '%s' used as background width", ToMultiByteString(inner_value));
            }
            // image height dimension
            else if (EqualString(inner_id, L"HEIGHT", true))
            {
                if (EqualString(inner_value, L"FULL", true))
                    tmp->typeBackground.spread = SPREAD_HEIGHT;
                else if (IsNumeric(inner_value))
                    tmp->typeBackground.dimensions[1] = ToInt(inner_value);
                else
                    RAISE_ERROR_NULL("SlideParser: invalid (non-numeric or special) value '%s' used as background height", ToMultiByteString(inner_value));
            }
            // gradients
            else if (EqualString(inner_id, L"GRADIENT", true))
            {
                el = LeftSide(inner_value, L' ');
                wchar_t* col = RightSide(inner_value, L' ');
                if (!el)
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - arguments missing");
                if (!col)
                    RAISE_ERROR_NULL("SlideParser: not enough parameters for gradient background - color argument missing");

                GradientData* gd = new GradientData;

                if (IsNumeric(el))
                    gd->size = ToInt(el);
                else if (EqualString(el, L"BODY", true))
                    tmp->typeBackground.gradientEdges = false;
                else if (EqualString(el, L"EDGE", true))
                    tmp->typeBackground.gradientEdges = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - invalid size '%s'", ToMultiByteString(el));

                // Is this function neccessary? Do we really need it in percents?
                //if (el[wcslen(el)-1] == L'%')
                //    el[wcslen(el)-1] = L'\0';

                if (!StyleParser::ParseColor(col, &gd->color))
                    RAISE_ERROR_NULL("SlideParser: invalid expression '%s' used as color value for background gradient", ToMultiByteString(col));

                // make all gradient data pointers to point to the same piece of memory
                for (uint32 i = 0; i < GRAD_MAX; i++)
                    tmp->typeBackground.gradients[i] = gd;
            }
            // gradient top
            else if (EqualString(inner_id, L"GRADIENT-TOP", true))
            {
                el = LeftSide(inner_value, L' ');
                wchar_t* col = RightSide(inner_value, L' ');
                if (!el)
                    RAISE_ERROR_NULL("SlideParser: invalid top gradient definition for background - arguments missing");
                if (!col)
                    RAISE_ERROR_NULL("SlideParser: not enough parameters for top gradient background - color argument missing");
                GradientData* gd = new GradientData;

                if (IsNumeric(el))
                    gd->size = ToInt(el);
                else if (EqualString(el, L"BODY", true))
                    tmp->typeBackground.gradientEdges = false;
                else if (EqualString(el, L"EDGE", true))
                    tmp->typeBackground.gradientEdges = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - invalid size '%s'", ToMultiByteString(el));

                if (!StyleParser::ParseColor(col, &gd->color))
                    RAISE_ERROR_NULL("SlideParser: invalid expression '%s' used as color value for background top gradient", ToMultiByteString(col));

                tmp->typeBackground.gradients[GRAD_TOP] = gd;
            }
            // gradient left
            else if (EqualString(inner_id, L"GRADIENT-LEFT", true))
            {
                el = LeftSide(inner_value, L' ');
                wchar_t* col = RightSide(inner_value, L' ');
                if (!el)
                    RAISE_ERROR_NULL("SlideParser: invalid left gradient definition for background - arguments missing");
                if (!col)
                    RAISE_ERROR_NULL("SlideParser: not enough parameters for left gradient background - color argument missing");

                GradientData* gd = new GradientData;

                if (IsNumeric(el))
                    gd->size = ToInt(el);
                else if (EqualString(el, L"BODY", true))
                    tmp->typeBackground.gradientEdges = false;
                else if (EqualString(el, L"EDGE", true))
                    tmp->typeBackground.gradientEdges = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - invalid size '%s'", ToMultiByteString(el));

                if (!StyleParser::ParseColor(col, &gd->color))
                    RAISE_ERROR_NULL("SlideParser: invalid expression '%s' used as color value for background left gradient", ToMultiByteString(col));

                tmp->typeBackground.gradients[GRAD_LEFT] = gd;
            }
            // gradient right
            else if (EqualString(inner_id, L"GRADIENT-RIGHT", true))
            {
                el = LeftSide(inner_value, L' ');
                wchar_t* col = RightSide(inner_value, L' ');
                if (!el)
                    RAISE_ERROR_NULL("SlideParser: invalid right gradient definition for background - arguments missing");
                if (!col)
                    RAISE_ERROR_NULL("SlideParser: not enough parameters for right gradient background - color argument missing");

                GradientData* gd = new GradientData;

                if (IsNumeric(el))
                    gd->size = ToInt(el);
                else if (EqualString(el, L"BODY", true))
                    tmp->typeBackground.gradientEdges = false;
                else if (EqualString(el, L"EDGE", true))
                    tmp->typeBackground.gradientEdges = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - invalid size '%s'", ToMultiByteString(el));

                if (!StyleParser::ParseColor(col, &gd->color))
                    RAISE_ERROR_NULL("SlideParser: invalid expression '%s' used as color value for background right gradient", ToMultiByteString(col));

                tmp->typeBackground.gradients[GRAD_RIGHT] = gd;
            }
            // gradient bottom
            else if (EqualString(inner_id, L"GRADIENT-BOTTOM", true))
            {
                el = LeftSide(inner_value, L' ');
                wchar_t* col = RightSide(inner_value, L' ');
                if (!el)
                    RAISE_ERROR_NULL("SlideParser: invalid bottom gradient definition for background - arguments missing");
                if (!col)
                    RAISE_ERROR_NULL("SlideParser: not enough parameters for bottom gradient background - color argument missing");

                GradientData* gd = new GradientData;

                if (IsNumeric(el))
                    gd->size = ToInt(el);
                else if (EqualString(el, L"BODY", true))
                    tmp->typeBackground.gradientEdges = false;
                else if (EqualString(el, L"EDGE", true))
                    tmp->typeBackground.gradientEdges = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid gradient definition for background - invalid size '%s'", ToMultiByteString(el));

                if (!StyleParser::ParseColor(col, &gd->color))
                    RAISE_ERROR_NULL("SlideParser: invalid expression '%s' used as color value for background bottom gradient", ToMultiByteString(col));

                tmp->typeBackground.gradients[GRAD_BOTTOM] = gd;
            }
            else
                sLog->ErrorLog("SlideParser: unknown key '%s' in background definition", ToMultiByteString(inner_id));

            el = er;

        } while (er != NULL);

        return tmp;
    }
    // text element
    else if (EqualString(left, L"\\TEXT", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_TEXT;
        tmp->drawable = true;

        ParseInputDefinitions(middle, &defs);

        tmp->elemId = GetDefinitionKeyValue(&defs, L"ID");
        tmp->elemStyle = GetDefinitionKeyValue(&defs, L"S");
        tmp->elemEffect = GetDefinitionKeyValue(&defs, L"E");

        tmp->typeText.text = right;

        GetPositionDefinitionKeyValue(&defs, L"P", &tmp->position[0], &tmp->position[1]);
        tmp->finalPosition[0] = tmp->position[0];
        tmp->finalPosition[1] = tmp->position[1];

        tmp->typeText.depth = 0;
        if (const wchar_t* depth = GetDefinitionKeyValue(&defs, L"D"))
            if (IsNumeric(depth))
                tmp->typeText.depth = ToInt(depth);

        tmp->typeText.wrapSign = WW_PREWRAP;

        if (const wchar_t* wrapsign = GetDefinitionKeyValue(&defs, L"W"))
            if (EqualString(wrapsign, L"NONE", true))
                tmp->typeText.wrapSign = WW_NO_WRAP;

        // Text elements needs to be postparsed due to possibility of marking
        sStorage->AddPostParseElement(tmp);

        return tmp;
    }
    // blocking the run of presentation for specified time or to any interface event
    else if (EqualString(left, L"\\BLOCK", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_BLOCK;
        tmp->typeBlock.time = 0;
        tmp->typeBlock.passthrough = false;

        while (right)
        {
            left = LeftSide(right, L' ');

            if (left)
            {
                if (IsNumeric(left))
                    tmp->typeBlock.time = ToInt(left);
                else if (EqualString(left, L"passthrough", true))
                    tmp->typeBlock.passthrough = true;
                else
                    RAISE_ERROR_NULL("SlideParser: invalid value '%S' used as parameter for blocking event", left);
            }

            right = RightSide(right, L' ');
        }

        return tmp;
    }
    // loading an external image
    else if (EqualString(left, L"\\LOAD_IMAGE", true))
    {
        std::wstring name, path;

        name = RemoveBeginningSpaces(LeftSide(right, ','));
        path = RemoveBeginningSpaces(RightSide(right, ','));

        if (name.size() < 2)
            RAISE_ERROR_NULL("SlideParser: Invalid name %s - name must have at least 2 characters", name.c_str());
        if (path.size() < 1)
            RAISE_ERROR_NULL("SlideParser: Invalid path - not entered or not valid");

        sStorage->PrepareImageResource(name.c_str(), path.c_str());

        // set special flag to 1, to avoid parsing by default
        // this is flag for items not-usable in templates!
        if (special)
            (*special) = SEPF_NON_TEMPLATE;

        return NULL;
    }
    // drawing loaded image
    else if (EqualString(left, L"\\DRAW_IMAGE", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_IMAGE;
        tmp->drawable = true;

        defs.clear();
        ParseInputDefinitions(middle, &defs);

        tmp->elemId = GetDefinitionKeyValue(&defs, L"ID");
        tmp->elemStyle = GetDefinitionKeyValue(&defs, L"S");
        tmp->elemEffect = GetDefinitionKeyValue(&defs, L"E");

        GetPositionDefinitionKeyValue(&defs, L"P", &tmp->position[0], &tmp->position[1]);
        GetPositionDefinitionKeyValue(&defs, L"V", (int32*)&tmp->typeImage.size[0], (int32*)&tmp->typeImage.size[1]); // we can make explicit conversion to int32* since range won't exceed

        ResourceEntry* res = sStorage->GetResource(right);
        if (res)
            tmp->typeImage.resourceId = res->internalId;
        else
            tmp->typeImage.resourceId = 0;

        if (tmp->typeImage.size[0] == 0 || tmp->typeImage.size[1] == 0)
        {
            if (res && res->implicitWidth > 0 && res->implicitHeight > 0)
            {
                tmp->typeImage.size[0] = res->implicitWidth;
                tmp->typeImage.size[1] = res->implicitHeight;
            }
            else
                sLog->ErrorLog("SlideParser: no valid dimensions for resource '%s' specified!", right);
        }

        return tmp;
    }
    // mouse press event element
    else if (EqualString(left, L"\\MOUSE_LEFT", true) || EqualString(left, L"\\MOUSE_RIGHT", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_MOUSE_EVENT;

        if (EqualString(left, L"\\MOUSE_LEFT", true))
            tmp->typeMouseEvent.type = MOUSE_EVENT_LEFT_DOWN;
        else
            tmp->typeMouseEvent.type = MOUSE_EVENT_RIGHT_DOWN;

        for (uint32 i = 0; i < 2; i++)
        {
            tmp->typeMouseEvent.positionSquareLU[i] = 0;
            tmp->typeMouseEvent.positionSquareRL[i] = 0;
        }

        ParseInputDefinitions(middle, &defs);

        GetPositionDefinitionKeyValue(&defs, L"PLU", (int32*)&tmp->typeMouseEvent.positionSquareLU[0], (int32*)&tmp->typeMouseEvent.positionSquareLU[1]);
        GetPositionDefinitionKeyValue(&defs, L"PRL", (int32*)&tmp->typeMouseEvent.positionSquareRL[0], (int32*)&tmp->typeMouseEvent.positionSquareRL[1]);

        return tmp;
    }
    // keyboard press event element
    else if (EqualString(left, L"\\KEY_PRESS", true) || EqualString(left, L"\\KEY_RELEASE", true))
    {
        uint16 key = 0;
        if (right != NULL)
        {
            if (IsNumeric(right))
                key = ToInt(right);
            else
                key = ResolveKey(right);
        }

        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_KEYBOARD_EVENT;
        if (EqualString(left, L"\\KEY_PRESS", true))
            tmp->typeKeyboardEvent.type = KEYBOARD_EVENT_KEY_DOWN;
        else
            tmp->typeKeyboardEvent.type = KEYBOARD_EVENT_KEY_UP;
        tmp->typeKeyboardEvent.key = key;

        return tmp;
    }
    // clear everything from screen
    else if (EqualString(left, L"\\NEW_SLIDE", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_NEW_SLIDE;
        tmp->typeNewSlide.type = SST_NONE;

        if (right)
            left = LeftSide(right, L' ');

        if (left && right)
        {
            if (EqualString(left, L"FADE", true))
            {
                // canvas effect --> colorize 100% opacity, delete elements, move on, decolorize

                tmp->elemType = SLIDE_ELEM_CANVAS_EFFECT;

                tmp->typeCanvasEffect.hard = true;
                tmp->typeCanvasEffect.effectType = CE_COLORIZE;
                tmp->typeCanvasEffect.effectTimer = 0;
                tmp->typeCanvasEffect.effProgress = EP_LINEAR;

                uint32 effTimer = 0;
                uint8 progress = EP_LINEAR;

                right = RightSide(right, L' ');
                while (right)
                {
                    left = LeftSide(right, L' ');

                    if (EqualString(left, L"quadratic", true))
                        progress = EP_QUADRATIC;
                    else if (EqualString(left, L"sinus", true))
                        progress = EP_SINUS;
                    else if (EqualString(left, L"linear", true))
                        progress = EP_LINEAR;
                    else if (IsNumeric(left))
                        effTimer = ToInt(left);
                    else
                    {
                        if (StyleParser::ParseColor(left, &tmp->typeCanvasEffect.amount.asUnsigned))
                            tmp->typeCanvasEffect.amount.asUnsigned |= COLOR_A(255);
                        else
                            RAISE_ERROR_NULL("SlideParser: invalid input '%S' in new slide definition", left);
                    }

                    right = RightSide(right, L' ');
                }

                tmp->typeCanvasEffect.effProgress = progress;
                tmp->typeCanvasEffect.effectTimer = effTimer;

                sStorage->AddSlideElement(tmp);

                tmp = new SlideElement;
                tmp->elemType = SLIDE_ELEM_BLOCK;
                tmp->typeBlock.time = effTimer;
                tmp->typeBlock.passthrough = true;

                sStorage->AddSlideElement(tmp);

                tmp = new SlideElement;
                tmp->elemType = SLIDE_ELEM_CANVAS_EFFECT;
                tmp->typeCanvasEffect.hard = false;
                tmp->typeCanvasEffect.amount.asUnsigned = MAKE_COLOR_RGBA(255, 255, 255, 0);
                tmp->typeCanvasEffect.effectType = CE_COLORIZE;
                tmp->typeCanvasEffect.effProgress = progress;
                tmp->typeCanvasEffect.effectTimer = effTimer;

                sStorage->AddSlideElement(tmp);

                tmp = new SlideElement;
                tmp->elemType = SLIDE_ELEM_NEW_SLIDE;
                tmp->typeNewSlide.type = SST_FADE;

                return tmp;
            }
            else if (EqualString(left, L"MOVE", true))
            {
                // create fake effect for every element on slide to allow generic reverting
                tmp->typeNewSlide.type = SST_MOVE;
            }
            else if (EqualString(left, L"DISPERSE", true))
            {
                // same as move --> move every element to different direction
                tmp->typeNewSlide.type = SST_DISPERSE;
            }
        }

        return tmp;
    }
    // play effect on specified element
    else if (EqualString(left, L"\\PLAY_EFFECT", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_PLAY_EFFECT;

        ParseInputDefinitions(middle, &defs);

        tmp->elemId = GetDefinitionKeyValue(&defs, L"ID");
        tmp->elemEffect = GetDefinitionKeyValue(&defs, L"E");

        return tmp;
    }
    // play canvas effect - move, rotate, scale
    else if (EqualString(left, L"\\CANVAS_MOVE", true) || EqualString(left, L"\\CANVAS_ROTATE", true) || EqualString(left, L"\\CANVAS_SCALE", true) || EqualString(left, L"\\CANVAS_RESET", true) || EqualString(left, L"\\CANVAS_COLORIZE", true))
    {
        tmp = new SlideElement;
        tmp->elemType = SLIDE_ELEM_CANVAS_EFFECT;

        tmp->typeCanvasEffect.hard = false;
        tmp->typeCanvasEffect.effectTimer = 0;
        tmp->typeCanvasEffect.effProgress = EP_LINEAR;

        if (EqualString(left, L"\\CANVAS_MOVE", true))
        {
            tmp->typeCanvasEffect.effectType = CE_MOVE;

            // parameter sequence:
            // 1. move vector [px] (i.e. 50,10)
            // 2. effect timer [ms] (i.e. 1000)
            // 3. any other definitions in any order

            // at least move vector is required

            left = LeftSide(right, L' ');

            float* movevect = ParseVector2(left, L',');
            if (!movevect)
                RAISE_ERROR_NULL("SlideParser: invalid movement vector '%S' in canvas move definition", left?ToMultiByteString(left):"");

            tmp->typeCanvasEffect.moveVector.x = movevect[0];
            tmp->typeCanvasEffect.moveVector.y = movevect[1];
        }
        else if (EqualString(left, L"\\CANVAS_ROTATE", true))
        {
            tmp->typeCanvasEffect.effectType = CE_ROTATE;

            // parameter sequence:
            // 1. rotate angle [degrees] (i.e. 180)
            // 2. effect timer [ms]
            // 3. any other definitions in any order

            left = LeftSide(right, L' ');

            if (!IsNumeric(left))
                RAISE_ERROR_NULL("SlideParser: invalid angle value '%S' in canvas rotate definition", left?ToMultiByteString(left):"");

            tmp->typeCanvasEffect.amount.asFloat = (float)ToInt(left);

            // implicit rotation center is in the middle of the screen
            tmp->typeCanvasEffect.moveVector = CVector2(sStorage->GetOriginalScreenWidth() / 2.0f, sStorage->GetOriginalScreenHeight() / 2.0f);
        }
        else if (EqualString(left, L"\\CANVAS_SCALE", true))
        {
            tmp->typeCanvasEffect.effectType = CE_SCALE;

            // parameter sequence:
            // 1. scale [%] (i.e. 200)
            // 2. effect timer [ms]
            // 3. any other definitions in any order

            left = LeftSide(right, L' ');

            if (!IsNumeric(left))
                RAISE_ERROR_NULL("SlideParser: invalid scale value '%S' in canvas scale definition", left?ToMultiByteString(left):"");

            tmp->typeCanvasEffect.amount.asFloat = (float)ToInt(left);
        }
        else if (EqualString(left, L"\\CANVAS_COLORIZE", true))
        {
            tmp->typeCanvasEffect.effectType = CE_COLORIZE;

            // parameter sequence:
            // 1. color
            // 2. effect timer [ms]
            // 3. any other definitions in any order

            left = LeftSide(right, L' ');

            if (!StyleParser::ParseColor(left, &tmp->typeCanvasEffect.amount.asUnsigned))
                RAISE_ERROR_NULL("SlideParser: unrecognized know value or invalid color value '%S' in canvas rotate definition", left?ToMultiByteString(left):"");

            // remove alpha byte
            tmp->typeCanvasEffect.amount.asUnsigned &= 0xFFFFFF00;
            // and put there our implicit value
            tmp->typeCanvasEffect.amount.asUnsigned |= 0x40;
        }
        else if (EqualString(left, L"\\CANVAS_RESET", true))
        {
            tmp->typeCanvasEffect.effectType = CE_RESET;
        }

        if (tmp->typeCanvasEffect.effectType != CE_RESET)
            right = RightSide(right, L' ');

        if (right)
        {
            left = LeftSide(right, L' ');
            if (!IsNumeric(left))
                RAISE_ERROR_NULL("SlideParser: invalid value '%s' used as timer for canvas move effect", left?ToMultiByteString(left):"");

            tmp->typeCanvasEffect.effectTimer = ToInt(left);

            right = RightSide(right, L' ');
            do
            {
                left = LeftSide(right, L' ');
                if (left)
                {
                    if (EqualString(left, L"hard", true))
                        tmp->typeCanvasEffect.hard = true;
                    else if (EqualString(left, L"linear", true))
                        tmp->typeCanvasEffect.effProgress = EP_LINEAR;
                    else if (EqualString(left, L"sinus", true))
                        tmp->typeCanvasEffect.effProgress = EP_SINUS;
                    else if (EqualString(left, L"quadratic", true))
                        tmp->typeCanvasEffect.effProgress = EP_QUADRATIC;
                    else if (IsNumeric(left))
                    {
                        if (tmp->typeCanvasEffect.effectType == CE_COLORIZE)
                        {
                            // clear alpha byte
                            tmp->typeCanvasEffect.amount.asUnsigned &= 0xFFFFFF00;
                            // and put there user defined value, also ensure, that it is from 0 to 100 percent, so we can convert it to 0 to 255 safely
                            uint32 value = ToInt(left);
                            if (value > 100)
                                value = 100;
                            tmp->typeCanvasEffect.amount.asUnsigned |= uint8((0xFF)*(float(value)/100.0f));
                        }
                    }
                    else
                    {
                        float* vec = ParseVector2(left, L',');
                        if (vec)
                        {
                            // relative rotation center

                            if (tmp->typeCanvasEffect.effectType == CE_ROTATE)
                            {
                                tmp->typeCanvasEffect.moveVector.x = vec[0];
                                tmp->typeCanvasEffect.moveVector.y = vec[1];
                            }
                        }
                        else
                            RAISE_ERROR_NULL("SlideParser: unknown definition '%s' as canvas move parameter", left?ToMultiByteString(left):"");
                    }
                }
            } while ((right = RightSide(right, L' ')) != NULL);
        }

        // we don't need to draw hard canvas effects - they are copied to global variables
        if (!tmp->typeCanvasEffect.hard)
            tmp->drawable = true;

        return tmp;
    }
    // call template
    else if (EqualString(left, L"\\TEMPLATE_CALL", true))
    {
        SlideTemplate* mytmp = sStorage->GetSlideTemplate(right);

        if (!mytmp)
            RAISE_ERROR_NULL("SlideParser: couldn't find template named '%S'!",((right != NULL)?right:L"unknown"));

        if (persistentIdentificator == NULL)
        {
            if (special)
                (*special) = SEPF_ONLY_TEMPLATE;
            return NULL;
        }

        // this causes marking of last added slide element as last overwritten
        // allows us to add raw elements to template
        sStorage->UpdateLastOverwritten(L"");

        (*persistentIdentificator) = new wchar_t[wcslen(right)+1];
        memset((*persistentIdentificator), 0, sizeof(wchar_t)*(wcslen(right)+1));
        wcsncpy((*persistentIdentificator), right, wcslen(right));

        SlideElement* ts = NULL;
        uint32 len = 0;
        for (SlideElementVector::iterator itr = mytmp->m_elements.begin(); itr != mytmp->m_elements.end(); ++itr)
        {
            ts = new SlideElement;
            memcpy(ts, (*itr), sizeof(SlideElement));

            len = wcslen((*itr)->elemId)+wcslen(TEMPLATE_ID_DELIMITER)+wcslen((*persistentIdentificator))+1;
            ts->elemId = new wchar_t[len];
            memset(ts->elemId, 0, len);
            swprintf(ts->elemId, 256, L"%s%s%s", (*itr)->elemId, TEMPLATE_ID_DELIMITER, (*persistentIdentificator));

            sStorage->AddSlideElement(ts);

            if (ts->elemType == SLIDE_ELEM_TEXT)
                sStorage->AddPostParseElement(ts);
        }

        if (special)
            (*special) = SEPF_ONLY_TEMPLATE;

        return NULL;
    }

    (*special) |= SEPF_REPORT_ERROR;
    return NULL;
}

uint16 SlideParser::ResolveKey(const wchar_t* input)
{
    if (!input)
        return 0;

    for (uint32 i = 0; i < sizeof(KnownKeys)/sizeof(KnownKey); i++)
    {
        if (EqualString(input, KnownKeys[i].name, true))
            return KnownKeys[i].code;
    }

    return 0;
}

void SlideParser::ParseMarkup(const wchar_t *input, const wchar_t* stylename, StyledTextList *target, ExprMap* exmap)
{
    if (!input)
        return;

    Style* defstyle = NULL;
    if (stylename)
        defstyle = sStorage->GetStyle(stylename);

    if (defstyle == NULL)
        defstyle = sStorage->GetDefaultStyle();

    Style* origstyle = defstyle;

    printTextData* tmp = NULL;
    wchar_t ident = '\0';

    // valid markups: {B}, {I}, {U}, {X}, {S:style_name}, or {$expression}
    uint32 len = wcslen(input);

    uint32 lastTextBegin = 0;

    uint32 i;
    for (i = 0; i < len; )
    {
        // expression
        if (i < len-2 && input[i] == L'{' && input[i+1] == L'$')
        {
            for (uint32 j = i+2; j < len; j++)
            {
                if (input[j] == L'}')
                {
                    if (!target)
                        target = new StyledTextList;
                    // delay markup build
                    if (!defstyle)
                    {
                        target->clear();
                        return;
                    }

                    tmp = new printTextData;

                    tmp->fontId = defstyle->fontId;

                    tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                    tmp->colorize = !(defstyle->fontColor == NULL);
                    if (tmp->colorize)
                        tmp->color = (*(defstyle->fontColor));

                    tmp->text = new wchar_t[i-lastTextBegin+1];
                    memset(tmp->text, 0, sizeof(wchar_t)*(i-lastTextBegin+1));
                    wcsncpy(tmp->text, &(input[lastTextBegin]), i-lastTextBegin);

                    target->push_back(tmp);

                    tmp = new printTextData;
                    tmp->fontId = defstyle->fontId;
                    tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                    tmp->colorize = !(defstyle->fontColor == NULL);
                    if (tmp->colorize)
                        tmp->color = (*(defstyle->fontColor));

                    tmp->text = new wchar_t[j-i-1];
                    memset(tmp->text, 0, sizeof(wchar_t)*(j-i-1));
                    wcsncpy(tmp->text, &(input[i+2]), j-i-2);

                    target->push_back(tmp);

                    ExpressionVector* exvec = ExpressionParser::Parse(tmp->text);
                    ExpressionTreeElement* elmnt = ExpressionParser::BuildTree(exvec, 0, exvec->size());
                    elmnt->SimplifyChildren();
                    ((*exmap)[target->size()-1]) = *elmnt;

                    i = j+1;
                    lastTextBegin = i;
                    goto outer_continue;
                }
            }
        }
        // char code
        if (i < len-2 && input[i] == L'{' && input[i+1] == L'%')
        {
            for (uint32 j = i+2; j < len; j++)
            {
                if (input[j] == L'}')
                {
                    if (!target)
                        target = new StyledTextList;

                    // delay markup build
                    if (!defstyle)
                    {
                        target->clear();
                        return;
                    }

                    tmp = new printTextData;

                    tmp->fontId = defstyle->fontId;

                    tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                    tmp->colorize = !(defstyle->fontColor == NULL);
                    if (tmp->colorize)
                        tmp->color = (*(defstyle->fontColor));

                    tmp->text = new wchar_t[i-lastTextBegin+1];
                    memset(tmp->text, 0, sizeof(wchar_t)*(i-lastTextBegin+1));
                    wcsncpy(tmp->text, &(input[lastTextBegin]), i-lastTextBegin);

                    target->push_back(tmp);

                    tmp = new printTextData;
                    tmp->fontId = defstyle->fontId;
                    tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                    tmp->colorize = !(defstyle->fontColor == NULL);
                    if (tmp->colorize)
                        tmp->color = (*(defstyle->fontColor));

                    wchar_t* codestr = new wchar_t[j-i-1];
                    memset(codestr, 0, sizeof(wchar_t)*(j-i-1));
                    wcsncpy(codestr, &(input[i+2]), j-i-2);

                    tmp->text = new wchar_t[2];
                    memset(tmp->text, 0, sizeof(wchar_t)*2);
                    if (IsNumeric(codestr))
                        tmp->text[0] = (wchar_t)ToInt(codestr);

                    target->push_back(tmp);

                    i = j+1;
                    lastTextBegin = i;
                    goto outer_continue;
                }
            }
        }
        // opening tag (without '/' at second position)
        if (i < len-2 && input[i] == L'{' && input[i+1] != L'/' && (input[i+2] == L'}' || input[i+2] == L':'))
        {
            ident = input[i+1];
            if (ident == L'B' || ident == L'I' || ident == L'U' || ident == L'X' || (ident == L'S' && input[i+2] == L':'))
            {
                if (!target)
                    target = new StyledTextList;
                // delay markup build
                if (!defstyle)
                {
                    target->clear();
                    return;
                }

                tmp = new printTextData;

                tmp->fontId = defstyle->fontId;

                tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                tmp->colorize = !(defstyle->fontColor == NULL);
                if (tmp->colorize)
                    tmp->color = (*(defstyle->fontColor));

                tmp->text = new wchar_t[i-lastTextBegin+1];
                memset(tmp->text, 0, sizeof(wchar_t)*(i-lastTextBegin+1));
                wcsncpy(tmp->text, &(input[lastTextBegin]), i-lastTextBegin);

                target->push_back(tmp);

                if (ident == L'B')
                {
                    defstyle->bold = true;
                    origstyle->bold = true;
                }
                else if (ident == L'I')
                {
                    defstyle->italic = true;
                    origstyle->italic = true;
                }
                else if (ident == L'U')
                {
                    defstyle->underline = true;
                    origstyle->underline = true;
                }
                else if (ident == L'X')
                {
                    defstyle->strikeout = true;
                    origstyle->strikeout = true;
                }
                else if (ident == L'S')
                {
                    const wchar_t* stname = LeftSide(&(input[i+3]), L'}');
                    if (stname)
                        defstyle = sStorage->GetStyle(stname);

                    i += 3 + 1 + wcslen(stname);
                }

                if (ident != L'S')
                    i += 3;

                lastTextBegin = i;
                continue;
            }
            i++;
            continue;
        }
        // closing tag (with '/' at second position)
        else if (i < len-2 && input[i] == L'{' && input[i+1] == L'/' && input[i+3] == L'}')
        {
            ident = input[i+2];
            if (ident == L'B' || ident == L'I' || ident == L'U' || ident == L'X' || ident == L'S')
            {
                if (!target)
                    target = new StyledTextList;
                // delay markup build
                if (!defstyle)
                {
                    target->clear();
                    return;
                }

                tmp = new printTextData;

                tmp->fontId = defstyle->fontId;

                tmp->feature = SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle);
                tmp->colorize = !(defstyle->fontColor == NULL);
                if (tmp->colorize)
                    tmp->color = (*(defstyle->fontColor));

                tmp->text = new wchar_t[i-lastTextBegin+1];
                memset(tmp->text, 0, sizeof(wchar_t)*(i-lastTextBegin+1));
                wcsncpy(tmp->text, &(input[lastTextBegin]), i-lastTextBegin);

                target->push_back(tmp);

                if (ident == L'B')
                {
                    defstyle->bold = false;
                    origstyle->bold = false;
                }
                else if (ident == L'I')
                {
                    defstyle->italic = false;
                    origstyle->italic = false;
                }
                else if (ident == L'U')
                {
                    defstyle->underline = false;
                    origstyle->underline = false;
                }
                else if (ident == L'X')
                {
                    defstyle->strikeout = false;
                    origstyle->strikeout = false;
                }
                else if (ident == L'S')
                    defstyle = origstyle;

                i += 4;
                lastTextBegin = i;
                continue;
            }
            i++;
            continue;
        }
        i++;
outer_continue:;
    }

    if (lastTextBegin != i)
    {
        tmp = new printTextData;

        tmp->fontId = (defstyle != NULL) ? defstyle->fontId : sStorage->GetDefaultFontId();

        // If some of fonts hasn't been rendered yet, invalidate the whole printlist and delay its generation
        if (tmp->fontId < 0)
        {
            delete tmp;
            target->clear();
            return;
        }

        tmp->feature = (defstyle != NULL) ? SlideElement::elemTextData::GetFeatureArrayIndexOf(defstyle) : 0;
        tmp->colorize = (defstyle != NULL) ? !(defstyle->fontColor == NULL) : false;
        if (tmp->colorize)
            tmp->color = (*(defstyle->fontColor));

        tmp->text = new wchar_t[i-lastTextBegin+1];
        memset(tmp->text, 0, sizeof(wchar_t)*(i-lastTextBegin+1));
        wcsncpy(tmp->text, &(input[lastTextBegin]), i-lastTextBegin);

        target->push_back(tmp);
    }
}
