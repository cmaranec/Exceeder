#include "Global.h"
#include "Log.h"
#include "Storage.h"
#include "Parsers\StyleParser.h"
#include "Defines\Slides.h"
#include "Defines\Styles.h"
#include "Defines\Effects.h"

void SlideElement::CreateEffectIfAny()
{
    myEffect = NULL;

    if (elemEffect.size() > 0)
    {
        Effect* tmp = sStorage->GetEffect(elemEffect.c_str());
        if (tmp)
            myEffect = new EffectHandler(this, tmp);
    }
}

void SlideElement::Draw()
{
    if (myEffect && !myEffect->isExpired())
        myEffect->Animate();

    switch (elemType)
    {
        case SLIDE_ELEM_TEXT:
            typeText.Draw(this);
            break;
        case SLIDE_ELEM_IMAGE:
            typeImage.Draw(this);
        default:
            break;
    }
}

void SlideElement::elemTextData::Draw(SlideElement* parent)
{
    if (parent->elemStyle.size() > 0)
    {
        Style* myStyle = sStorage->GetStyle(parent->elemStyle.c_str());

        // Set color if any
        if (myStyle->fontColor)
        {
            uint32 color = (*(myStyle->fontColor));
            glColor3ub(COLOR_R(color),COLOR_G(color),COLOR_B(color));
        }

        // draw text with own font. If not set, use default font
        if (myStyle->fontId >= 0)
            sSimplyFlat->Drawing->PrintText(myStyle->fontId, parent->position[0], parent->position[1], parent->typeText.text.c_str());
        else
            sSimplyFlat->Drawing->PrintText(sStorage->GetDefaultFontId(), parent->position[0], parent->position[1], parent->typeText.text.c_str());

        // Set color back to white if necessary
        if (myStyle->fontColor)
            glColor3ub(255, 255, 255);
    }
}

void SlideElement::elemImageData::Draw(SlideElement* parent)
{
    if (parent->typeImage.resourceId > 0)
    {
        ResourceEntry* res = sStorage->GetResource(parent->typeImage.resourceId);
        if (res && res->image)
            sSimplyFlat->Drawing->DrawRectangle(parent->position[0], parent->position[1], parent->typeImage.size[0], parent->typeImage.size[1], 0, res->image->textureId);
    }
}