#include"engine.h"
namespace rawinput
{
    bool enabled = false;
    VAR(debugrawmouse, 0, 0, 1);
    SVARFP(rawmouse, "", { pick(rawmouse); });
//    ICOMMAND(listrawdevices, "", (), listdevices());
    // event interface common to raw input systems
    // provides a thread-safe buffered event system
    enum
    {
        REV_MOTION = 0,
        REV_BUTTON = 1
    };
    struct rawevent
    {
        int type; // one of REV_MOTION, REV_BUTTON
        union
        {
            struct { int dx, dy; };
            struct { int button, state; };
        };
        rawevent(int t, int data0, int data1) : type(t)
        {
            switch(t)
            {
            case REV_MOTION:
                dx = data0;
                dy = data1;
                break;
            case REV_BUTTON:
                button = data0;
                state = data1;
                break;
            }
        }
    };
    static vector<rawevent> buffer_a, buffer_b;
    static vector<rawevent> *frontbuffer = &buffer_a, *backbuffer = &buffer_b;
    static SDL_mutex *bufferlock = NULL;
    void os_release(); // should be idempotent
    // free resources, idempotent
    void release()
    {
        os_release();
        if(bufferlock) SDL_DestroyMutex(bufferlock);
        bufferlock = NULL;
        enabled = false;
    }
    int os_pick(const char *name);
    void pick(const char *name)
    {
        release();
        if(!*name) return;
        bufferlock = SDL_CreateMutex();
        if(!bufferlock)
        {
            conoutf(CON_ERROR, "Couldn't create mutex for raw input buffers");
            return;
        }
        if(os_pick(name)) enabled = true; //FIXME offer "*" as catch-all name
        else
        {
            release();
            conoutf(CON_ERROR, "Couldn't open any raw input devices matching \"%s\"", name);
        }
    }
    // clear the processing (front) buffer and swap it with the
    // accumulation (back) buffer.
    //
    // return the former accumulation buffer.
    //
    // thread-safe, but the returned buffer becomes thread-DANGEROUS
    // when swapevents() is called again.
    //
    // correct usage is therefore to call swapevents(),
    // then use and discard the buffer before calling it again.
    // e.g.
    // for(;;) process_events(swapevents());
    vector<rawevent> *swapevents()
    {
        vector<rawevent> *trash = frontbuffer;
        trash->setsize(0);
        SDL_LockMutex(bufferlock);
        frontbuffer = backbuffer;
        backbuffer = trash;
        SDL_UnlockMutex(bufferlock);
        return frontbuffer;
    }
    // add an event to queue; completely thread-safe
    void addevent(rawevent ev)
    {
        SDL_LockMutex(bufferlock);
        backbuffer->add(ev);
        SDL_UnlockMutex(bufferlock);
    }
    // process pending events, call this as part of main loop
    void flush()
    {
        vector<rawevent> *evs = swapevents();
        loopv(*evs)
        {
            rawevent &ev = (*evs)[i];
            if(debugrawmouse)
            {
                const char *names[] = { "REV_MOTION", "REV_BUTTON" };
                conoutf("%d rawevent: %s %d %d", lastmillis, names[ev.type], ev.dx, ev.dy);
            }
            switch(ev.type)
            {
            case REV_MOTION:
                if(!g3d_movecursor(ev.dx, ev.dy)) mousemove(ev.dx, ev.dy);
                break;
            case REV_BUTTON:
                keypress(ev.button, ev.state, 0);
                break;
            }
        }
    }
    // device listing helper
    static vector<char> devicenames;
    bool listdevice(const char *name)
    {
        if(!devicenames.empty()) devicenames.add(' ');
        devicenames.add('[');
        devicenames.put(name, strcspn(name, "[]"));
        devicenames.add(']');
        return false;
    }
}

