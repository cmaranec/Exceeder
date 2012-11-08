#ifndef EXCDR_PRESENTATION_H
#define EXCDR_PRESENTATION_H

#include "Global.h"
#include "Log.h"
#include "Storage.h"
#include "Parsers/StyleParser.h"
#include "Defines/Slides.h"
#include "Defines/Styles.h"
#include "Defines/Effects.h"

enum InterfaceEventTypes
{
    IE_MOUSE_LEFT_DOWN      = 0,
    IE_MOUSE_LEFT_UP        = 1,
    IE_MOUSE_RIGHT_DOWN     = 2,
    IE_MOUSE_RIGHT_UP       = 3,
    IE_MOUSE_MOVE           = 4,
    IE_KEYBOARD_PRESS       = 5,
    IE_KEYBOARD_RELEASE     = 6,
    IE_EFFECT_END           = 7,
    IE_MAX
};

// We have to define this since on Linux we handle the cycle with internal GLUT loop unlike on Windows - on Windows, we have our own cycle
#ifdef _WIN32
#define PRESENTATION_CONTINUE continue
#define PRESENTATION_BREAK break
#else
#define PRESENTATION_CONTINUE return
#define PRESENTATION_BREAK return
#endif

typedef std::list<SlideElement*> SlideList;

class PresentationMgr
{
    public:
        PresentationMgr();

        bool Init();
        void Run();

        void InterfaceEvent(InterfaceEventTypes type, int32 param1 = 0, int32 param2 = 0);

        void SetBlocking(bool block) { m_blocking = block; };
        bool IsBlocking() { return m_blocking; };

        SlideElement* GetActiveElementById(const wchar_t* id);

    private:
        uint32 m_slideElementPos;
        SlideElement* m_slideElement;
        SlideList m_activeElements;

        struct BackgroundData
        {
            uint32 color;
            uint32 resourceId;             // NYI, ID of image
            uint32 backgroundPosition[2];  // NYI, position of image
            // TODO: image positioning and options
        } bgData;

        bool m_blocking;
};

#define sPresentation Singleton<PresentationMgr>::instance()

#endif