namespace rawinput
{
#ifdef WIN32
////////////////////////////////////////////////////////////////////////////////
// Windows raw input handling (WM_INPUT)
////////////////////////////////////////////////////////////////////////////////
#include<windows.h>
#include"SDL_syswm.h"
    struct windev
    {
        HANDLE device;
        DWORD type;
        string name;
    };
    // open registry key corresponding to device name as returned from GetRawInputDeviceInfo
    // return true on successful open
    bool opendevicekey(char *name, HKEY *handle)
    {
        // convert name to a registry path for the device
        static char path[0x200];
        const char *base = "SYSTEM\\CurrentControlSet\\Enum\\";
        // skip leading '?' and '\\' chars
        while(*name && (*name == '\\' || *name == '?')) name++;
        // copy base path
        uint i, k, p;
        for(i = 0; base[i] && i < sizeof(path); i++) path[i] = base[i];
        // copy name, converting '#' characters to '\\' and stopping at 3rd '#' character
        for(k = 0, p = 0; name[k] && i < sizeof(path); ++i && ++k)
        {
            char c = name[k];
            if(c == '#')
            {
                if(++p > 2) break;
                path[i] = '\\';
            }
            else path[i] = c;
        }
        // sentinel
        path[i] = '\0';
        LONG open
            = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                           path,
                           0,
                           KEY_READ,
                           handle);
        return open == ERROR_SUCCESS;
    }
    // fill in info from registry for a windev, dev.device must be set
    void loadregistryinfo(windev &dev)
    {
        *dev.name = '\0';
        static string rawname;
        UINT rawlen = sizeof(rawname);
        UINT tryinfo = GetRawInputDeviceInfo(dev.device, RIDI_DEVICENAME, &rawname, &rawlen);
        if(tryinfo == (UINT)-1) return;
        HKEY regkey;
        if(!opendevicekey(rawname, &regkey)) return;
        DWORD regtype;
        ULONG size = sizeof(dev.name);
        RegQueryValueExA(regkey, "DeviceDesc", NULL, &regtype, (LPBYTE)dev.name, &size);
        RegCloseKey(regkey);
        if(size > 0) dev.name[size-1] = '\0';
    }
    // returns no. of devices listed
    int listdevices(vector<windev> &devs, bool (*filter)(windev &))
    {
        devs.setsize(0);
        PRAWINPUTDEVICELIST rids;
        UINT numrids, trylist;
        trylist = GetRawInputDeviceList(NULL, &numrids, sizeof(RAWINPUTDEVICELIST));
        if(trylist == (UINT)-1) return -1;
        rids = new RAWINPUTDEVICELIST[numrids];
        if(!rids) return -1;
        trylist = GetRawInputDeviceList(rids, &numrids, sizeof(RAWINPUTDEVICELIST));
        if(trylist == (UINT)-1)
        {
            delete[] rids;
            return -1;
        }
        windev candidate;
        loopi(numrids)
        {
            candidate.device = rids[i].hDevice;
            candidate.type = rids[i].dwType;
            loadregistryinfo(candidate);
            if(filter(candidate)) devs.add(candidate);
        }
        return numrids;
    }
#define USAGE_PAGE_GENERIC_DESKTOP 0x01
#define USAGE_MOUSE 0x02
#define USAGE_KEYBOARD 0x06
    // true on successful (un)registration
    bool registerdevice(DWORD usage, HWND window, DWORD flags)
    {
        RAWINPUTDEVICE rid;
        rid.usUsagePage = USAGE_PAGE_GENERIC_DESKTOP;
        rid.usUsage = usage;
        rid.dwFlags = flags;
        rid.hwndTarget = window;
        return (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == TRUE);
    }

    struct buttonmap
    {
        USHORT windown;
        USHORT winup;
        int translate;
    };
#define WINBUTTON(x) RI_MOUSE_##x##_DOWN, RI_MOUSE_##x##_UP
    static const buttonmap lookup[] =
    { { WINBUTTON(LEFT_BUTTON), -SDL_BUTTON_LEFT },
      { WINBUTTON(RIGHT_BUTTON), -SDL_BUTTON_RIGHT },
      { WINBUTTON(MIDDLE_BUTTON), -SDL_BUTTON_MIDDLE },
      { WINBUTTON(BUTTON_4), -SDL_BUTTON_X1 },
      { WINBUTTON(BUTTON_5), -SDL_BUTTON_X2 }
    };
    static const int numbuttons = sizeof(lookup) / sizeof(buttonmap);

    // add events to queue from a RAWMOUSE event
    void handlemouse(RAWMOUSE &ev)
    {
        if((ev.usFlags & MOUSE_MOVE_RELATIVE) == MOUSE_MOVE_RELATIVE)
        {
            if(!g3d_movecursor(ev.lLastX, ev.lLastY)) mousemove(ev.lLastX, ev.lLastY);
        }
        for (int i = 0; i < numbuttons; i++)
        {
            if(ev.usButtonFlags & lookup[i].windown) addevent(rawevent(REV_BUTTON, lookup[i].translate, 1));
            else if(ev.usButtonFlags & lookup[i].winup) addevent(rawevent(REV_BUTTON, lookup[i].translate, 0));
        }
    }

    // chosen devices
    static vector<windev> devices;
    // return true if raw input was handled
    bool readrawinput(LPARAM lparam)
    {
        RAWINPUT raw;
        UINT size = sizeof(raw);
        // read into buffer
        UINT read =
            GetRawInputData((HRAWINPUT)lparam,
                            RID_INPUT,
                            &raw,
                            &size,
                            sizeof(RAWINPUTHEADER));
        if(read == (UINT)-1)
        {
            conoutf(CON_ERROR, "failed to read raw input data");
            return false;
        }
        if(read > size || read < sizeof(RAWINPUTHEADER))
        {
            conoutf(CON_ERROR, "bad size for raw input data (%d)\n", read);
            return false;
        }
        if(raw.header.dwType != RIM_TYPEMOUSE) return false;
        bool care = false;
        loopv(devices) if(devices[i].device == raw.header.hDevice)
        {
            care = true;
            break;
        }
        if(care) handlemouse(raw.data.mouse);
        return care;
    }

    static WNDPROC basewndproc = NULL;
    LRESULT CALLBACK rawwndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if(msg==WM_INPUT) readrawinput(lparam);
        if(!basewndproc)
        {
            conoutf(CON_ERROR, "!!! something is very wrong (base wndproc missing) !!!");
            return DefWindowProc(hwnd, msg, wparam, lparam);
        }
        else
        {
            return CallWindowProc(basewndproc, hwnd, msg, wparam, lparam);
        }
    }

    HWND gethwnd()
    {
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        switch(SDL_GetWMInfo(&info))
        {
        case -1:
            conoutf(CON_ERROR, "failed to get window handle");
        case 0:
            conoutf(CON_ERROR, "no support for system window manager information");
            return NULL;
        default:
            return info.window;
        }
    }

    bool namefilter(windev &dev)
    {
        bool use = strstr(dev.name, rawmouse);
        if(use) conoutf("Listening to raw device %s", dev.name);
        return use;
    }

    int os_pick(const char *name)
    {
        if(enabled) return devices.length();
        int count = listdevices(devices, namefilter);
        if(!count) return 0;
        HWND handle = gethwnd();
        if(!handle) return 0;
        if(registerdevice(USAGE_MOUSE, handle, RIDEV_NOLEGACY))
        {
            basewndproc = (WNDPROC)SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR)rawwndproc);
            return count;
        }
        devices.setsize(0);
        return 0;
    }
    void os_release()
    {
        HWND handle = gethwnd();
        if(!handle) return;
        if(!registerdevice(USAGE_MOUSE, handle, RIDEV_REMOVE))
        {
            conoutf(CON_ERROR, "failed to unregister raw mouse");
            return;
        }
        if(basewndproc != NULL)
        {
            if(SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR)basewndproc)) basewndproc = NULL;
        }
    }
#elif linux
////////////////////////////////////////////////////////////////////////////////
// Linux raw input handling (/dev/input/event*)
////////////////////////////////////////////////////////////////////////////////
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<poll.h>
#include<linux/input.h>
    namespace thread
    {
        static const short readflags = POLLIN|POLLPRI;
        static SDL_Thread *thread = NULL;
        static SDL_mutex *reins = NULL;
        // must have reins to safely read/write cease
        static bool cease = false;
        // must stop thread to safely read/write fds
        static vector<struct pollfd> fds;
        void closefds()
        {
            loopv(fds)
            {
                close(fds[i].fd);
            }
            fds.setsize(0);
        }
        struct buttonmap
        {
            __u16 native;
            int translate;
        };
        static const buttonmap lookup[] =
        { { BTN_LEFT, -SDL_BUTTON_LEFT },
          { BTN_RIGHT, -SDL_BUTTON_RIGHT },
          { BTN_MIDDLE, -SDL_BUTTON_MIDDLE },
          { BTN_SIDE, -SDL_BUTTON_X1 },
          { BTN_EXTRA, -SDL_BUTTON_X2 }
        };
        static const int numbuttons = sizeof(lookup) / sizeof(buttonmap);

        void event(struct input_event &ev)
        {
            switch(ev.type)
            {
            case EV_REL:
                // relative events occur one axis at a time
                switch(ev.code)
                {
                case REL_X:
                    addevent(rawevent(REV_MOTION, ev.value, 0));
                    break;
                case REL_Y:
                    addevent(rawevent(REV_MOTION, 0, ev.value));
                    break;
                }
                break;
            case EV_KEY:
                // search in lookup table for a corresponding button code to emit
                loopi(numbuttons) if(lookup[i].native == ev.code)
                {
                    addevent(rawevent(REV_BUTTON, lookup[i].translate, ev.value));
                    break;
                }
            }
        }
        int worker(void *unused)
        {
            struct input_event ev;
            struct timespec tv;
            tv.tv_sec = 0;
            tv.tv_nsec = 50000000;
            bool stop = false;
            while(!stop)
            {
                int sel = ppoll(fds.getbuf(), fds.length(), &tv, NULL);
                SDL_LockMutex(reins);
                stop = cease;
                if(sel < 0) stop = true; // poll error
                else if(sel > 0) // something happened!
                {
                    loopv(fds)
                    {
                        if(!(fds[i].revents & readflags)) continue;
                        int rd = read(fds[i].fd, &ev, sizeof(struct input_event));
                        if(rd < 0)
                        {
                            stop = true; // read error
                            break;
                        }
                        else if(rd == sizeof(struct input_event)) event(ev);
                    }
                }
                SDL_UnlockMutex(reins);
            }
            closefds();
            return 0;
        }
        void stop()
        {
            if(!reins) // nothing to release... probably
            {
                if(thread) // this shouldn't happen...
                {
                    conoutf(CON_ERROR, "linux raw input killing unexpected survivor thread");
                    SDL_KillThread(thread);
                    thread = NULL;
                }
            }
            else
            {
                // take reins and indicate thread should cease
                SDL_LockMutex(reins);
                cease = true;
                SDL_UnlockMutex(reins);
                // wait for thread to finish
                SDL_WaitThread(thread, NULL);
                thread = NULL;
                SDL_DestroyMutex(reins);
                reins = NULL;
            }
            closefds(); // should've been done by thread, but just in case
        }
        void start()
        {
            if(fds.empty()) return; // don't start a useless thread
            cease = false;
            reins = SDL_CreateMutex();
            if(!reins) return;
            thread = SDL_CreateThread(worker, NULL);
        }
    }
    void searchdevs(bool (*handle)(int, const char *))
    {
        string path;
        string name;
        loopi(32)
        {
            formatstring(path)("/dev/input/event%d", i);
            int fd = open(path, O_RDONLY);
            if(fd < 0)
            {
                if(ENOENT == errno) break; // if event{n} doesn't exist, assume event{n+k} doesn't either.
                else continue; // otherwise try next device.
            }
            // get name
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            // get supported features
            uint features = 0L;
            ioctl(fd, EVIOCGBIT(0, sizeof(features)), &features);
            if(!(features & 1 << EV_REL)) close(fd); // ignore non-mice
            else if(handle(fd, name)) break;
        }
    }
    bool listdevice_callback(int fd, const char *name)
    {
        close(fd);
        listdevice(name);
        return false;
    }
    void listdevices()
    {
        devicenames.setsize(0);
        searchdevs(listdevice_callback);
        devicenames.add('\0');
        result(devicenames.getbuf());
    }
    bool usedev(int fd, const char *name)
    {
        if(*rawmouse && strstr(name, rawmouse) == NULL)
        {
            close(fd);
        }
        else
        {
            conoutf(CON_DEBUG, "Listening to raw device %s", name);
            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = thread::readflags;
            thread::fds.add(pfd);
        }
        return false;
    }
    int os_pick(const char *dev)
    {
        thread::stop();
        searchdevs(usedev);
        int count = thread::fds.length();
        thread::start();
        return count;
    }
    void os_release()
    {
        thread::stop();
    }
#else
    int os_pick(const char *dev)
    {
        return -1;
    }
    void os_release()
    {
    }
#endif
}
